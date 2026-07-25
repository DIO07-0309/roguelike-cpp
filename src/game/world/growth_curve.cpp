#include "growth_curve.h"
#include <cmath>

// ============================================================
// G9: 统一成长曲线 — 每层 HP/ATK ×1.15，F1=1.00
// DEF 不随楼层增长 (三连击需要怪能扛住但不至于打不动)
// ============================================================

static const GrowthCurve CURVES[15] = {
    // F1
    {1.00f,1.00f, 2.20f,2.00f, 1.80f, 1.00f,1.00f,1.00f,1.00f, 1.00f,1.00f},
    // F2
    {1.15f,1.15f, 2.40f,2.20f, 1.85f, 1.05f,1.05f,1.05f,1.05f, 1.02f,1.02f},
    // F3
    {1.32f,1.32f, 2.65f,2.45f, 1.90f, 1.10f,1.10f,1.10f,1.10f, 1.04f,1.04f},
    // F4
    {1.52f,1.52f, 2.95f,2.70f, 1.95f, 1.18f,1.18f,1.20f,1.15f, 1.06f,1.06f},
    // F5: Boss1
    {1.80f,1.70f, 3.50f,3.20f, 2.00f, 1.30f,1.30f,1.40f,1.20f, 1.08f,1.08f},
    // F6
    {1.75f,1.75f, 3.80f,3.50f, 2.05f, 1.38f,1.38f,1.50f,1.30f, 1.10f,1.10f},
    // F7
    {2.01f,2.01f, 4.10f,3.80f, 2.10f, 1.45f,1.45f,1.58f,1.40f, 1.12f,1.12f},
    // F8
    {2.31f,2.31f, 4.45f,4.15f, 2.15f, 1.52f,1.52f,1.65f,1.50f, 1.14f,1.14f},
    // F9
    {2.66f,2.66f, 4.85f,4.55f, 2.20f, 1.60f,1.60f,1.75f,1.60f, 1.16f,1.16f},
    // F10: Boss2
    {3.10f,3.00f, 5.50f,5.00f, 2.25f, 1.72f,1.72f,1.95f,1.70f, 1.18f,1.18f},
    // F11
    {3.06f,3.06f, 5.90f,5.40f, 2.30f, 1.82f,1.82f,2.05f,1.85f, 1.20f,1.20f},
    // F12
    {3.52f,3.52f, 6.40f,5.90f, 2.35f, 1.92f,1.92f,2.15f,2.00f, 1.22f,1.22f},
    // F13
    {4.05f,4.05f, 7.00f,6.50f, 2.40f, 2.02f,2.02f,2.25f,2.15f, 1.24f,1.24f},
    // F14
    {4.66f,4.66f, 7.80f,7.20f, 2.45f, 2.15f,2.15f,2.35f,2.30f, 1.26f,1.26f},
    // F15
    {5.50f,5.00f, 9.50f,8.00f, 2.50f, 2.30f,2.30f,2.55f,2.50f, 1.28f,1.28f},
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
