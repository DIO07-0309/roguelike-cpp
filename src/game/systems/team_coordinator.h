#pragma once
#include <vector>
#include "raylib.h"

// 前向声明 — TeamCoordinator 不依赖 Monster 完整定义
class Monster;
class Player;
class GameMap;
enum class TeamRole : int;  // 来自 monster.h

// ============================================================
// G2.2: TeamDecision — 协同建议 (纯数据, 不包含执行逻辑)
// TeamCoordinator 是分析器, MonsterAI 是决策者
// ============================================================

struct TeamDecision {
    // ── 移动建议 (归一化方向, 0/0=无建议) ──
    float move_x = 0.0f;
    float move_y = 0.0f;

    // ── 辅助行动建议 ──
    bool     should_support = false;      // 应该 buff/治疗 盟友
    Monster* support_target = nullptr;    // 辅助目标
    int      support_type = 0;            // 0=buff, 1=heal

    // ── Elite 指挥光环 ──
    bool should_command = false;
};

// ============================================================
// G2.2: TeamCoordinator — 怪物协同分析器
// 纯 static, 无状态, 无跨 Tick 记忆
// 只读取 Monster/Player 状态, 不修改任何对象
// ============================================================

class TeamCoordinator {
public:
    // 核心入口: 分析单个怪物的协同上下文, 返回团队建议
    static TeamDecision evaluate(
        const Monster* self,
        const Player& player,
        const std::vector<Monster*>& all,
        const GameMap* map
    );

private:
    // 辅助: 找指定 TeamRole 的最近盟友 (8 tile 内)
    static Monster* _find_nearest_ally_with_role(
        const std::vector<Monster*>& allies,
        const Monster* self,
        TeamRole role,
        float self_ax, float self_ay
    );

    // 辅助: 找最近石柱位置 (arena cover)
    static bool _find_nearest_cover(
        const GameMap* map,
        float ax, float ay,
        float& out_cx, float& out_cy,
        float max_dist
    );
};
