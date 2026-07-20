#include "boss_system_director.h"
#include "event_bus.h"
#include "monster.h"
#include "player.h"
#include "game_map.h"
#include "world_state.h"
#include "relationship_system.h"
#include "boss.h"              // BossAI, spawn_boss
#include "combat_system.h"     // rng

// ============================================================
// D6 Step3: BossSystemDirector — 组合所有Boss子系统
// ============================================================

void BossSystemDirector::reset() {
    evolution = BossEvolutionData{};
    behavior = BossBehaviorState{};
    skill_queue = BossSkillQueue{};
    arena.clear();
    encounter = BossEncounterController{};
    replay_mem = BossReplayMemory{};
    battle_report = BossBattleReport{};
    cinematic = BossCinematicController{};
    timeline.clear();
    current_cmd = BossCommand::NONE;
    modifier_hook = BossModifierHook{};
    intro_text.clear();
    modifier_text.clear();
    dmg_done = 0; dmg_taken = 0;
    _arena_spawn_timer = 0.0f;   // G2.3
    _arena_cfg = nullptr;        // G2.3
}

void BossSystemDirector::init_on_spawn(Monster* boss, int floor,
    const WorldState& ws, BuildType bt, const RelationshipSystem& rels,
    GameMap* map) {
    if (!boss) return;

    // D5 Step1: BossModifier
    modifier_hook = BossNarrative::calc_modifier(floor, ws, bt, rels);
    if (modifier_hook.hp_scale != 1.0f) {
        boss->combat.max_hp = (int)(boss->combat.max_hp * modifier_hook.hp_scale);
        boss->combat.current_hp = boss->combat.max_hp;
    }
    if (modifier_hook.damage_scale != 1.0f)
        boss->combat.attack = (int)(boss->combat.attack * modifier_hook.damage_scale);

    // D5 Step2: Evolution
    evolution = calc_boss_evolution(floor, ws, bt, rels);
    float skill_dmg_avg = (evolution.charge_mod.damage_scale
                         + evolution.shockwave_mod.damage_scale
                         + evolution.summon_mod.damage_scale) / 3.0f;
    if (skill_dmg_avg > 1.0f || skill_dmg_avg < 1.0f)
        boss->combat.attack = (int)(boss->combat.attack * skill_dmg_avg);

    if (modifier_hook.disable_phase) {
        auto* bai = dynamic_cast<BossAI*>(boss->ai);
        if (bai) bai->is_enraged = true;
    }
    if (modifier_hook.add_minions && map) {
        if (rng() % 100 < 80) {
            auto [etx, ety] = map->pixel_to_tile(
                boss->entity.position.x + 32, boss->entity.position.y);
            if (map->is_walkable(etx, ety)) {
                // Note: caller must add to monsters vector — we just signal via the hook
                // The actual add_minions is handled in game_scene (monsters.emplace_back)
                modifier_hook.add_minions = true;  // flag for caller
            } else modifier_hook.add_minions = false;
        } else modifier_hook.add_minions = false;
    }

    modifier_text = modifier_hook.modifier_text ? modifier_hook.modifier_text : "";

    // D5 Step3: Behavior
    behavior = BossBehaviorState{};
    behavior.personality = boss_personality_for_floor(floor);

    // D5 Step5: Encounter
    encounter.start(floor);

    // D5 Step6: Cinematic + Timeline
    cinematic.start_intro();
    timeline.clear();
    timeline.record(0, "INTRO");

    // Replay memory
    replay_mem = BossReplayMemory{};
    replay_mem.build = bt;
    replay_mem.blood_ritual = ws.has(WorldFlag::Blood_Ritual);
    replay_mem.curse = ws.has(WorldFlag::Accepted_Curse);
    replay_mem.saved_priest = ws.has(WorldFlag::Saved_Priest);

    dmg_done = 0; dmg_taken = 0;

    // G2.3: Arena 配置从 BossAI 读取 (boss_factory_create 已设置)
    if (auto* bai = dynamic_cast<BossAI*>(boss->ai))
        _arena_cfg = bai->_arena_cfg;
    else
        _arena_cfg = nullptr;

    // D5 Step2: Apply cooldown/area modifiers to BossAI
    if (auto* bai = dynamic_cast<BossAI*>(boss->ai)) {
        bai->_charge->cooldown    *= evolution.charge_mod.cooldown_scale;
        bai->_shockwave->cooldown *= evolution.shockwave_mod.cooldown_scale;
        bai->_summon->cooldown    *= evolution.summon_mod.cooldown_scale;
        bai->_charge->fx_radius   *= evolution.charge_mod.area_scale
                                      + evolution.charge_mod.damage_scale - 1.0f;
        bai->_shockwave->fx_radius *= evolution.shockwave_mod.area_scale;
        if (evolution.summon_mod.double_strike)
            bai->_summon->cooldown *= 0.5f;
    }

    // Build combined modifier text for intro
    if (!modifier_text.empty() && evolution.evolution_name) {
        modifier_text = modifier_text + " [" + evolution.evolution_name + "]";
    }
}

