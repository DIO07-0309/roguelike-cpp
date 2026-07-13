#pragma once
#include <string>
#include <vector>
#include "types/boss_types.h"

class Monster;
class Player;
class GameMap;
struct Effect;

// D5 Step4: BossCommand — Behavior→Execution 执行层

// Behavior → Command 转换
BossCommand    boss_decision_to_command(int decision_enum);
const char*    boss_command_name(BossCommand c);
BossAction     boss_default_action(BossCommand c);

// 执行 Command (BossAI 调用此函数替代原有状态机)
void boss_execute_command(BossCommand cmd, Monster* self, Player* player,
    GameMap* map, std::vector<Monster*>* all, std::vector<Effect>* effects,
    double game_time);
