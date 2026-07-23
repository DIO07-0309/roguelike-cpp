#include "ai/agents/bt_agent.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"
#include "build_score.h"
#include <cmath>

BTAgent::BTAgent()  {}
BTAgent::~BTAgent() {}

// ═══════════════════════════════════════════════════════════
//  Blackboard sync: game state → key-value pairs
// ═══════════════════════════════════════════════════════════

void BTAgent::_sync_state(const Player* player,
    const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return;

    _board.set("hp", (float)player->combat.current_hp);
    _board.set("max_hp", (float)player->combat.max_hp);
    _board.set("hp_ratio", _hp_ratio(player));

    int enemy_count = (int)monsters.size();
    int alive = 0;
    float nearest_dist = 9999;
    std::string nearest_id = "none";
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        alive++;
        float d = hypotf(m->entity.rect.x - player->entity.rect.x,
                         m->entity.rect.y - player->entity.rect.y);
        if (d < nearest_dist) { nearest_dist = d; nearest_id = m->name; }
    }
    _board.set("enemy_count", alive);
    _board.set("enemy_near", nearest_dist < 3.0f * 32.0f);
    _board.set("nearest_dist", nearest_dist);
    _board.set("enemies_in_aoe", _count_enemies_in_range(player, monsters, 4.0f * 32.0f));

    // Boss proximity
    bool boss_near = false;
    for (auto* m : monsters)
        if (m && m->is_boss && m->combat.is_alive) {
            float d = hypotf(m->entity.rect.x - player->entity.rect.x,
                             m->entity.rect.y - player->entity.rect.y);
            if (d < 5.0f * 32.0f) { boss_near = true; break; }
        }
    _board.set("boss_near", boss_near);
    _board.set("stairs_active", stairs_active);
    _board.set("boss_intro", boss_intro_active);

    // Skills
    bool any_ready = false;
    for (int i = 0; i < 4; i++)
        if (i < (int)player->skills.active_skills.size() && player->skills.active_skills[i]->can_use(0)) { any_ready = true; break; }
    _board.set("skill_ready", any_ready);

    // Heal slot available
    // Heal skills are typically index 2 (self_heal). Check if ready.
    _board.set("heal_ready", player->skills.active_skills.size() > 2);

    // Near special room
    bool near_room = false;
    if (map) {
        for (auto& sr : map->special_rooms) {
            float dx = player->entity.rect.x + player->entity.rect.width/2 - (sr.cx * 32 + 16);
            float dy = player->entity.rect.y + player->entity.rect.height/2 - (sr.cy * 32 + 16);
            if (sqrtf(dx*dx+dy*dy) < 3.0f * 32.0f) { near_room = true; break; }
        }
    }
    _board.set("near_room", near_room);

    // Build identity
    _board.set("prefer_range", _board.get("prefer_range", 0.3f));
    _board.set("prefer_aoe", _board.get("prefer_aoe", 0.2f));
}

// ═══════════════════════════════════════════════════════════
//  Tree builder
// ═══════════════════════════════════════════════════════════

