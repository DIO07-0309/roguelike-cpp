// G8.3: MCTS search implementation
#include "ai/mcts/mcts_search.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace mcts {

// ── Combat simulation (simplified damage model) ─────────────
namespace detail {

static float dist(const PlayerSnapshot& p, const MonsterSnapshot& m) {
    return std::abs(p.x - m.x) + std::abs(p.y - m.y);
}

static int calc_damage(int atk, int def) {
    int dmg = atk - (int)(def * 0.5f);
    return std::max(1, dmg);
}

static void apply_action(SimulationState& state, CombatAction a, uint32_t& rng_seed) {
    auto& p = state.player;
    if (!p.alive) return;

    auto lcg = [&]() -> float {
        rng_seed = rng_seed * 1664525u + 1013904223u;
        return (float)(rng_seed & 0x7FFFFFFF) / 2147483648.0f;
    };

    switch (a) {
    case CombatAction::ATTACK: {
        // Hit nearest monster
        MonsterSnapshot* best = nullptr; float bd = 999;
        for (auto& m : state.monsters) {
            if (!m.alive) continue;
            float d = dist(p, m);
            if (d < 1.5f && d < bd) { bd = d; best = &m; }
        }
        if (best) {
            int dmg = calc_damage(p.attack, best->pdef);
            dmg = (int)(dmg * (0.8f + 0.4f * lcg())); // ±20% swing
            best->hp -= (float)dmg;
            if (best->hp <= 0) best->alive = false;
        }
        p.attack_cooldown = 0.5f;
        break;
    }
    case CombatAction::SKILL_1:
    case CombatAction::SKILL_2:
    case CombatAction::SKILL_3:
    case CombatAction::SKILL_4: {
        int si = (int)a - (int)CombatAction::SKILL_1;
        int dmg = (int)(p.attack * 1.5f); // skill multiplier
        // Hit nearest monster
        MonsterSnapshot* best = nullptr; float bd = 999;
        for (auto& m : state.monsters) {
            if (!m.alive) continue;
            float d = dist(p, m);
            if (d < 3.0f && d < bd) { bd = d; best = &m; }
        }
        if (best) {
            dmg = (int)(dmg * (0.8f + 0.4f * lcg()));
            best->hp -= (float)dmg;
            if (best->hp <= 0) best->alive = false;
        }
        p.skill_cooldowns[si] = 3.0f;
        break;
    }
    case CombatAction::MOVE_UP:    p.y -= 1; break;
    case CombatAction::MOVE_DOWN:  p.y += 1; break;
    case CombatAction::MOVE_LEFT:  p.x -= 1; break;
    case CombatAction::MOVE_RIGHT: p.x += 1; break;
    default: break; // WAIT
    }

    // Decay cooldowns
    if (p.attack_cooldown > 0) p.attack_cooldown -= 0.25f;
    for (int i = 0; i < 4; i++)
        if (p.skill_cooldowns[i] > 0) p.skill_cooldowns[i] -= 0.25f;

    // Monster retaliates (simplified: nearest hits player)
    for (auto& m : state.monsters) {
        if (!m.alive) continue;
        float d = dist(p, m);
        if (d < 1.5f && lcg() < 0.4f) { // ~40% chance per monster per tick
            int dmg = calc_damage(m.attack, p.pdef);
            p.hp -= (float)dmg;
            if (p.hp <= 0) { p.alive = false; state.terminal = true; }
        }
    }

    state.depth++;
    // Check victory
    bool any_alive = false;
    for (auto& m : state.monsters) if (m.alive) { any_alive = true; break; }
    if (!any_alive) { state.victory = true; state.terminal = true; }
}

static double evaluate_terminal(const SimulationState& state) {
    if (state.victory) return 1000.0;
    if (!state.player.alive) return -1000.0;
    // Heuristic: HP remaining + enemy deaths
    double score = state.player.hp * 2.0;
    for (auto& m : state.monsters) {
        if (!m.alive) score += 200.0;
        else score -= m.hp * 1.5;
    }
    // Depth penalty:
    score -= state.depth * 5.0;
    return score;
}

} // namespace detail

// ═══════════════════════════════════════════════════════════
//  MCTS public API
// ═══════════════════════════════════════════════════════════

CombatAction MCTS::search(const SimulationState& state) {
    // Build root
    auto root = std::make_unique<MCTSNode>();
    root->state = state.clone();

    for (int i = 0; i < _iterations; i++) {
        // 1. Selection
        MCTSNode* node = select(root.get());
        // 2. Expansion
        if (!node->is_terminal() && !node->is_fully_expanded()) {
            expand(node);
            if (!node->children.empty())
                node = node->children.back().get();
        }
        // 3. Simulation
        double reward = simulate(node->state, state.rng.next() + i * 7919u);
        // 4. Backpropagation
        backpropagate(node, reward);
    }

    // Pick best child by visit count
    MCTSNode* best = nullptr;
    int best_visits = -1;
    for (auto& c : root->children) {
        if (c->visits > best_visits) {
            best_visits = c->visits;
            best = c.get();
        }
    }
    if (best) return best->action;

    // Fallback: any possible action
    auto actions = get_possible_actions(state);
    return actions.empty() ? CombatAction::WAIT : actions[0];
}

MCTSNode* MCTS::select(MCTSNode* node) {
    while (!node->is_terminal()) {
        if (!node->is_fully_expanded()) return node;
        // UCT selection
        MCTSNode* best = nullptr;
        double best_uct = -1e9;
        for (auto& c : node->children) {
            double uct = c->uct_value();
            if (uct > best_uct) { best_uct = uct; best = c.get(); }
        }
        if (!best) return node;
        node = best;
    }
    return node;
}

void MCTS::expand(MCTSNode* node) {
    auto actions = get_possible_actions(node->state);
    for (auto a : actions) {
        auto child = std::make_unique<MCTSNode>();
        child->action = a;
        child->parent = node;
        child->state = node->state.clone();
        uint32_t seed = child->state.rng.next() + node->children.size() * 7331u;
        detail::apply_action(child->state, a, seed);
        node->children.push_back(std::move(child));
    }
}

double MCTS::simulate(const SimulationState& state, uint32_t rng_seed) {
    SimulationState sim = state.clone();
    int max_depth = 10;
    uint32_t rng = rng_seed;
    while (!sim.is_terminal() && sim.depth < max_depth) {
        auto actions = get_possible_actions(sim);
        if (actions.empty()) break;
        CombatAction a = actions[(rng + sim.depth) % actions.size()];
        detail::apply_action(sim, a, rng);
    }
    return detail::evaluate_terminal(sim);
}

void MCTS::backpropagate(MCTSNode* leaf, double reward) {
    MCTSNode* node = leaf;
    while (node) {
        node->visits++;
        node->reward += reward;
        reward *= 0.95; // discount toward root
        node = node->parent;
    }
}

} // namespace mcts
