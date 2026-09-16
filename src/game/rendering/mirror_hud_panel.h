#pragma once
// ============================================================
// v1.6-B1.1: Mirror HUD Panel — "玩家终于看见它学会了什么"
// 只读 MirrorAgent 决策缓存 + 本局观察计数; 零逻辑侵入
// 分析卡 (顶部) = Echo 登场 6.5s 戏剧性 reveal; 观察卡 (右上) = 战斗期常驻
// ============================================================
class MirrorAgent;

struct MirrorHudPanel {
    // 顶部中央分析卡 (Skill Preference 4 柱 + 战斗习惯 4 叶)
    // 依据 battle_seconds 自动淡入 (0→0.5s) / 完整 (0.5→5.5s) / 收缩淡出 (5.5→6.5s)
    // 淡出完毕后本函数不再画任何像素
    static void render_analysis_card(const MirrorAgent& agent,
                                     int screen_w, float game_time);

    // 右上观察卡 (它猜你下一步 + 本局节奏)
    // 常驻显示; last_pred_age>2s 时预测行退化为 "观察中…"
    static void render_observation_card(const MirrorAgent& agent,
                                        float px, float py, float game_time);

private:
    // 4 柱状图 (skill preference; 高亮本命)
    static void _skill_bars(const MirrorAgent& agent, float px, float py,
                            float w, float h);
    // 4 叶习惯图 (behavior envelope; 非属性雷达)
    static void _habit_leaves(const MirrorAgent& agent, float px, float py,
                              float w, float h);
    // 预测行 (action + conf%, 或 stale → "观察中…")
    static void _prediction_row(const MirrorAgent& agent, float px, float py,
                                float game_time);
    // 本局频率行 (4 数字 /s + 战斗秒)
    static void _rhythm_row(const MirrorAgent& agent, float px, float py);
};
