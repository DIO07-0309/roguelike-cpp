#include "sim_ai.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"  // rng
#include "build_score.h"    // BuildType, calculate_build
#include "ai/mcts/mcts_search.h"
#include "ai/mcts/action.h"
#include <cmath>
#include <cstring>
#include <algorithm>

bool DecisionAgent::g_use_mcts = false;
int  DecisionAgent::g_mcts_iters = 100;

// G8.3: Build SimulationState snapshot from live game state
mcts::SimulationState DecisionAgent::build_sim_state(
    const Player* player, const std::vector<Monster*>& monsters) {
    mcts::SimulationState s;
    if (!player) return s;
    auto& p = s.player;
    p.hp = (float)player->combat.current_hp;
    p.max_hp = (float)player->combat.max_hp;
    p.x = player->entity.rect.x / 32.0f;
    p.y = player->entity.rect.y / 32.0f;
    p.attack = player->combat.attack;
    p.pdef = player->combat.physical_defense;
    p.mdef = player->combat.magical_defense;
    p.alive = player->combat.is_alive;
    // Cooldowns from player's last_attack_time vs game_time
    float gt = 0; // MCTS operates in abstract time
    p.attack_cooldown = std::max(0.0f, 0.5f);
    for (int i = 0; i < 4; i++)
        p.skill_cooldowns[i] = (i < (int)player->skills.active_skills.size()) ? 0.0f : 99.0f;
    // Buffs
    for (auto& b : player->active_buffs)
        if (b.stacks > 0)
            p.buffs.push_back({b.id, b.stacks, b.remaining});

    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        mcts::MonsterSnapshot ms;
        ms.type = m->name;
        ms.hp = (float)m->combat.current_hp;
        ms.max_hp = (float)m->combat.max_hp;
        ms.x = m->entity.rect.x / 32.0f;
        ms.y = m->entity.rect.y / 32.0f;
        ms.attack = m->combat.attack;
        ms.pdef = m->combat.physical_defense;
        ms.mdef = m->combat.magical_defense;
        ms.alive = true;
        ms.is_boss = m->is_boss;
        s.monsters.push_back(ms);
    }
    s.rng.seed = (uint32_t)(int)(player->entity.rect.x * 1000 + player->entity.rect.y);
    return s;
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Build-aware behavioral profiles
// ═══════════════════════════════════════════════════════════

DecisionAgent::DecisionAgent() {}

void DecisionAgent::start(const Player* player) {
    _frame = 0; _dir_timer = 0; _current_dir = -1;
    _resolve_profile(player);
}

void DecisionAgent::_resolve_profile(const Player* player) {
    if (!player) return;
    _build_type = calculate_build(player).identify();

    // Default: balanced
    _prefer_range = 0.3f; _prefer_aoe = 0.2f;
    _prefer_skill = 0.4f; _aggro_bias = 0.5f;
    _prefer_heal = 0.35f;
    _skill_priority[0]=0; _skill_priority[1]=1;
    _skill_priority[2]=2; _skill_priority[3]=3;

    switch (_build_type) {
    case BuildType::ICE_MAGE:
        _prefer_range = 0.9f; _prefer_aoe = 0.8f;
        _prefer_skill = 0.7f; _aggro_bias = 0.2f; // kite
        break;
    case BuildType::FIRE_MAGE:
        _prefer_range = 0.7f; _prefer_aoe = 0.7f;
        _prefer_skill = 0.8f; _aggro_bias = 0.3f;
        break;
    case BuildType::LIGHTNING_MAGE:
        _prefer_range = 0.6f; _prefer_aoe = 0.6f;
        _prefer_skill = 0.7f; _aggro_bias = 0.4f;
        break;
    case BuildType::BERSERKER:
        _prefer_range = 0.0f; _prefer_aoe = 0.3f;
        _prefer_skill = 0.3f; _aggro_bias = 0.9f; // rush in
        break;
    case BuildType::BLEED_BLADE:
        _prefer_range = 0.1f; _prefer_aoe = 0.3f;
        _prefer_skill = 0.5f; _aggro_bias = 0.7f;
        break;
    case BuildType::SHADOW_STRIKER:
        _prefer_range = 0.0f; _prefer_aoe = 0.0f;
        _prefer_skill = 0.6f; _aggro_bias = 0.6f; // single target burst
        break;
    case BuildType::SUPPORT:
        _prefer_range = 0.4f; _aggro_bias = 0.3f;
        _prefer_heal = 0.50f; // heal early
        break;
    case BuildType::JUGGERNAUT:
        _prefer_range = 0.0f; _prefer_aoe = 0.4f;
        _prefer_skill = 0.2f; _aggro_bias = 0.8f; // tank
        break;
    case BuildType::SUMMON_LORD:
        _prefer_range = 0.6f; _prefer_skill = 0.8f;
        _aggro_bias = 0.2f; // stay back, let summons fight
        break;
    case BuildType::POISON_MASTER:
        _prefer_range = 0.5f; _prefer_aoe = 0.4f;
        _prefer_skill = 0.5f; _aggro_bias = 0.4f;
        break;
    case BuildType::TIME_MASTER:
        _prefer_range = 0.5f; _prefer_skill = 0.7f;
        _aggro_bias = 0.3f;
        break;
    default: break; // keep defaults
    }
}

