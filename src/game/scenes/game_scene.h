#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <cstdint>
#include "config.h"      // Batch 1 (D4): FOV_RADIUS_DEFAULT
#include "node.h"
#include "player.h"
#include "monster.h"

// G8.1: AI agent forward declarations (global scope)
class DecisionAgent;
class BTAgent;
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
#include "challenge_room.h"
#include "relic_progression.h"
#include "meta_progression.h"
#include "boss_evolution.h"
#include "boss_behavior.h"
#include "boss_command.h"
#include "arena_manager.h"
#include "boss_replay.h"
#include "minimap.h"
#include "room_manager.h"    // Batch 2C: Room Encounter
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

// Batch 3I: WorldMode — 主地牢 / 挑战竞技场 切换
enum class WorldMode : uint8_t {
    DUNGEON,           // 正常地牢探索
    CHALLENGE_ARENA    // 挑战竞技场 (主地牢冻结)
};

class GameScene : public Node {
    friend class GameFlowDirector;
    friend class PlayerController;
    friend class GameSceneInput;
    friend class GameSceneCombat;
    friend class GameSceneInteraction;
public:
    GameScene();
    ~GameScene();  // G8.1: defined in .cpp (needed for unique_ptr<DecisionAgent/BTAgent>)
    GameState state = GameState::TITLE;

    // Batch 2B: 接触开门回调 — 门开启后重算 FOV (由 PlayerController 在 R1 触发时调用)
    void on_door_opened();

    // Batch 2C: Room Encounter 通知 — 由 RoomManager 调用 (显示房间消息)
    void show_room_message(const char* msg);

    // G10.8-B4: 首次提示转发 (first_hint.h 使用; 简单转给 show_message)
    void show_hint(const char* msg, float duration = 4.5f);

    // 核心数据
    std::unique_ptr<Player> player;
    std::shared_ptr<GameMap> game_map;
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<DroppedItem> ground_items;

    int current_floor = 1;
    int max_unlocked_floor = 1;
    uint32_t _dungeon_seed = 0;   // B8: 当前楼层地牢种子

    // G10.1: Element select overlay
    bool element_select_active = false;
    int  element_select_cursor = 0;

    // 游戏状态
    bool inventory_open = false;
    int inventory_cursor = 0;
    bool stairs_active = false;
    std::pair<int,int> stairs_pos{0, 0};
    float game_time = 0.0f;

    // Batch 3H: Gamble Room UI state
    bool gamble_open = false;
    int  gamble_cursor = 0;
    std::string gamble_result_msg;
    float gamble_result_timer = 0.0f;

    // 时停
    float time_stop_remaining = 0.0f;
    std::vector<std::pair<Monster*, int>> pending_damage;

    // M4.2: 镜像专属真冻结 — 玩家禁移动/攻击 (Echo 可行动, 计时在 Director::tick 递减)
    bool player_frozen_by_mirror() const { return _boss.mirror_freeze_active(); }

    // Boss
    std::string boss_intro_title, boss_intro_lore, boss_intro_skills;
    Color boss_intro_color{200, 40, 40, 255};
    float boss_cinematic_timer = 0.0f;
    int boss_floor = 0;

    // 等待场景入树后播放 BGM
    std::string _pending_bgm;

    // VFX
    std::vector<Effect> active_effects;

    // D2: Unified projectiles (PLAYER + MONSTER + ENVIRONMENT)
    std::vector<Projectile> projectiles;

    // 信号
    Object::Signal<> on_floor_cleared;
    Object::Signal<int> on_player_leveled;

    // 生命周期
    void _ready() override;
    void _process(double delta) override;
    void _render() override;

    // 场景间通信
    void enter_floor(int floor, uint32_t seed = 0);
    // Batch 3I: 挑战竞技场往返 (场景级转换; G9.2 移至 public 以支持回归测试驱动)
    void enter_challenge_arena();
    void exit_challenge_arena();
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

    // G9.2 (audit TEST-001): 生命周期回归测试访问器 — 仅测试/诊断用, 勿在业务逻辑中穿透
    ChallengeRoomController&       challenge_ctrl()       { return _challenge; }
    const ChallengeRoomController& challenge_ctrl() const { return _challenge; }
    RoomManager&       room_mgr()       { return _room_mgr; }
    const RoomManager& room_mgr() const { return _room_mgr; }
    WorldMode          world_mode() const { return _world_mode; }

    // G9.3 (RNG-001): 屏震相机偏移计算 — 消耗独立 visual_rng, 严禁触碰 gameplay rng()。
    // _draw() 每帧调用; timer<=0 返回零偏移。static 纯函数以便确定性回归测试。
    static std::pair<float, float> shake_offset(float intensity, float timer);

