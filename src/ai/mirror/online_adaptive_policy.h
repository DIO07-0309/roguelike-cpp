#pragma once
#include "ai/player_behavior/player_habit_profile.h"
#include <vector>

// ============================================================
// M4e: OnlineAdaptivePolicy — Thompson Sampling 在线自适应策略
// 上下文分桶 (距离3档 × 玩家血量3档 = 9桶) × 4 动作臂
// 离线行为画像(profile)注入先验 → 战斗中命中/闪避反馈驱动后验
// 标准 contextual bandit: 桶=状态, 臂=镜像动作
// ============================================================

enum class MirrorAction : int {
    APPROACH = 0,   // 贴近压制
    RETREAT  = 1,   // 后撤拉扯
    SKILL    = 2,   // 释放技能
    COMBO    = 3,   // 连招输出
};

class OnlineAdaptivePolicy {
public:
    static constexpr int BUCKET_COUNT = 9;    // 距离3档 × 血量3档
    static constexpr int ACTION_COUNT = 4;

    OnlineAdaptivePolicy();

    // 离线画像 → Beta 先验 (冷启动知识注入, 战斗反馈会逐步纠正)
    void init_prior(const PlayerHabitProfile& profile);

    // 上下文分桶: 距离(px)与玩家血量比例 → [0, BUCKET_COUNT)
    static int bucket_for(float dist_px, float player_hp_pct);

    // Thompson 采样: 对每臂采样 Beta(alpha, beta), 取最大样本
    int select_action(int bucket);

    // 在线更新: reward ∈ [0,1] 累加进对应桶/臂的后验
    void update(int bucket, int action, float reward);

    // 跨对局记忆: 导出/导入 Beta 参数 (144 float, 按桶-major 排列)
    void export_alpha(std::vector<float>& out) const;
    void export_beta(std::vector<float>& out) const;
    // 存档后验叠加回先验 (旧后验 = 新先验), 空输入忽略
    void import_alpha(const std::vector<float>& in);
    void import_beta(const std::vector<float>& in);

    // 调试: 某桶某臂的当前胜率估计 alpha/(alpha+beta)
    float win_rate(int bucket, int action) const;

private:
    float _alpha[BUCKET_COUNT][ACTION_COUNT];   // 胜计数
    float _beta[BUCKET_COUNT][ACTION_COUNT];    // 负计数

    static double _sample_beta(float a, float b);
    static double _sample_gamma(float a);       // Marsaglia & Tsang (2000)
    static double _rand_gaussian();             // Box-Muller
    static double _rand_uniform();
};
