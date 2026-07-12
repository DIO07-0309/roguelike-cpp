#pragma once
#include <string>
#include <vector>

class Monster;
class Player;
class GameMap;
struct Effect;

// ============================================================
// D5 Step4: BossCommand — Behavior→Execution 执行层
// ============================================================
enum class BossCommand {
    NONE,
    MOVE,        CHARGE,    SHOCKWAVE,  SUMMON,
    DEFEND,      RETREAT,   CAST,       PHASE,
    LAST_STAND
};

// 单条技能/动作指令
struct BossAction {
    BossCommand command = BossCommand::NONE;
    float duration = 0.0f;       // 动作持续时间
    float cooldown = 0.0f;       // 冷却时间
    float windup = 0.0f;         // 蓄力时间 (0=瞬发)
};

// 技能连招队列 (BossCombo)
struct BossSkillQueue {
    std::vector<BossCommand> queue;
    int  current = -1;
    float next_delay = 0.0f;
    bool active = false;

    void clear() { queue.clear(); current = -1; active = false; }
    void enqueue(BossCommand c) { queue.push_back(c); }
    void start() { active = true; current = 0; next_delay = 0.3f; }
    BossCommand current_cmd() const {
        return (current >= 0 && current < (int)queue.size()) ? queue[current] : BossCommand::NONE;
    }
    void advance() { current++; if (current >= (int)queue.size()) clear(); }
};

// Behavior → Command 转换
BossCommand    boss_decision_to_command(int decision_enum);
const char*    boss_command_name(BossCommand c);
BossAction     boss_default_action(BossCommand c);

// 执行 Command (BossAI 调用此函数替代原有状态机)
void boss_execute_command(BossCommand cmd, Monster* self, Player* player,
    GameMap* map, std::vector<Monster*>* all, std::vector<Effect>* effects,
    double game_time);