void BossSystemDirector::tick(float dt, Monster* boss, const Player* player,
    int floor, const WorldState& ws, const RelationshipSystem& rels) {
    if (!boss || !boss->is_boss) return;

    // Build context for behavior evaluation
    BossContext ctx;
    ctx.hp_pct = (float)boss->combat.current_hp / boss->combat.max_hp;
    ctx.dist_tiles = hypotf(
        boss->entity.rect.x - player->entity.rect.x,
        boss->entity.rect.y - player->entity.rect.y) / 32.0f;
    ctx.player_low_hp    = player->combat.current_hp < player->combat.max_hp * 0.3f;
    ctx.player_combo_high = player->combo.count >= 4;
    ctx.player_far       = ctx.dist_tiles > 7;
    ctx.player_near      = ctx.dist_tiles < 3;
    ctx.last_stand       = evolution.last_stand_triggered;
    ctx.build = replay_mem.build;
    ctx.stage = StoryStage::CHAPTER_1; // simplified — real value from StoryDirector

    behavior.memory.tick(dt, false);
    evaluate_boss_decision(floor, ctx, ws, rels, behavior, dt);

    // Behavior→Command
    current_cmd = boss_decision_to_command((int)behavior.current);

    // Encounter tick
    encounter.tick(dt, ctx.hp_pct);

    // Cinematic tick
    cinematic.tick(dt);

    // ── G2.3: Arena 生成 (原在 GameScene 中的硬编码) ──
    if (_arena_cfg && _arena_cfg->danger_type != "none") {
        _arena_spawn_timer += dt;
        if (_arena_spawn_timer >= _arena_cfg->spawn_interval) {
            _arena_spawn_timer = 0.0f;
            ArenaEvent ev;
            ev.type     = ArenaEventType::SPAWN_ZONE;
            ev.count    = 1;
            ev.duration = _arena_cfg->zone_duration;
            float bx = boss->entity.rect.x + boss->entity.rect.width / 2;
            float by = boss->entity.rect.y + boss->entity.rect.height / 2;
            float px = player->entity.rect.x + player->entity.rect.width / 2;
            float py = player->entity.rect.y + player->entity.rect.height / 2;
            arena.execute_event(ev, *_arena_cfg, bx, by, px, py);
        }
    }
}

void BossSystemDirector::notify_phase2() {
    cinematic.trigger_phase2();
    timeline.record(encounter.total_time(), "PHASE2");
}

void BossSystemDirector::notify_last_stand(Monster* boss) {
    evolution.last_stand_triggered = true;
    if (auto* bai = dynamic_cast<BossAI*>(boss->ai)) {
        bai->_charge->cooldown    *= 0.5f;
        bai->_shockwave->cooldown *= 0.5f;
        bai->_summon->cooldown    *= 0.5f;
        bai->_shockwave->fx_radius *= 1.5f;
        bai->_charge->fx_radius   *= 1.3f;
    }
    cinematic.trigger_last_stand();
    timeline.record(encounter.total_time(), "LAST_STAND");
}

void BossSystemDirector::notify_death(const WorldState& ws,
    const RelationshipSystem& rels, const QuestManager& qm) {
    replay_mem.survive_time = encounter.total_time();
    encounter.end(replay_mem, dmg_done, dmg_taken, (int)arena.zones().size());
    battle_report = encounter.report();
    cinematic.trigger_death();
    timeline.record(encounter.total_time(), "DEATH");
    (void)ws; (void)rels; (void)qm;
}

void BossSystemDirector::init_events() {
    EventBus::inst().subscribe(GameEventType::BOSS_DEAD,
        [this](const GameEvent& ev) { notify_death_ev(ev); }, "BossSys");
    EventBus::inst().subscribe(GameEventType::FLOOR_ENTER,
        [this](const GameEvent&) { reset(); }, "BossSys");
}

void BossSystemDirector::notify_death_ev(const GameEvent&) {
    // D7 Step5: reserved — Boss死亡事件处理
}
