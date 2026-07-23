#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "node.h"
#include "player.h"
#include "monster.h"
#include "item.h"
#include "game_map.h"
#include "vfx_server.h"
#include "game_renderer.h"
#include "interaction_handler.h"
#include "floor_manager.h"
#include "combat_coordinator.h"
#include "build_score.h"
#include "event_system.h"
#include "floor_narrative.h"
#include "npc_system.h"
#include "world_state.h"
#include "quest_manager.h"
#include "relationship_system.h"
#include "world_reaction.h"
#include "boss_narrative.h"
#include "growth_curve.h"
#include "combat_feel.h"
#include "flow_director.h"
#include "relic_progression.h"
#include "meta_progression.h"
#include "boss_evolution.h"
#include "boss_behavior.h"
#include "boss_command.h"
#include "arena_manager.h"
#include "boss_replay.h"
#include "boss_encounter.h"
#include "camera_director.h"
#include "boss_cinematic.h"
#include "boss_timeline.h"
#include "ending_director.h"
#include "boss_system_director.h"
#include "gameplay_system_director.h"
#include "presentation_system_director.h"
#include "game_flow_director.h"
#include "player_controller.h"
#include "scene/game_scene_input.h"
#include "scene/game_scene_combat.h"
#include "scene/game_scene_interaction.h"

// ── G4.5: Replay ──
#include "core/replay/recorder.h"
#include "core/replay/player.h"

// ============================================================
// D4 Step2: EventPresentation — 事件演出系统
// ============================================================
enum class EventPhase { INACTIVE, ENTER, DESC, CHOICE, ANIM, REWARD, COMPLETE };

struct EventPresentation {
    bool     active = false;
    EventPhase phase = EventPhase::INACTIVE;
    EventType current = EventType::NONE;
    float timer = 0.0f;           // 阶段计时器
    float fade = 0.0f;            // 淡入淡出 0~1
    int  selected = 0;            // 选项索引
    int  option_count = 0;        // 选项数 (0=确认型, >0=选择型)
    std::string desc_text;        // 事件描述文字
    std::string reward_text;      // 奖励文字 (phase=REWARD时显示)
    std::string complete_text;    // 完成文字 (phase=COMPLETE时显示)
    Color  anim_color{255,200,50,255}; // 动画颜色
};

// ============================================================
// GameScene — 核心游戏场景（替代 game.py 的 PLAYING 状态）
// ============================================================
enum class GameState { TITLE, PLAYING, BOSS_INTRO, BOSS_CINEMATIC,
                       FLOOR_SELECT, TUTORIAL, DEATH, VICTORY };

class GameScene : public Node {
    friend class GameFlowDirector;
    friend class PlayerController;
    friend class GameSceneInput;
    friend class GameSceneCombat;
    friend class GameSceneInteraction;
public:
    GameState state = GameState::TITLE;

    // 核心数据
    std::unique_ptr<Player> player;
    std::shared_ptr<GameMap> game_map;
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<DroppedItem> ground_items;

    int current_floor = 1;
    int max_unlocked_floor = 1;
    uint32_t _dungeon_seed = 0;   // B8: 当前楼层地牢种子

    // 游戏状态
    bool inventory_open = false;
    int inventory_cursor = 0;
    bool stairs_active = false;
    std::pair<int,int> stairs_pos{0, 0};
    float game_time = 0.0f;

    // 时停
    float time_stop_remaining = 0.0f;
    std::vector<std::pair<Monster*, int>> pending_damage;

    // Boss
    std::string boss_intro_title, boss_intro_lore, boss_intro_skills;
    Color boss_intro_color{200, 40, 40, 255};
    float boss_cinematic_timer = 0.0f;
    int boss_floor = 0;

    // 等待场景入树后播放 BGM
    std::string _pending_bgm;

    // VFX
    std::vector<Effect> active_effects;

    // 信号
    Object::Signal<> on_floor_cleared;
    Object::Signal<int> on_player_leveled;

    // 生命周期
    void _ready() override;
    void _process(double delta) override;
    void _render() override;

    // 场景间通信
    void enter_floor(int floor, uint32_t seed = 0);
    void new_game();
    // B13: Relic 不再跨层 (load_saved_game 不再接收 relics 参数)
    void load_saved_game(int floor, int max_floor, std::unique_ptr<Player> p,
                         uint32_t seed = 0,
                         const std::vector<bool>& special_triggered = {},
                         const std::vector<bool>& special_discovered = {},
                         const std::unordered_map<std::string, int>& rule_counters = {},
                         const std::unordered_map<int, int>& quest_states = {},
                         const std::vector<int>& unlocked_endings = {});

    // 输入 (override Node::_input)
    void _input(const InputMap& input) override;

