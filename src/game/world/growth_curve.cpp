#include "growth_curve.h"
#include <cmath>

// ============================================================
// G9.4 数值平衡: monster_hp ×1.25/floor, monster_atk ×1.18/floor
// Boss HP 大幅提升 (×1.5~2.0 原值)，DEF 有节制增长
// F1 保持不变 (教学层), F6+/F11+ 加速
// ============================================================

static const GrowthCurve CURVES[15] = {
    // hp   atk  boss_hp boss_atk elite exp gold relic arena ph_sr
    // F1: 教学层
    {1.00f,1.00f, 2.50f, 2.00f, 2.00f, 1.00f,1.00f,1.00f,1.00f, 1.00f,1.00f},
    // F2
    {1.20f,1.15f, 2.80f, 2.25f, 2.10f, 1.05f,1.05f,1.05f,1.05f, 1.02f,1.02f},
    // F3
    {1.44f,1.32f, 3.15f, 2.55f, 2.20f, 1.10f,1.10f,1.10f,1.10f, 1.04f,1.04f},
    // F4
    {1.73f,1.52f, 3.55f, 2.90f, 2.30f, 1.18f,1.18f,1.20f,1.15f, 1.06f,1.06f},
    // F5: Boss1
    {2.20f,2.00f, 4.50f, 3.80f, 2.40f, 1.30f,1.30f,1.40f,1.20f, 1.08f,1.08f},
    // F6: 第二章
    {2.35f,2.10f, 5.00f, 4.20f, 2.50f, 1.45f,1.40f,1.55f,1.35f, 1.12f,1.12f},
    // F7
    {2.80f,2.40f, 5.60f, 4.70f, 2.60f, 1.55f,1.50f,1.65f,1.45f, 1.16f,1.16f},
    // F8
    {3.30f,2.75f, 6.30f, 5.30f, 2.70f, 1.65f,1.60f,1.75f,1.55f, 1.20f,1.20f},
    // F9
    {3.90f,3.15f, 7.00f, 5.90f, 2.80f, 1.75f,1.70f,1.85f,1.65f, 1.24f,1.24f},
    // F10: Boss2
    {4.50f,3.60f, 8.00f, 6.50f, 2.90f, 1.90f,1.85f,2.00f,1.80f, 1.28f,1.28f},
    // F11: 第三章
    {5.00f,4.00f, 9.00f, 7.20f, 3.00f, 2.10f,2.00f,2.20f,2.00f, 1.32f,1.32f},
    // F12
    {5.80f,4.50f, 10.0f, 8.00f, 3.10f, 2.25f,2.15f,2.35f,2.15f, 1.36f,1.36f},
    // F13
    {6.70f,5.10f, 11.5f, 9.00f, 3.20f, 2.40f,2.30f,2.50f,2.30f, 1.40f,1.40f},
    // F14
    {7.70f,5.80f, 13.0f, 10.0f, 3.30f, 2.55f,2.45f,2.65f,2.45f, 1.44f,1.44f},
    // F15: 深渊之主
    {9.00f,6.50f, 15.0f, 11.0f, 3.50f, 2.70f,2.60f,2.80f,2.60f, 1.48f,1.48f},
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
