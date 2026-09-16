// ============================================================
// v1.6-B1.1: MirrorHudPanel — Mirror 记忆实时可视化
// 只读 MirrorAgent 缓存, 不修改任何游戏状态, 不产生 RNG
// ============================================================
#include "mirror_hud_panel.h"
#include "ai/mirror/mirror_agent.h"
#include "ai/mirror/mirror_debug_stats.h"
#include "ai/player_behavior/player_habit_profile.h"
#include "ai/player_behavior/player_action.h"
#include "raylib.h"
#include <cstdio>
#include <algorithm>

extern Font g_font_small;

namespace {
    const Color BG_HL     = {22,  9,  9, 220};
    const Color BORDER_HL = {130, 40, 40, 190};
    const Color TITLE_HL  = {230, 120, 110, 255};
    const Color LABEL_HL  = {195, 140, 135, 240};
    const Color HINT_HL   = {160, 100,  90, 220};
    const Color GOLD_HL   = {255, 200,  50, 255};
    const Color ACCENT_HL = {255, 140, 100, 255};
    const Color BAR_BG_HL = {50,  20,  20, 255};
    const Color BAR_FILL_HL = {180,  80,  80, 255};
    const Color LEAF_FILL_HL = {180, 140, 100, 160};

    const char* skill_tag(int i) {
        static const char* L[4] = {"SL", "FB", "SH", "TW"};
        return L[i];
    }
    const char* habit_tag(int i) {
        static const char* L[4] = {"ATK", "SKL", "RET", "APP"};
        return L[i];
    }
    const char* action_tag(PlayerActionType a) {
        switch (a) {
            case PlayerActionType::ATTACK: return "ATTACK";
            case PlayerActionType::SKILL:  return "SKILL";
            case PlayerActionType::DODGE:  return "DODGE";
            case PlayerActionType::HEAL:   return "HEAL";
            default:                       return "----";
        }
    }
    Color tint(Color c, float a) {
        int al = (int)(a * 255.0f);
        al = al < 0 ? 0 : (al > 255 ? 255 : al);
        return Color{c.r, c.g, c.b, (unsigned char)al};
    }
    float reveal_alpha(float t) {
        if (t < 0.0f)   return 0.0f;
        if (t < 0.5f)   return t / 0.5f;
        if (t < 5.5f)   return 1.0f;
        if (t < 6.5f)   return 1.0f - (t - 5.5f);
        return 0.0f;
    }
}

void MirrorHudPanel::render_analysis_card(const MirrorAgent& agent,
                                          int screen_w, float /*game_time*/) {
    float bt = agent.battle_seconds();
    float reveal = reveal_alpha(bt);
    if (reveal <= 0.001f) return;
    const float w = 500.0f, h = 88.0f;
    float px = (float)(screen_w - (int)w) / 2.0f;
    float py = 4.0f - (bt > 5.5f ? (1.0f - reveal) * 6.0f : 0.0f);
    DrawRectangleRounded({px, py, w, h}, 0.10f, 4, tint(BG_HL, reveal * 0.90f));
    DrawRectangleRoundedLines({px, py, w, h}, 0.10f, 4, 1.0f, tint(BORDER_HL, reveal));
    DrawTextEx(g_font_small, "MIRROR ANALYSIS", {px + 8, py + 4}, 12, 1,
               tint(TITLE_HL, reveal));
    DrawTextEx(g_font_small, "战斗习惯", {px + 246, py + 4}, 12, 1,
               tint(TITLE_HL, reveal));
    DrawTextEx(g_font_small, "基于本局观察 · 非角色属性",
               {px + 246, py + 22}, 9, 1, tint(HINT_HL, reveal));
    _skill_bars(agent, px + 10, py + 22, 220, 58);
    _habit_leaves(agent, px + 246, py + 34, 246, 46);
}

// 4 柱技能偏好; 归一化到最大值; 本命技能金框
void MirrorHudPanel::_skill_bars(const MirrorAgent& agent, float px, float py,
                                 float w, float h) {
    const auto& sp = agent.profile().skill_preference;
    float fav = (float)agent.profile().predicted_fav_skill;
    float mx = 0.01f;
    for (int i = 0; i < 4; i++) mx = std::max(mx, sp[i]);
    float cell = w / 4.0f;
    float bar_h = h - 22.0f;
    float alpha = reveal_alpha(agent.battle_seconds());
    for (int i = 0; i < 4; i++) {
        float cx = px + i * cell + cell * 0.18f;
        float cw = cell * 0.64f;
        float ratio = sp[i] / mx;
        DrawRectangleRec({cx, py, cw, bar_h}, tint(BAR_BG_HL, alpha));
        float fill_h = bar_h * ratio;
        Color fc = ((float)i == fav && fav >= 0.0f) ? GOLD_HL : BAR_FILL_HL;
        DrawRectangleRec({cx, py + (bar_h - fill_h), cw, fill_h},
                         tint(fc, alpha));
        DrawTextEx(g_font_small, skill_tag(i), {cx + 1, py + bar_h + 3}, 9, 1,
                   tint(((float)i == fav) ? GOLD_HL : LABEL_HL, alpha));
    }
}

