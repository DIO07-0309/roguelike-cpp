#pragma once
#include <memory>
#include <vector>
#include <string>
#include "node.h"
#include "player.h"
#include "monster.h"
#include "item.h"
#include "game_map.h"
#include "vfx_server.h"

// ============================================================
// GameScene — 核心游戏场景（替代 game.py 的 PLAYING 状态）
// ============================================================
enum class GameState { TITLE, PLAYING, BOSS_INTRO, BOSS_CINEMATIC,
                       FLOOR_SELECT, TUTORIAL, DEATH, VICTORY };

class GameScene : public Node {
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
    void load_saved_game(int floor, int max_floor, std::unique_ptr<Player> p,
                         uint32_t seed = 0,
                         const std::vector<bool>& special_triggered = {},
                         const std::vector<bool>& special_discovered = {});

    // 输入 (override Node::_input)
    void _input(const InputMap& input) override;

private:
    // 战斗
    void _player_attack();
    void _use_skill(int index);
    void _update_monsters(float dt);
    void _on_monster_killed(Monster* m);
    void _check_floor_clear();
    void _cleanup_dead_monsters();   // poison tick 后统一收尾
    void _apply_pending_damage();
    void _handle_debug_buff_test_input();  // F1-F6 Debug Buff 测试 (仅 _DEBUG)

    // 楼层
    void _activate_stairs();
    void _check_floor_transition();

    // 道具
    void _pickup();
    void _interact_special();   // B8: 特殊房间交互 (E键优先)
    void _spawn_floor_monsters(const std::vector<std::pair<int,int>>& rooms);

    // B10: 特殊房间体验层
    void _check_special_room_discovery();
    void _show_room_message(const std::string& msg);
    void _draw_room_message();
    std::string _room_message;
    float _room_message_timer = 0.0f;

    // Boss
    Monster* _get_boss() const;
    void _drop_boss_reward(Monster* boss);

    // 渲染辅助
    void _draw_map();
    void _draw_entities();
    void _draw_ground_items();
    void _draw_hud();
    void _draw_inventory_panel();
    void _draw_skill_bar();
    void _draw_player_buffs();     // 玩家 Buff HUD（技能栏下方，全名+层数+时间）
    void _draw_monster_buffs(const Monster& m, float draw_x, float draw_y);  // 怪物 Buff 简写标签
    void _draw_effects();
    void _draw_time_stop_overlay();
    void _draw_boss_cinematic_overlay();
    void _draw_boss_intro();

    float _cam_x = 0, _cam_y = 0;
    void _update_camera();
};
