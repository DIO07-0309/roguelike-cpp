#pragma once
#include "node.h"
#include "input_map.h"
#include "player.h"
#include "monster.h"
#include "item.h"
#include "game_map.h"
#include "tutorial_guide.h"
#include "combat_feel.h"    // G10.8-B1: HitStop 常量
#include "systems/weapon_executor.h"  // G10.8-B1: 正式战斗链
#include <memory>
#include <vector>

class TutorialScene : public Node {
public:
    void _ready() override;
    void _process(double delta) override;
    void _render() override;
    void _input(const InputMap& input) override;

    std::unique_ptr<Player> player;
    std::shared_ptr<GameMap> game_map;
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<DroppedItem> ground_items;
    TutorialGuide guide;
    bool inventory_open = false;
    int inv_cursor = 0;
    bool gave_skill = false;
    float game_time = 0;
    float cam_x = 0, cam_y = 0;
    float _tutorial_hitstop = 0.0f;   // G10.8-B1: 轻量打击停顿
    int element_cursor = 0;           // G10.8-B2: 元素选择光标
};