void DecisionAgent::tick() { _frame++; }

// ═══════════════════════════════════════════════════════════
//  G7.4: Action evaluators
// ═══════════════════════════════════════════════════════════

float DecisionAgent::_evaluate_attack(const Player* p,
    const std::vector<Monster*>& monsters) const {
    auto* t = _find_nearest(p, monsters);
    if (!t) return 0;
    float d = hypotf(t->entity.rect.x - p->entity.rect.x,
                     t->entity.rect.y - p->entity.rect.y);
    if (d > 2.0f * 32.0f) return 0; // out of range — no score
    // Melee builds score higher for attacking
    float base = 1.0f - _prefer_range; // range=0 → score 1.0
    return base * (1.0f - d / (3.0f * 32.0f)); // closer = better
}

float DecisionAgent::_evaluate_skill(int slot, const Player* p,
    const std::vector<Monster*>& monsters) const {
    if (slot < 0 || slot >= 4) return 0;
    int n = _count_in_range(p, monsters, 5.0f * 32.0f);
    if (n <= 0) return 0;
    float aoe_bonus = _prefer_aoe * (n > 1 ? 1.0f : 0.3f);
    return _prefer_skill * (0.5f + aoe_bonus);
}

float DecisionAgent::_evaluate_move(int dir, const Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map) const {
    auto* t = _find_nearest(p, monsters);
    float px = p->entity.rect.x / 32.0f;
    float py = p->entity.rect.y / 32.0f;

    // Direction offsets (0=up,1=down,2=left,3=right)
    float dx = 0, dy = 0;
    if (dir == 0) dy = -1; else if (dir == 1) dy = 1;
    else if (dir == 2) dx = -1; else if (dir == 3) dx = 1;

    // Check walkable
    int tx = (int)(px + dx), ty = (int)(py + dy);
    if (map && !map->is_walkable(tx, ty)) return -999; // blocked

    if (!t) return 0.1f; // no enemies → neutral

    float ex = t->entity.rect.x / 32.0f;
    float ey = t->entity.rect.y / 32.0f;
    float cur_dist = hypotf(ex - px, ey - py);
    float new_dist = hypotf(ex - (px + dx), ey - (py + dy));

    // Ranged builds: move AWAY from enemies. Melee: move TOWARD.
    float ideal_dist = 1.5f + _prefer_range * 4.0f; // melee=1.5, range=5.5
    float cur_diff = fabsf(cur_dist - ideal_dist);
    float new_diff = fabsf(new_dist - ideal_dist);

    return (cur_diff - new_diff) * _aggro_bias * 2.0f; // positive = improvement
}