void BTAgent::build_tree() {
    using namespace bt;

    // Conditions (predicates on Blackboard)
    auto bossIntro = std::make_unique<Condition>("BossIntro",
        [](Blackboard& b) { return b.get("boss_intro", false); });
    auto stairsActive = std::make_unique<Condition>("StairsActive",
        [](Blackboard& b) { return b.get("stairs_active", false); });
    auto hpLow = std::make_unique<Condition>("HPLow",
        [](Blackboard& b) { return b.get("hp_ratio", 1.0f) < 0.35f; });
    auto healReady = std::make_unique<Condition>("HealReady",
        [](Blackboard& b) { return b.get("heal_ready", false); });
    auto enemyNear = std::make_unique<Condition>("EnemyNear",
        [](Blackboard& b) { return b.get("enemy_near", false); });
    auto bossNear = std::make_unique<Condition>("BossNear",
        [](Blackboard& b) { return b.get("boss_near", false); });
    auto skillReady = std::make_unique<Condition>("SkillReady",
        [](Blackboard& b) { return b.get("skill_ready", false); });
    auto enemiesInAoE = std::make_unique<Condition>("EnemiesInAoE",
        [](Blackboard& b) { return b.get("enemies_in_aoe", 0) >= 2; });
    auto nearRoom = std::make_unique<Condition>("NearRoom",
        [](Blackboard& b) { return b.get("near_room", false); });

    // Actions (write action string into Blackboard)
    auto actConfirm  = std::make_unique<Action>("Confirm",
        [](Blackboard& b) { b.set<std::string>("action", "confirm"); });
    auto actDescend  = std::make_unique<Action>("Descend",
        [](Blackboard& b) { b.set<std::string>("action", "descend"); });
    auto actHeal     = std::make_unique<Action>("Heal",
        [](Blackboard& b) { b.set<std::string>("action", "skill_3"); });
    auto actAttack   = std::make_unique<Action>("Attack",
        [](Blackboard& b) { b.set<std::string>("action", "attack"); });
    auto actSkill    = std::make_unique<Action>("Skill",
        [](Blackboard& b) { b.set<std::string>("action", "skill_1"); });
    auto actAoE      = std::make_unique<Action>("AoESkill",
        [](Blackboard& b) { b.set<std::string>("action", "skill_2"); });
    auto actPickup   = std::make_unique<Action>("Pickup",
        [](Blackboard& b) { b.set<std::string>("action", "pickup"); });
    auto actWander   = std::make_unique<Action>("Wander",
        [](Blackboard& b) { b.set<std::string>("action", "move_up"); }); // default wander

    // Sub-trees
    // Heal: HP low AND heal ready → use heal
    auto healSeq = std::make_unique<Sequence>(std::vector<std::unique_ptr<Node>>{});
    // Need to build via alternative: Sequence takes vector in constructor after we build it

    std::vector<std::unique_ptr<Node>> heal_children;
    heal_children.push_back(hpLow->clone());
    heal_children.push_back(healReady->clone());
    heal_children.push_back(actHeal->clone());
    auto healST = std::make_unique<Sequence>(std::move(heal_children));

    // Boss fight: boss nearby → attack
    std::vector<std::unique_ptr<Node>> boss_children;
    boss_children.push_back(bossNear->clone());
    boss_children.push_back(actAttack->clone());
    auto bossST = std::make_unique<Sequence>(std::move(boss_children));

    // AoE skill: multiple enemies + skill ready
    std::vector<std::unique_ptr<Node>> aoe_children;
    aoe_children.push_back(enemiesInAoE->clone());
    aoe_children.push_back(skillReady->clone());
    aoe_children.push_back(actAoE->clone());
    auto aoeST = std::make_unique<Sequence>(std::move(aoe_children));

    // Combat: enemy near → attack or skill
    std::vector<std::unique_ptr<Node>> combat_children;
    combat_children.push_back(enemyNear->clone());
    combat_children.push_back(actAttack->clone());
    auto combatST = std::make_unique<Sequence>(std::move(combat_children));

    // Pickup
    std::vector<std::unique_ptr<Node>> pu_children;
    pu_children.push_back(nearRoom->clone());
    pu_children.push_back(actPickup->clone());
    auto pickupST = std::make_unique<Sequence>(std::move(pu_children));

    // Root: Selector (priority order)
    std::vector<std::unique_ptr<Node>> root_children;
    root_children.push_back(bossIntro->clone());   // 1. Boss intro → confirm
    root_children.push_back(stairsActive->clone()); // 2. Stairs → descend
    root_children.push_back(std::move(healST));     // 3. Heal if low
    root_children.push_back(std::move(bossST));     // 4. Boss fight
    root_children.push_back(std::move(aoeST));      // 5. AoE if multi-target
    root_children.push_back(std::move(combatST));   // 6. Attack nearest
    root_children.push_back(std::move(pickupST));   // 7. Pickup near room
    root_children.push_back(actWander->clone());    // 8. Wander (always succeeds)

    // Root: Selector (priority order)
    _root = std::make_unique<Selector>(std::move(root_children));
}

// ═══════════════════════════════════════════════════════════
//  Interface: same as DecisionAgent
// ═══════════════════════════════════════════════════════════

std::string BTAgent::best_action(const Player* player,
    const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player || !_root) return "move_up";

    _sync_state(player, monsters, map, stairs_active, boss_intro_active);
    _board.set<std::string>("action", "move_up"); // default

    _root->tick(_board);
    return _board.get<std::string>("action", "move_up");
}

bool BTAgent::is_action_just_pressed(const char* action_name,
    const Player* player, const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return false;
    std::string best = best_action(player, monsters, map, stairs_active, boss_intro_active);
    return best == action_name;
}

bool BTAgent::accept_event(float risk_pct, const std::string& effect_desc,
                            const Player* player) const {
    if (!player) return false;
    float hp = _hp_ratio(player);
    if (risk_pct > 0.40f && hp < 0.50f) return false;
    if (risk_pct > 0.25f && hp < 0.30f) return false;
    if (risk_pct > 0.10f && hp < 0.15f) return false;
    bool high_value = (effect_desc.find("relic") != std::string::npos) ||
                      (effect_desc.find("skill") != std::string::npos) ||
                      (effect_desc.find("legendary") != std::string::npos);
    if (high_value && hp > 0.60f) return true;
    if (risk_pct == 0) return true;
    return hp > risk_pct * 2.0f + 0.25f;
}

// ═══════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════

void BTAgent::_resolve_profile(const Player* player) {
    auto bt = calculate_build(player).identify();
    float range = 0.3f, aoe = 0.2f;
    switch (bt) {
    case BuildType::ICE_MAGE:       range=0.9f; aoe=0.8f; break;
    case BuildType::FIRE_MAGE:      range=0.7f; aoe=0.7f; break;
    case BuildType::LIGHTNING_MAGE: range=0.6f; aoe=0.6f; break;
    case BuildType::BERSERKER:      range=0.0f; aoe=0.3f; break;
    case BuildType::SUMMON_LORD:    range=0.6f;         break;
    case BuildType::JUGGERNAUT:     range=0.0f; aoe=0.4f; break;
    default: break;
    }
    _board.set("prefer_range", range);
    _board.set("prefer_aoe", aoe);
}

Monster* BTAgent::_find_nearest(const Player* player,
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

float BTAgent::_hp_ratio(const Player* p) const {
    if (!p || p->combat.max_hp <= 0) return 0;
    return (float)p->combat.current_hp / (float)p->combat.max_hp;
}

int BTAgent::_count_enemies_in_range(const Player* player,
    const std::vector<Monster*>& monsters, float range_px) const {
    int n = 0;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        if (hypotf(m->entity.rect.x - px, m->entity.rect.y - py) < range_px) n++;
    }
    return n;
}