    // ── M4e: 跨对局镜像记忆 (读档时由场景注入, spawn 自动恢复) ──
    void set_mirror_memory(const std::vector<float>& alpha,
                           const std::vector<float>& beta);
    std::vector<float> _mirror_mem_alpha;
    std::vector<float> _mirror_mem_beta;

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
    static int  g_rl_mirror_episodes;   // F15.4: --rl-mirror N
    static bool g_show_mirror_acc;      // 验收: F9 toggle MIRROR AI 调用链统计

private:
    // 战斗
    void _player_attack();
    void _use_skill(int index);
    void _update_monsters(float dt);
    void _unstuck_wedged_monsters(double gt);   // Q3.2: 怪物脱卡 (贴墙钉子户软锁修复)
    void _on_monster_killed(Monster* m);
    void _check_floor_clear();
    void _cleanup_dead_monsters();
    void _apply_pending_damage();
    // 收官: EXPLOSIVE_BARREL 可交互闭环 (攻击触发→倒计时→AOE 爆炸)
    void _try_trigger_barrel_near(const Rectangle& rect);
    void _explode_barrel(const ArenaObject& ao);
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
    // M4b: Boss 房机制地形 (熔岩环带安全区)
    void _setup_boss_arena_terrain(const class DungeonGenerator& gen, int floor);
    float _lava_tick_timer = 0.0f;   // M4b: 熔岩灼烧 0.5s 节拍

    // 特殊房间 (委托给 InteractionHandler)

    // Relic 面板
    bool _show_relic_panel = false;

    // Boss (B15: Phase2 + entrance + 击杀奖励增强)
    Monster* _get_boss() const;
    void _drop_boss_reward(Monster* boss);
    float _boss_entrance_timer = 0.0f;
    bool  _boss_phase2_shown = false;
    bool  _boss_entered = false;
    BossArenaState _boss_last_arena_state = BossArenaState::INTRO;  // F10.1

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
    // F15.5: Mirror analysis panel
    void  _draw_mirror_analysis_panel(int sw, int sh);
    GameSceneInput  _input_handler{*this};
    GameSceneCombat _combat{*this};
    GameSceneInteraction _interaction{*this};
    GameRenderer _renderer;
    InteractionHandler _interact;

    // Phase 3: Minimap — 只读 GameMap，无第二套探索状态
    MinimapRenderer _minimap;
    bool _show_minimap = true;              // 默认显示，M 切换

    // Batch 2C: Room Encounter Manager
    RoomManager _room_mgr;
    // Batch 3F: Challenge Room Controller
    ChallengeRoomController _challenge;

    // Batch 3I: WorldMode + Challenge Arena transition state
    WorldMode _world_mode = WorldMode::DUNGEON;
    float _saved_player_x = 0, _saved_player_y = 0;
    float _teleport_fade_timer = 0.0f;
    float _portal_pulse_timer = 0.0f;
    bool _portal_fade_in = false;
    bool is_save_blocked() const;

    // Batch 3I: Challenge choice UI
    bool challenge_choice_active = false;
    int challenge_choice_cursor = 0;

    // Batch 3I: Independent arena — saved dungeon state during arena
    std::shared_ptr<GameMap> _arena_map;
    std::vector<std::unique_ptr<Monster>> _arena_monsters;
    std::shared_ptr<GameMap> _saved_dungeon_map;
    std::vector<std::unique_ptr<Monster>> _saved_dungeon_monsters;
    std::vector<DroppedItem> _saved_dungeon_ground_items;
    int _return_portal_tx_arena = -1, _return_portal_ty_arena = -1;
    std::pair<int,int> _boss_last_known{-1,-1};  // Boss 最后已知可见位置（已发现才记录）

    // 渲染辅助 (保留 GameScene 中的轻量级方法)
    void _draw_map();
    void _draw_entities();
    void _draw_ground_items();
    void _draw_arena_map();
    void _draw_arena_entities();
    void _cleanup_dead_arena_monsters();

    float _cam_x = 0, _cam_y = 0;

    // Phase 1: FOV — 玩家跨 tile 时更新
    int _last_player_tile_x = -1;
    int _last_player_tile_y = -1;
    // Batch 1 (D4): FOV 半径可配置 — enter_floor 时按 FloorConfig::fov_radius 解析 (0→FOV_RADIUS_DEFAULT)
    int _fov_radius = FOV_RADIUS_DEFAULT;

    // ── G4.5: Replay ──
    ReplayRecorder _recorder;
    ReplayPlayer   _replay_player;
    bool _is_action_just_pressed(const InputMap& input, const char* name);
    void _tick_replay_hash();

    // ── G5.6/G7.4/G8.1: Sim AI (dual-agent support) ──
    std::unique_ptr<DecisionAgent> _sim_ai;
    std::unique_ptr<BTAgent> _sim_bt;
    bool _sim_mode = false;
    bool _use_bt_agent = false;        // G8.1: true = BT, false = DecisionAgent
    void _collect_sim_stats();
    double _sim_wall_start = 0;        // Q3.10: 墙钟兜底超时 (game_time 可能被时停稀释)

    // Q3.2: sim 真实伤害统计 — HP 差值累计 (毒池/怪伤全计入, 替代 kills*10 估算)
    int _sim_hp_prev = -1;
    double _sim_dmg_taken = 0;
    double _sim_dmg_dealt = 0;
    double _sim_heal_total = 0;
    // Q3.9: 按稳定 instance_id 记账 (原 intptr_t 指针键: 跨进程堆地址不同 + 地址复用 → 假伤害/非确定性)
    std::map<uint64_t, int> _sim_mon_hp;

    // Q3.8: 怪物脱卡状态 — instance_id 键 (原指针键: 跨进程堆地址不同 + 地址复用 → 残留污染/非确定性)
    std::unordered_map<uint64_t, std::pair<int, int>> _unstuck_last_pos;
    std::unordered_map<uint64_t, double> _unstuck_since;
};