float DecisionAgent::_evaluate_pickup(const Player* p, const GameMap* map) const {
    if (!map) return 0;
    for (auto& sr : map->special_rooms) {
        float dx = p->entity.rect.x + p->entity.rect.width/2 - (sr.cx * 32 + 16);
        float dy = p->entity.rect.y + p->entity.rect.height/2 - (sr.cy * 32 + 16);
        if (sqrtf(dx*dx+dy*dy) < 3.0f * 32.0f)
            return 1.5f; // always high priority near room
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Best action selection (evaluate → pick max)
// ═══════════════════════════════════════════════════════════

std::string DecisionAgent::best_action(const Player* player,
    const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return "";

    if (boss_intro_active) return "confirm";
    if (stairs_active)      return "descend";

    // ── G8.3: MCTS path (combat-only, enemies present) ──
    if (g_use_mcts && !monsters.empty()) {
        auto sim = build_sim_state(player, monsters);
        if (sim.alive_monsters() > 0) {
            MCTS mcts(g_mcts_iters);
            CombatAction ca = mcts.search(sim);
            return action_name(ca);
        }
    }

    float best_score = 0;
    std::string best = "";

    // Attack
    float atk_score = _evaluate_attack(player, monsters);
    if (atk_score > best_score) { best_score = atk_score; best = "attack"; }

    // Skills (priority-ordered)
    for (int si = 0; si < 4; si++) {
        int slot = _skill_priority[si];
        float sk_score = _evaluate_skill(slot, player, monsters);
        if (sk_score > best_score) {
            best_score = sk_score;
            best = "skill_" + std::to_string(slot + 1);
        }
    }

    // Heal decision (G7.4: HP below threshold → prioritize heal)
    if (_hp_ratio(player) < _prefer_heal) {
        for (int si = 0; si < 4; si++) {
            int slot = _skill_priority[si];
            // SelfHealSkills map to skill index — approximate by slot
            // skill_3 is often heal slot
            if (slot == 2 && _evaluate_skill(slot, player, monsters) > 0.1f) {
                best_score = 3.0f; // override other actions
                best = "skill_3";
            }
        }
    }

    // Pickup
    float pu_score = _evaluate_pickup(player, map);
    if (pu_score > best_score) { best_score = pu_score; best = "pickup"; }

    // Movement (pick best direction)
    float move_scores[4];
    for (int d = 0; d < 4; d++)
        move_scores[d] = _evaluate_move(d, player, monsters, map);
    int best_dir = 0;
    for (int d = 1; d < 4; d++)
        if (move_scores[d] > move_scores[best_dir]) best_dir = d;
    if (move_scores[best_dir] > best_score) {
        const char* dirs[] = {"move_up","move_down","move_left","move_right"};
        best = dirs[best_dir];
    }

    // If nothing better — move randomly
    if (best.empty()) {
        const char* rand_dirs[] = {"move_up","move_down","move_left","move_right"};
        best = rand_dirs[rng() % 4];
    }

    return best;
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Event decision
// ═══════════════════════════════════════════════════════════

bool DecisionAgent::accept_event(float risk_pct, const std::string& effect_desc,
                                  const Player* player) const {
    if (!player) return false;
    float hp = _hp_ratio(player);

    // Never suicide
    if (risk_pct > 0.40f && hp < 0.50f) return false;
    if (risk_pct > 0.25f && hp < 0.30f) return false;
    if (risk_pct > 0.10f && hp < 0.15f) return false;

    // High-value effects worth risking for
    bool high_value = (effect_desc.find("relic") != std::string::npos) ||
                      (effect_desc.find("skill_level") != std::string::npos) ||
                      (effect_desc.find("legendary") != std::string::npos);

    if (high_value && hp > 0.60f) return true;
    if (risk_pct == 0) return true;  // no risk → always accept

    // Moderate risk: accept if HP > risk*2 + buffer
    return hp > risk_pct * 2.0f + 0.25f;
}

// ═══════════════════════════════════════════════════════════
//  Backward compat: is_action_just_pressed gate
// ═══════════════════════════════════════════════════════════

bool DecisionAgent::is_action_just_pressed(const char* action_name,
    const Player* player, const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return false;

    std::string best = best_action(player, monsters, map, stairs_active, boss_intro_active);
    if (best.empty()) return false;
    return best == action_name;
}

// ═══════════════════════════════════════════════════════════
//  Helpers (unchanged from G5.6)
// ═══════════════════════════════════════════════════════════

Monster* DecisionAgent::_find_nearest(const Player* player,
    const std::vector<Monster*>& monsters) const {
    Monster* best = nullptr; float bd = 99999;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float d = hypotf(m->entity.rect.x + m->entity.rect.width/2 - px,
                         m->entity.rect.y + m->entity.rect.height/2 - py);
        if (d < bd) { bd = d; best = m; }
    }
    return best;
}

int DecisionAgent::_count_in_range(const Player* player,
    const std::vector<Monster*>& monsters, float range_px) const {
    int n = 0;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float d = hypotf(m->entity.rect.x - px, m->entity.rect.y - py);
        if (d < range_px) n++;
    }
    return n;
}

float DecisionAgent::_hp_ratio(const Player* p) const {
    if (!p || p->combat.max_hp <= 0) return 0;
    return (float)p->combat.current_hp / (float)p->combat.max_hp;
}

void DecisionAgent::_pick_direction(const Player* player,
    const std::vector<Monster*>& monsters) {
    Monster* t = _find_nearest(player, monsters);
    if (t) {
        float dx = t->entity.rect.x + t->entity.rect.width/2 -
                   (player->entity.rect.x + player->entity.rect.width/2);
        float dy = t->entity.rect.y + t->entity.rect.height/2 -
                   (player->entity.rect.y + player->entity.rect.height/2);
        if (fabsf(dx) > fabsf(dy))
            _current_dir = (dx > 0) ? 3 : 2;
        else
            _current_dir = (dy > 0) ? 1 : 0;
    } else {
        _current_dir = rng() % 4;
    }
}