    // ── G4.5: Replay ──
    void start_recording(uint32_t seed);
    void start_replay(const std::string& path);
    ReplayRecorder& recorder() { return _recorder; }
    ReplayPlayer&  replay_player() { return _replay_player; }

    static std::string g_record_path;   // --record <path>
    static std::string g_replay_path;   // --replay <path>
    static bool g_record_mode;
    static bool g_replay_mode;
    static bool g_sim_mode;             // G5.6: --sim mode
    static int  g_sim_runs;             // G5.6: number of sim runs
    static bool g_sim_all_builds;       // G7.4: --sim-all-builds mode
    static int  g_sim_build_type;       // G7.4: current build in all-builds rotation
    static std::string g_sim_ai_type;   // G8.1: "decision" or "bt"
    static int  g_rl_test_episodes;     // G8.4: --rl-test N
    static int  g_rl_train_episodes;    // G8.4: --rl-train N

private:
    // 战斗
    void _player_attack();
    void _use_skill(int index);
    void _update_monsters(float dt);
    void _on_monster_killed(Monster* m);
    void _check_floor_clear();
    void _cleanup_dead_monsters();
    void _apply_pending_damage();
    // D4.6 Debug flags (按F键切换的面板) — moved to presentation
    // D6 Step3: Boss全子系统
    BossSystemDirector _boss;
    // D6 Step4: Gameplay全子系统 (Flow/Quest/Relationship/Story/Ending/Meta)
    GameplaySystemDirector _gameplay;
    // D6 Step5: Presentation全子系统 (Shake/Freeze/Damage/Msg/Intro/Overlays)
    PresentationSystemDirector _presentation;
    // D6 Step6: GameFlowDirector (生命周期状态机 + 场景切换)
    GameFlowDirector _flow;
    // D6 Step7: PlayerController (玩家输入/攻击/技能/移动/交互)
    PlayerController _player_ctrl;

    // 楼层 (委托给 FloorManager)
    void _activate_stairs();
    void _check_floor_transition();

    // 特殊房间 (委托给 InteractionHandler)

    // Relic 面板
    bool _show_relic_panel = false;

    // Boss (B15: Phase2 + entrance + 击杀奖励增强)
    Monster* _get_boss() const;
    void _drop_boss_reward(Monster* boss);
    float _boss_entrance_timer = 0.0f;
    bool  _boss_phase2_shown = false;
    bool  _boss_entered = false;

    // C1: 伤害数字 / 震屏 / 冻结 (moved to PresentationSystemDirector)

    // D3 Step3: The World 进化回调节 (E2 shockwave / E3 speed)
    int _tw_evo_level = 0;
    float _tw_speed_boost = 0.0f;

    // D3 Step4: Build Fusion — 构筑通知追踪 (moved to GameplaySystemDirector)

    // D4 Step2: 事件演出
    EventPresentation _event_ui;
    bool _is_event_running() const { return _event_ui.active; }
    void _start_event_presentation(EventType et);
    void _tick_event_ui(float dt);
    void _draw_event_ui(int sw, int sh);

    // D4 Step3: 楼层/章节叙事 — moved to PresentationSystemDirector

    // D4 Step4: NPC / Dialogue
    NPCState _npc_state[10];
    int     _npc_count = 0;
    int     _current_npc_index = -1;
    DialogueState _dialogue;
    bool _quest_log_open = false;
    // NPC 放置位置 (瓦片坐标)
    int    _npc_tile_x[10], _npc_tile_y[10];

    NPCState* _find_or_create_npc_state(int npc_id);
    void  _spawn_floor_npcs(int floor, const std::vector<std::pair<int,int>>& rooms);
    void  _start_dialogue(int npc_index);
    void  _update_dialogue(float dt);
    void  _draw_dialogue(int sw, int sh);
    void  _draw_quest_log(int sw, int sh);
    GameSceneInput  _input_handler{*this};
    GameSceneCombat _combat{*this};
    GameSceneInteraction _interaction{*this};
    GameRenderer _renderer;
    InteractionHandler _interact;

    // 渲染辅助 (保留 GameScene 中的轻量级方法)
    void _draw_map();
    void _draw_entities();
    void _draw_ground_items();

    float _cam_x = 0, _cam_y = 0;

    // ── G4.5: Replay ──
    ReplayRecorder _recorder;
    ReplayPlayer   _replay_player;
    bool _is_action_just_pressed(const InputMap& input, const char* name);
    void _tick_replay_hash();

    // ── G5.6/G7.4/G8.1: Sim AI (dual-agent support) ──
    class DecisionAgent;
    class BTAgent;
    std::unique_ptr<DecisionAgent> _sim_ai;
    std::unique_ptr<BTAgent> _sim_bt;
    bool _sim_mode = false;
    bool _use_bt_agent = false;        // G8.1: true = BT, false = DecisionAgent
    void _collect_sim_stats();
};
