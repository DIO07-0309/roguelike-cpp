#pragma once
#include <vector>
#include <memory>
#include <string>
#include "raylib.h"
#include "entity.h"

// 前向声明
class Player;
class Monster;
class GameMap;
struct DroppedItem;
struct Effect;
class InputMap;

// ============================================================
// GameRenderer — 纯渲染类 (组合模式)
// 接收 GameScene 的状态数据，执行所有绘制
// 不持有可变状态，不修改游戏数据
// ============================================================
class GameRenderer {
public:
    // ---- 静态绘制工具 (无状态) ----
    static void draw_panel(Rectangle r, const char* title, Color bg = {20, 20, 40, 230});
    static void draw_glow_text(const char* text, float x, float y, int size, Color c,
                               bool center = true);
    static void draw_progress_bar(Rectangle r, float ratio, Color fill,
                                  Color bg = {30, 30, 60, 255});

    // ---- 摄像机 ----
    void update_camera(float& cam_x, float& cam_y, const Player* player,
                       const GameMap* map, int screen_w, int screen_h);

    // ---- 特效与覆盖层 ----
    void draw_effects(const std::vector<Effect>& effects, float cam_x, float cam_y);
    void draw_time_stop_overlay(int sw, int sh, float time_stop_remaining);
    void draw_boss_cinematic_overlay(int sw, int sh);
    void draw_boss_intro(int sw, int sh, const std::string& title, const std::string& lore,
                         const std::string& skills_text, Color color, int boss_floor);
    void draw_room_message(int sw, int sh, const std::string& msg, float timer);

    // ---- HUD 渲染 ----
    void draw_hud(const Player* player, int current_floor, float game_time,
                  Monster* boss, bool show_relic_panel,
                  int inventory_open, int inventory_cursor,
                  const std::string& room_msg, float room_msg_timer,
                  int screen_w, int screen_h);
    void draw_skill_bar(const Player* player, float game_time);
    void draw_player_buffs(const Player* player);
    void draw_player_relics(const Player* player);
    void draw_monster_buffs(const Monster& m, float draw_x, float draw_y);
    void draw_inventory_panel(const Player* player, int cursor, int sw, int sh);
    void draw_relic_panel(const Player* player, int sw);

private:
    static Color _relic_rarity_color(const std::string& rarity);
    static std::string _rarity_label_cn(const std::string& rarity);
};