// 4 叶战斗习惯 — 2x2 grid 方块, 尺寸=归一化行为帧数 (非角色属性雷达)
void MirrorHudPanel::_habit_leaves(const MirrorAgent& agent, float px, float py,
                                   float w, float h) {
    const MirrorDebugStats* ds = agent.debug_stats();
    if (!ds) return;
    auto s = ds->snapshot();
    float vals[4] = {(float)s.behavior_attack, (float)s.behavior_skill,
                     (float)s.behavior_retreat, (float)s.behavior_approach};
    float total = vals[0] + vals[1] + vals[2] + vals[3];
    float mx = std::max(1.0f, std::max(std::max(vals[0], vals[1]),
                                       std::max(vals[2], vals[3])));
    float cw = w / 2.0f, ch = h / 2.0f;
    float alpha = reveal_alpha(agent.battle_seconds());
    static const float BOX[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    for (int i = 0; i < 4; i++) {
        float bx = px + BOX[i][0] * cw;
        float by = py + BOX[i][1] * ch;
        float ratio = vals[i] / mx;
        float box_side = (ch - 14.0f) * std::max(0.15f, ratio);
        float off = ((ch - 14.0f) - box_side) * 0.5f;
        DrawRectangleRec({bx + off, by + off, box_side, box_side},
                         tint(LEAF_FILL_HL, alpha));
        char buf[32];
        int pct = total > 0 ? (int)(vals[i] / total * 100.0f) : 0;
        snprintf(buf, sizeof(buf), "%s %d%%", habit_tag(i), pct);
        DrawTextEx(g_font_small, buf, {bx + 2, by + ch - 14.0f}, 9, 1,
                   tint(LABEL_HL, alpha));
    }
}

// ── 顶部观察卡 (analysis reveal 结束后接手, 常驻 340x54) ──
void MirrorHudPanel::render_observation_card(const MirrorAgent& agent,
                                             float px, float py,
                                             float game_time) {
    const float w = 340.0f, h = 54.0f;
    DrawRectangleRounded({px, py, w, h}, 0.12f, 4, {22, 9, 9, 220});
    DrawRectangleRoundedLines({px, py, w, h}, 0.12f, 4, 1.0f, BORDER_HL);
    DrawTextEx(g_font_small, "它猜你下一步", {px + 8, py + 4}, 12, 1, TITLE_HL);
    _prediction_row(agent, px + 96, py + 4, game_time);
    _rhythm_row(agent, px + 8, py + 28);
}

// 预测行: 只读 agent 缓存; >2s 视为陈旧, 显示"上次预测 (Ns前)"; >200s 显示"观察中…"
void MirrorHudPanel::_prediction_row(const MirrorAgent& agent, float px, float py,
                                     float game_time) {
    float age = agent.last_pred_age(game_time);
    char buf[64];
    if (age > 200.0f) {
        snprintf(buf, sizeof(buf), "观察中…");
        DrawTextEx(g_font_small, buf, {px, py}, 11, 1, HINT_HL);
        return;
    }
    PlayerActionType a = agent.last_pred_action();
    int conf = (int)(agent.last_pred_conf() * 100.0f);
    if (age > 2.0f) {
        snprintf(buf, sizeof(buf), "%s %d%%  (%ds前)",
                 action_tag(a), conf, (int)age);
        DrawTextEx(g_font_small, buf, {px, py}, 11, 1, HINT_HL);
    } else {
        snprintf(buf, sizeof(buf), "→ %s  %d%%", action_tag(a), conf);
        DrawTextEx(g_font_small, buf, {px, py}, 12, 1, GOLD_HL);
    }
}

// 本局节奏行: 4 数字/秒 + 战斗秒; 前 3s 数据不足显示"采样中…"
void MirrorHudPanel::_rhythm_row(const MirrorAgent& agent, float px, float py) {
    float bt = agent.battle_seconds();
    char buf[96];
    if (bt < 3.0f) {
        snprintf(buf, sizeof(buf), "本局节奏 · 采样中…");
        DrawTextEx(g_font_small, buf, {px, py}, 10, 1, HINT_HL);
        return;
    }
    float denom = std::max(1.0f, bt);
    snprintf(buf, sizeof(buf), "本局节奏 A%.1f S%.1f D%.1f H%.1f · %ds",
             agent.obs_attack_count() / denom,
             agent.obs_skill_count()  / denom,
             agent.obs_dodge_count()  / denom,
             agent.obs_heal_count()   / denom,
             (int)bt);
    DrawTextEx(g_font_small, buf, {px, py}, 10, 1, ACCENT_HL);
}
