#pragma once
#include <vector>
#include <string>
#include <memory>
#include "raylib.h"

class Player;
class Monster;
class GameMap;
struct DroppedItem;

// ============================================================
// TutorialGuide — 6阶段新手教程 (与 Python 版一致)
// ============================================================
enum class TutorialStage {
    WELCOME, ELEMENT, MOVE, ATTACK_COMBO, PICKUP, INVENTORY, EQUIP,
    SKILL, COOLDOWN, WEAPON_INFO, COMPLETE
};

class TutorialGuide {
public:
    TutorialStage stage = TutorialStage::WELCOME;

    // 每阶段提示文字
    std::vector<std::string> get_instructions() const;

    // 检测并推进阶段
    void check_and_advance(Player* player, bool inventory_open,
                           std::vector<Monster*>& monsters,
                           std::vector<DroppedItem>& items);
    void advance_stage();
    void notify_skill_used();

    // 状态
    float move_distance = 0;
    int attack_hits = 0;
    bool picked_up = false, item_used = false, equipped = false, _skill_used = false;
    // G10.8-B2 新步状态
    bool element_picked = false;
    int combo_reached = 0;
    bool cooldown_waited = false;

private:
    float _last_px = 0, _last_py = 0;
    bool _last_set = false;

    void _check_move(Player* p);
    void _check_attack(std::vector<Monster*>& monsters);
    void _check_pickup(std::vector<DroppedItem>& items);
    void _check_inventory(Player* p);
    void _check_equip(Player* p);
    void _check_skill();
    void _check_element(Player* p);       // G10.8-B2
    void _check_cooldown(Player* p);      // G10.8-B2
};

// 工厂函数
std::shared_ptr<GameMap> build_tutorial_map();
std::unique_ptr<Monster> create_tutorial_dummy(int tx, int ty);
std::vector<DroppedItem> create_tutorial_items(int tx, int ty);
void give_tutorial_skill(Player* p);
