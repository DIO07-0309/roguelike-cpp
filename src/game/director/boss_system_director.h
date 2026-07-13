#pragma once
#include <string>
#include "boss_narrative.h"
#include "boss_evolution.h"
#include "boss_behavior.h"
#include "boss_command.h"
#include "arena_manager.h"
#include "boss_replay.h"
#include "boss_encounter.h"
#include "boss_cinematic.h"
#include "boss_timeline.h"
#include "build_score.h"

class Monster;
class Player;
class GameMap;
class WorldState;
class RelationshipSystem;
struct Effect;
struct GameEvent;

// ============================================================
// D6 Step3: BossSystemDirector — 收纳所有Boss子系统
// 组合模式: 持有Narrative/Evolution/Behavior/Command/
//           Encounter/Replay/Cinematic/Arena/Timeline
// GameScene 仅通过 BossSystemDirector 协调 Boss 生命周期
// ============================================================
class BossSystemDirector {
public:
    // ── 聚合的子系统 (组合, 非继承) ──
    BossNarrative         narrative;
    BossEvolutionData     evolution;
    BossBehaviorState     behavior;
    BossSkillQueue        skill_queue;
    ArenaManager          arena;
    BossEncounterController encounter;
    BossReplayMemory      replay_mem;
    BossBattleReport      battle_report;
    BossCinematicController cinematic;
    BossTimeline          timeline;
    BossCommand           current_cmd = BossCommand::NONE;
    BossModifierHook      modifier_hook;

    std::string intro_text;
    std::string modifier_text;
    int  dmg_done = 0, dmg_taken = 0;

    // ── 生命周期 Hooks (GameScene 调用) ──
    void reset();   // 新楼层开始时调用
    void init_on_spawn(Monster* boss, int floor, const WorldState& ws, BuildType bt,
                       const RelationshipSystem& rels, GameMap* map);
    void tick(float dt, Monster* boss, const Player* player, int floor,
              const WorldState& ws, const RelationshipSystem& rels);
    void notify_phase2();
    void notify_last_stand(Monster* boss);
    void notify_death(const WorldState& ws, const RelationshipSystem& rels,
                      const QuestManager& qm);

    void init_events();  // D7 Step5: 订阅 EventBus

private:
    void notify_death_ev(const struct GameEvent&);

public:
    // ── 查询接口 ──
    const BossBattleReport& report() const { return battle_report; }
    BossCommand command() const { return current_cmd; }
    bool cinematic_active() const { return cinematic.is_running(); }
    const BossTimeline& get_timeline() const { return timeline; }
};
