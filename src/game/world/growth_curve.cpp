#include "growth_curve.h"

// ============================================================
// D4.6 Step1: 15层成长曲线 (唯一数值权威来源)
// monster_hp/atk: 平滑递增, 每章略有加速
// boss_hp/atk: 跳跃式增长 (Boss层使用)
// elite_scale: 1.8~2.5× monster base
// exp_scale: 第一章快→第二章放缓→第三章明显放缓
// gold_scale: 后期略多
// relic_scale: 后期稀有度提升
// arena_scale: 后期陷阱伤害/收益同步
// ============================================================

static const GrowthCurve CURVES[15] = {
    // F1: 教学层, 最轻松
    {1.00f,1.00f, 1.00f,1.00f, 1.80f, 1.00f,1.00f,1.00f,1.00f, 1.00f,1.00f},
    // F2: 略微提升
    {1.10f,1.10f, 1.12f,1.10f, 1.85f, 1.08f,1.05f,1.05f,1.05f, 1.02f,1.02f},
    // F3: 继续温和
    {1.20f,1.20f, 1.25f,1.20f, 1.90f, 1.15f,1.10f,1.10f,1.10f, 1.04f,1.04f},
    // F4: 休息层前陡坡 (准备Boss)
    {1.35f,1.35f, 1.45f,1.35f, 1.95f, 1.25f,1.18f,1.20f,1.15f, 1.06f,1.06f},
    // F5: Boss1 — 暗影骑士
    {1.60f,1.50f, 2.00f,1.80f, 2.00f, 1.50f,1.30f,1.40f,1.20f, 1.08f,1.08f},
    // F6: 第二章开始 — 明显提升
    {1.85f,1.70f, 2.20f,2.00f, 2.05f, 1.60f,1.38f,1.50f,1.30f, 1.10f,1.10f},
    // F7: Shaman出现, 难度上升
    {2.10f,1.90f, 2.45f,2.20f, 2.10f, 1.70f,1.45f,1.58f,1.40f, 1.12f,1.12f},
    // F8: 窄通道高风险
    {2.40f,2.10f, 2.75f,2.45f, 2.15f, 1.80f,1.52f,1.65f,1.50f, 1.14f,1.14f},
    // F9: 休息层前
    {2.70f,2.35f, 3.05f,2.70f, 2.20f, 1.90f,1.60f,1.75f,1.60f, 1.16f,1.16f},
    // F10: Boss2 — 地狱火魔
    {3.10f,2.60f, 3.60f,3.20f, 2.25f, 2.20f,1.72f,1.95f,1.70f, 1.18f,1.18f},
    // F11: 第三章 — 大幅加速
    {3.50f,2.90f, 4.10f,3.60f, 2.30f, 2.35f,1.82f,2.05f,1.85f, 1.20f,1.20f},
    // F12: Elite+Bomber+Tank综合
    {4.00f,3.20f, 4.80f,4.00f, 2.35f, 2.50f,1.92f,2.15f,2.00f, 1.22f,1.22f},
    // F13: 疯狂难度
    {4.60f,3.55f, 5.50f,4.50f, 2.40f, 2.65f,2.02f,2.25f,2.15f, 1.24f,1.24f},
    // F14: 最终休息层
    {5.30f,4.00f, 6.40f,5.00f, 2.45f, 2.80f,2.15f,2.35f,2.30f, 1.26f,1.26f},
    // F15: 深渊之主·终焉
    {6.20f,4.50f, 8.00f,6.00f, 2.50f, 3.00f,2.30f,2.55f,2.50f, 1.28f,1.28f},
};

GrowthCurveSystem g_growth;

const GrowthCurve& GrowthCurveSystem::curve(int floor) const {
    if (floor < 1) return CURVES[0];
    if (floor > 15) return CURVES[14];
    return CURVES[floor - 1];
}

float GrowthCurveSystem::hp_scale(int floor) const       { return curve(floor).monster_hp; }
float GrowthCurveSystem::atk_scale(int floor) const      { return curve(floor).monster_atk; }
float GrowthCurveSystem::boss_hp_scale(int floor) const   { return curve(floor).boss_hp; }
float GrowthCurveSystem::boss_atk_scale(int floor) const  { return curve(floor).boss_atk; }
float GrowthCurveSystem::elite_scale(int floor) const     { return curve(floor).elite_scale; }
float GrowthCurveSystem::exp_scale(int floor) const       { return curve(floor).exp_scale; }
float GrowthCurveSystem::gold_scale(int floor) const      { return curve(floor).gold_scale; }
float GrowthCurveSystem::relic_scale(int floor) const     { return curve(floor).relic_scale; }
float GrowthCurveSystem::arena_scale(int floor) const     { return curve(floor).arena_scale; }
