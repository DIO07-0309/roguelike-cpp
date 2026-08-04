#pragma once
#include <string>
#include <vector>
#include <memory>
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

// F15.3: MirrorAgent
#include "ai/mirror/mirror_agent.h"
// F15 Mirror Combat
#include "mirror_combat_director.h"

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

    // ── F10.1: Arena state machine (domain boss) ──
    BossArenaState arena_state = BossArenaState::INTRO;
    float  domain_timer = 0.0f;
    float  domain_cycle_duration = 30.0f;  // seconds per full domain→mechanic→vulnerable cycle
    bool   boss_invulnerable = false;       // set true during DOMAIN/MECHANIC phases
    int    domain_cycle_count = 0;          // how many cycles completed
    std::string _behavior_type;             // from BossDef ("" = standard, "domain" = domain boss)

    // ── F10.2: Weak point tracking ──
    Monster* _active_core = nullptr;          // currently spawned fire_core (nullptr if broken)
    float    _vulnerable_dmg_mult = 2.0f;    // damage multiplier during VULNERABLE_PHASE
    float    _vulnerable_duration = 10.0f;   // F10.3: 弱点阶段时长 (数据驱动)
    float    _weakness_dmg_mult = 1.0f;      // F10.3: 克制元素对核心伤害倍率 (数据驱动)
    std::vector<std::unique_ptr<Monster>>* _weak_point_pool = nullptr;  // set by caller

    // ── F15: Mirror Boss ──
    std::unique_ptr<MirrorAgent> _mirror_agent;
    MirrorCombatDirector _mirror_combat;
    void _init_mirror_boss(Monster* boss, const class Player* player);
    // M4e: 跨对局记忆 — 导出/注入 (空 vector 安全)
    void export_mirror_memory(std::vector<float>& alpha,
                              std::vector<float>& beta) const;
    void inject_mirror_memory(const std::vector<float>& alpha,
                              const std::vector<float>& beta);

    std::string intro_text;
    std::string modifier_text;
    int  dmg_done = 0, dmg_taken = 0;

    // ── 生命周期 Hooks (GameScene 调用) ──
    void reset();   // 新楼层开始时调用
    void init_on_spawn(Monster* boss, int floor, const WorldState& ws, BuildType bt,
                       const RelationshipSystem& rels, GameMap* map,
                       const class Player* player = nullptr);
    void tick(float dt, Monster* boss, Player* player, int floor,
              const WorldState& ws, const RelationshipSystem& rels,
              StoryStage stage, std::vector<std::unique_ptr<Monster>>& monsters,
              std::vector<Effect>* effects = nullptr);
    void notify_phase2();
    void notify_last_stand(Monster* boss);
    void notify_death(const WorldState& ws, const RelationshipSystem& rels,
                      const QuestManager& qm);
    void on_core_maybe_erased();   // F10.2-fix: UAF guard — cleanup 前置钩子

    void init_events();  // D7 Step5: 订阅 EventBus

private:
    void notify_death_ev(const struct GameEvent&);
    void _tick_domain_state(float dt, Monster* boss);  // F10.1
    void _spawn_domain_core(Monster* boss);             // F10.2
    bool _enraged = false;                              // M4d-fix: 狂暴领域标记
    void _tick_core_phase(float dt, Monster* boss, float max_duration);  // M4d-fix
    void _enter_vulnerable_phase(Monster* boss, bool core_destroyed);    // M4d-fix
    float _enraged_cycle_duration() const;      // M4d-fix: 狂暴时核心周期减半
    float _enraged_vulnerable_duration() const; // M4d-fix: 狂暴时弱点窗口减半

public:
    // ── 查询接口 ──
    const BossBattleReport& report() const { return battle_report; }
    BossCommand command() const { return current_cmd; }
    bool cinematic_active() const { return cinematic.is_running(); }
    const BossTimeline& get_timeline() const { return timeline; }

    // ── G2.3: Arena spawn timer (属于 BossSystemDirector — Encounter 节奏) ──
    float _arena_spawn_timer = 0.0f;
    const BossArenaDef* _arena_cfg = nullptr;
    int   _arena_phase = 0;     // M4b: 0=normal, 1=phase2, 2=last_stand
    float _arena_spawn_mult = 1.0f; // M4b: spawn_interval multiplier per phase
};
