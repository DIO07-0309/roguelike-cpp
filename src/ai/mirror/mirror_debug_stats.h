#pragma once
// M3/验收: MirrorDebugStats — F15 后验验收的 AI 调用链统计。
// 证明"链路真闭环": predict/克隆/规则/打断/Phase行为 均有真实计数。
#include <string>

struct MirrorDebugSnap {
    int  predict_count = 0;       // predict_next_action 总次数
    int  clone_exact = 0;         // 克隆层 level 0 (精确状态命中)
    int  clone_fuzzy = 0;         // 克隆层 level 1 (模糊状态命中)
    int  profile_fallback = 0;    // 降级链 level 2 (画像)
    int  default_fallback = 0;    // 降级链 level 3 (默认)
    int  rule_fallback = 0;       // 规则兜底 (克隆层未命中的规则分支)
    int  clone_arbitrate = 0;     // 行为选择仲裁: 克隆层采纳次数
    int  ml_slot_used = 0;        // 行为选择仲裁: ML 插槽使用次数
    int  rl_used = 0;             // v0.9.30: 行为选择仲裁: RL Q 表使用次数
    int  thompson_used = 0;       // 行为选择仲裁: Thompson 采样次数
    int  interrupt_attempt = 0;   // BOSS 打断尝试
    int  interrupt_success = 0;   // 打断成功 (时停命中)
    int  behavior_attack = 0;     // 行为分布: 普攻状态帧
    int  behavior_skill = 0;      // 行为分布: 技能状态帧
    int  behavior_retreat = 0;    // 行为分布: 后撤状态帧
    int  behavior_approach = 0;   // 行为分布: 逼近状态帧
    float phase_seconds[3] = {0, 0, 0};   // 各 Phase 战斗时长 (s)
};

class MirrorDebugStats {
public:
    void reset();
    void on_predict(int level);           // 按降级链等级计数 (0-3, -1=规则)
    void on_arbitrate(bool clone_used, bool ml_used);
    void on_rl();                       // v0.9.30: RL 仲裁计数
    void on_interrupt(bool success);
    void on_behavior_state(int state);    // 0=approach,1=attack,2=skill,3=retreat
    void tick_phase(int phase, float dt);

    MirrorDebugSnap snapshot() const;
    std::string summary() const;          // 单行摘要 (HUD/日志)

private:
    MirrorDebugSnap _s;
};