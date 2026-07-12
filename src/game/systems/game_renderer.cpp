#include "game_renderer.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "item.h"
#include "combat_system.h"
#include "boss.h"
#include "config.h"
#include "vfx_server.h"
#include "boss.h"
#include "build_score.h"
#include "relic_progression.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

// 字体指针 (在 main.cpp 中初始化)
extern Font g_font;
extern Font g_font_small;
extern bool g_font_loaded;

// ============================================================
// 静态绘制工具
// ============================================================
void GameRenderer::draw_panel(Rectangle r, const char* title, Color bg) {
    DrawRectangleRounded(r, 0.08f, 8, bg);
    DrawRectangleRoundedLines(r, 0.08f, 8, 2, {100, 100, 180, 255});
    if (title && g_font_loaded)
        DrawTextEx(g_font_small, title, {r.x + 12, r.y + 8}, 18, 1, {200, 200, 255, 255});
}

void GameRenderer::draw_glow_text(const char* text, float x, float y, int size, Color c,
                                   bool center) {
    if (!g_font_loaded) return;
    float w = MeasureTextEx(g_font, text, (float)size, 1).x;
    if (center) x -= w / 2;
    DrawTextEx(g_font, text, {x + 1, y + 1}, size, 1, {0, 0, 0, 100});
    DrawTextEx(g_font, text, {x, y}, size, 1, c);
}

void GameRenderer::draw_progress_bar(Rectangle r, float ratio, Color fill, Color bg) {
    DrawRectangleRec(r, bg);
    DrawRectangleRec({r.x, r.y, r.width * ratio, r.height}, fill);
    DrawRectangleLinesEx(r, 1, {60, 60, 90, 255});
}

// ============================================================
// 摄像机
// ============================================================
void GameRenderer::update_camera(float& cam_x, float& cam_y, const Player* player,
                                  const GameMap* map, int screen_w, int screen_h) {
    if (!player) return;
    cam_x = player->entity.rect.x + player->entity.rect.width / 2 - screen_w / 2;
    cam_y = player->entity.rect.y + player->entity.rect.height / 2 - screen_h / 2;
    if (map) {
        cam_x = std::max(0.0f, std::min(cam_x, (float)map->pixel_width - screen_w));
        cam_y = std::max(0.0f, std::min(cam_y, (float)map->pixel_height - screen_h));
    }
}

// ============================================================
// 特效
// ============================================================
void GameRenderer::draw_effects(const std::vector<Effect>& effects, float cam_x, float cam_y) {
    for (auto& e : effects) {
        float sx = e.world_x - cam_x, sy = e.world_y - cam_y;
        float alpha = 1.0f - e.elapsed / e.duration;
        if (alpha <= 0) continue;
        Color c = e.color; c.a = (unsigned char)(c.a * alpha);

        if (e.kind == "pulse") {
            float r = e.radius * (0.5f + 0.5f * e.elapsed / e.duration);
            DrawRing({sx, sy}, r * 0.6f, r, 0, 360, 24, c);
        } else if (e.kind == "spark") {
            DrawCircle(sx, sy, e.radius, c);
        } else if (e.kind == "bolt") {
            DrawLineEx({sx, sy}, {e.target_x - cam_x, e.target_y - cam_y}, 2, c);
        } else if (e.kind == "flash") {
            DrawCircle(sx, sy, e.radius, Fade(c, 0.3f));
        } else if (e.kind == "slash_arc") {
            float arc_r = e.radius * (0.6f + 0.4f * e.elapsed / e.duration);
            float startAngle = 0;
            switch (e.direction) {
                case Direction::DOWN:  startAngle = 0;   break;
                case Direction::UP:    startAngle = 180; break;
                case Direction::RIGHT: startAngle = 270; break;
                case Direction::LEFT:  startAngle = 90;  break;
            }
            float endAngle = startAngle + 120;
            DrawRing({sx, sy}, arc_r * 0.4f, arc_r, startAngle, endAngle, 12, c);
            DrawLineEx({sx, sy},
                       {sx + cosf((startAngle + 60) * DEG2RAD) * arc_r,
                        sy + sinf((startAngle + 60) * DEG2RAD) * arc_r}, 3, c);
            for (int i = 0; i < 5; i++) {
                float a = (startAngle + (float)(rand() % 120)) * DEG2RAD;
                float dist = arc_r * (0.5f + (float)(rand() % 50) / 100.0f);
                DrawCircle(sx + cosf(a) * dist, sy + sinf(a) * dist, 2.5f, Fade(c, 0.5f));
            }
        } else if (e.kind == "cone") {
            DrawRectangleLines(sx - e.radius / 2, sy - e.radius / 4, e.radius, e.radius / 2, c);
        }
    }
}

// ============================================================
// 覆盖层
// ============================================================
void GameRenderer::draw_time_stop_overlay(int sw, int sh, float time_stop_remaining) {
    DrawRectangle(0, 0, sw, sh, {90, 90, 100, 130});
    int remain = (int)time_stop_remaining;
    char buf[4]; snprintf(buf, sizeof(buf), "%d", remain);
    draw_glow_text(buf, sw / 2.0f, sh / 2.0f, 80, WHITE, true);
    draw_glow_text("The World · 时停", sw / 2.0f, 60, 24, WHITE, true);
}

void GameRenderer::draw_boss_cinematic_overlay(int sw, int sh) {
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});
    draw_glow_text("BOSS 来了！", sw / 2.0f, sh / 2.0f, 48, {230, 50, 50, 255}, true);
}

void GameRenderer::draw_boss_intro(int sw, int sh, const std::string& title,
                                    const std::string& lore,
                                    const std::string& skills_text, Color color,
                                    int boss_floor) {
    ClearBackground(BLACK);
    float pw = 500, ph = 380;
    Rectangle pr = {sw / 2.0f - pw / 2, sh / 2.0f - ph / 2, pw, ph};
    draw_panel(pr, "! Boss 遭遇 !");

    draw_glow_text(title.c_str(), sw / 2.0f, pr.y + 45, 30, color, true);

    if (g_font_loaded) {
        auto info = get_boss_info(boss_floor);
        DrawTextEx(g_font_small, skills_text.c_str(), {pr.x + 80, pr.y + 160}, 16, 1,
                   {180, 180, 180, 255});
        DrawTextEx(g_font, lore.c_str(), {pr.x + 40, pr.y + 210}, 18, 1,
                   {160, 160, 180, 255});
    }
    draw_glow_text("按 Enter 进入战斗...", sw / 2.0f, (float)(sh - 60), 20,
                   {140, 20, 20, 255}, true);
}

void GameRenderer::draw_room_message(int sw, int sh, const std::string& msg, float timer) {
    if (timer <= 0 || msg.empty() || !g_font_loaded) return;

    // C1: 平滑淡出 (ease-out: quadratic)
    float raw = std::min(1.0f, timer / 0.6f);
    float alpha = raw * raw;  // ease-out curve
    bool is_relic = (msg.size() > 6 && msg.substr(0, 6) == "RELIC:");
    std::string display = is_relic ? msg.substr(6) : msg;

    float tw = MeasureTextEx(g_font_small, display.c_str(), 18, 1).x;
    float px = sw / 2.0f - tw / 2;
    float py = (float)(sh - 70);

    // C1: Relic 获得 — 金色背景
    Color bg = is_relic
        ? Color{40, 30, 10, (unsigned char)(200 * alpha)}
        : Color{10, 10, 20, (unsigned char)(180 * alpha)};
    Color fg = is_relic
        ? Color{255, 220, 60, (unsigned char)(255 * alpha)}
        : Color{255, 255, 200, (unsigned char)(255 * alpha)};
    Color border = is_relic
        ? Color{255, 200, 50, (unsigned char)(180 * alpha)}
        : Color{80, 80, 120, (unsigned char)(160 * alpha)};

    DrawRectangleRounded({px - 16, py - 4, tw + 32, 28}, 0.15f, 6, bg);
    DrawRectangleRoundedLines({px - 16, py - 4, tw + 32, 28}, 0.15f, 6, 1, border);
    DrawTextEx(g_font_small, display.c_str(), {px, py}, 18, 1, fg);
}

// ============================================================
// HUD 渲染
// ============================================================
Color GameRenderer::_relic_rarity_color(const std::string& rarity) {
    if (rarity == "rare")  return Color{100, 170, 255, 255};
    if (rarity == "epic") return Color{190, 100, 255, 255};
    return Color{255, 220, 100, 255}; // common
}

std::string GameRenderer::_rarity_label_cn(const std::string& rarity) {
    if (rarity == "rare")  return "稀有";
    if (rarity == "epic") return "史诗";
    return "普通";
}

void GameRenderer::draw_skill_bar(const Player* player, float game_time) {
    auto& active = player->skills.active_skills;
    if (active.empty() || !g_font_loaded) return;
    float x = 10, y = 56;
    for (int i = 0; i < (int)active.size(); i++) {
        float ry = y + i * 28;
        bool ready = active[i]->can_use(game_time);
        Color bg = ready ? Color{50, 160, 50, 255} : Color{60, 60, 60, 255};
        DrawRectangleRounded({x, ry, 22, 18}, 0.2f, 3, bg);
        DrawTextEx(g_font_small, std::to_string(i + 1).c_str(), {x + 7, ry + 1}, 14, 1, WHITE);

        // D3 Step2: Evolution 标签 (金色)
        std::string evo_tag;
        if (active[i]->evolution_level > 0) {
            evo_tag = " E" + std::to_string(active[i]->evolution_level);
        }
        std::string label = active[i]->name + " " + active[i]->get_level_text() + evo_tag;
        Color label_c = ready ? Color{180, 220, 255, 255} : Color{100, 100, 100, 255};
        if (active[i]->evolution_level > 0) label_c = ready ? Color{255, 200, 50, 255} : Color{140, 120, 50, 255};
        DrawTextEx(g_font_small, label.c_str(), {x + 26, ry + 2}, 14, 1, label_c);

        float cd_r = 1.0f - active[i]->remaining_cooldown(game_time) / active[i]->cooldown;
        draw_progress_bar({x + 26, ry + 16, 90, 8}, cd_r,
                          ready ? Color{60, 180, 255, 255} : Color{70, 70, 70, 255});
    }
}

// C1: Buff icon mapping
static const char* _buff_icon(const std::string& id) {
    if (id == "attack_up") return "▲";
    if (id == "poison")    return "☠";
    if (id == "slow")      return "❄";
    return "■";
}

void GameRenderer::draw_player_buffs(const Player* player) {
    if (!player || player->active_buffs.empty() || !g_font_loaded) return;
    auto& buffs = player->active_buffs;
    float x = 10;
    float y = 56.0f + player->skills.active_skills.size() * 28.0f + 4.0f;
    for (auto& b : buffs) {
        Color c = get_buff_hud_color(b.id);
        // C1: icon + name
        std::string line = std::string(_buff_icon(b.id)) + " "
            + get_buff_display_name(b.id)
            + " x" + std::to_string(b.stacks)
            + " " + format_buff_time(b.remaining);
        DrawTextEx(g_font_small, line.c_str(), {x, y}, 14, 1, c);
        y += 18;
    }
}

void GameRenderer::draw_player_relics(const Player* player) {
    if (!player || player->relics.empty() || !g_font_loaded) return;

    float x = 10;
    float base_y = 56.0f + player->skills.active_skills.size() * 28.0f + 4.0f;
    if (!player->active_buffs.empty())
        base_y += player->active_buffs.size() * 18.0f + 4.0f;

    DrawTextEx(g_font_small, "圣物:", {x, base_y}, 13, 1, Color{255, 220, 100, 255});
    float cx = x + MeasureTextEx(g_font_small, "圣物:", 13, 1).x + 4.0f;

    for (auto& r : player->relics) {
        const RelicDef* def = get_relic_def(r.id);
        if (!def) continue;
        Color rc = _relic_rarity_color(def->rarity);
        std::string token = def->short_name + std::string(" ");
        DrawTextEx(g_font_small, token.c_str(), {cx, base_y}, 13, 1, rc);
        cx += MeasureTextEx(g_font_small, token.c_str(), 13, 1).x;
    }
}

void GameRenderer::draw_relic_panel(const Player* player, int sw) {
    if (!player || !g_font_loaded) return;

    int count = (int)player->relics.size();
    float line_h = 24.0f;
    float panel_w = 370.0f;
    float panel_x = (float)sw - panel_w - 20.0f;
    float panel_y = 70.0f;
    // D4.6 Step4: 面板高度 + 收集率行
    float panel_h = 60.0f + (count > 0 ? count * line_h : line_h) + 22.0f;

    DrawRectangleRounded({panel_x, panel_y, panel_w, panel_h}, 0.08f, 8, Color{15, 15, 35, 220});
    DrawRectangleRoundedLines({panel_x, panel_y, panel_w, panel_h}, 0.08f, 8, 1.5f,
                              Color{100, 100, 160, 200});

    // D4.6 Step4: 标题行 + 收集进度
    int coll = g_relic_archive.collected_count();
    int total = g_relic_archive.total_relic_count();
    char title_buf[80];
    snprintf(title_buf, sizeof(title_buf), "圣物图鉴  %d/%d (%.0f%%)",
             coll, total, g_relic_archive.collection_pct() * 100.0f);
    DrawTextEx(g_font_small, title_buf, {panel_x + 14, panel_y + 10}, 16, 1,
               Color{255, 255, 200, 255});

    if (count == 0) {
        DrawTextEx(g_font_small, "本层尚未获得圣物。",
                   {panel_x + 14, panel_y + 40}, 14, 1, Color{160, 160, 180, 255});
        return;
    }

    float ly = panel_y + 38.0f;
    for (auto& r : player->relics) {
        const RelicDef* def = get_relic_def(r.id);
        if (!def) continue;

        Color rc = _relic_rarity_color(def->rarity);
        std::string label = "[" + _rarity_label_cn(def->rarity) + "]";
        // D4.6 Step4: mastery 星标
        int mlv = g_relic_archive.mastery_level(r.id);
        std::string mstars;
        for (int s = 0; s < mlv; s++) mstars += "★";
        char line[256];
        snprintf(line, sizeof(line), "%s %s %s - %s",
                 mstars.c_str(), label.c_str(), def->name.c_str(), def->desc.c_str());
        DrawTextEx(g_font_small, line, {panel_x + 14, ly}, 14, 1, rc);
        ly += line_h;
    }
}

void GameRenderer::draw_monster_buffs(const Monster& m, float draw_x, float draw_y) {
    if (m.active_buffs.empty() || !g_font_loaded) return;
    std::string label;
    int shown = 0;
    for (auto& b : m.active_buffs) {
        if (shown >= 3) break; // C1: 最多显示3个
        if (!label.empty()) label += " ";
        label += std::string(_buff_icon(b.id)) + "x" + std::to_string(b.stacks);
        shown++;
    }
    float tw = MeasureTextEx(g_font_small, label.c_str(), 12, 1).x;
    float px = draw_x + (m.entity.size.x - tw) / 2;
    float py = draw_y - 16;
    Color c = get_buff_hud_color(m.active_buffs[0].id);
    DrawTextEx(g_font_small, label.c_str(), {px + 1, py + 1}, 12, 1, {0, 0, 0, 180});
    DrawTextEx(g_font_small, label.c_str(), {px, py}, 12, 1, c);
}

void GameRenderer::draw_inventory_panel(const Player* player, int cursor, int sw, int sh) {
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 180});
    float pw = 440, ph = 480;
    Rectangle pr = {sw / 2.0f - pw / 2, sh / 2.0f - ph / 2, pw, ph};
    draw_panel(pr, "背包 I关闭");

    auto& inv = player->inventory;
    float y0 = pr.y + 40;

    std::string eq = "装备: weapon:";
    eq += inv.equipped.at("weapon") ? inv.equipped.at("weapon")->get_description() : "空";
    eq += " armor:";
    eq += inv.equipped.at("armor") ? inv.equipped.at("armor")->get_description() : "空";
    if (g_font_loaded)
        DrawTextEx(g_font_small, eq.c_str(), {pr.x + 30, y0}, 18, 1, {255, 200, 50, 255});
    DrawLine(pr.x + 30, y0 + 26, pr.x + pw - 30, y0 + 26, {60, 60, 90, 255});

    for (int i = 0; i < (int)inv.items.size(); i++) {
        float ry = y0 + 38 + i * 30;
        std::string mk = (i == cursor) ? ">" : " ";
        char idx[4]; snprintf(idx, sizeof(idx), "%2d", i + 1);
        std::string txt = mk + " [" + idx + "] " + inv.items[i]->get_description();
        if (g_font_loaded)
            DrawTextEx(g_font_small, txt.c_str(), {pr.x + 30, ry}, 18, 1, inv.items[i]->color);
    }
    if (g_font_loaded) {
        DrawTextEx(g_font_small, "^v择 X装 U用 D丢 I关",
                   {pr.x + (pw - 220) / 2, pr.y + ph - 30}, 16, 1, {140, 140, 140, 255});
    }
}

void GameRenderer::draw_hud(const Player* player, int current_floor, float game_time,
                             Monster* boss, bool show_relic_panel,
                             int inventory_open, int inventory_cursor,
                             const std::string& room_msg, float room_msg_timer,
                             int screen_w, int screen_h) {
    if (!player) return;
    auto& c = player->combat;

    // HP bar
    int eff_max_hp = get_effective_max_hp(player);
    float hp_r = eff_max_hp > 0 ? (float)c.current_hp / eff_max_hp : 0.0f;
    if (hp_r > 1.0f) hp_r = 1.0f;
    Color hp_c = hp_r > 0.5f ? Color{50, 200, 50, 255}
               : hp_r > 0.25f ? Color{200, 200, 50, 255} : Color{200, 50, 50, 255};
    draw_progress_bar({10, 10, 200, 16}, hp_r, hp_c, {40, 20, 20, 255});

    if (g_font_loaded) {
        char buf[128];
        snprintf(buf, sizeof(buf), "HP:%d/%d ATK:%d PD:%d MD:%d",
            c.current_hp, eff_max_hp, c.get_effective_attack(),
            c.get_effective_defense(AttackType::PHYSICAL),
            c.get_effective_defense(AttackType::MAGICAL));
        DrawTextEx(g_font_small, buf, {215, 10}, 16, 1, {220, 220, 220, 255});
    }

    // XP bar
    float xp_r = (float)player->xp / player->xp_to_next;
    draw_progress_bar({10, 30, 200, 10}, xp_r, {80, 120, 255, 255});

    if (g_font_loaded) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Lv%d XP:%d/%d", player->level, player->xp, player->xp_to_next);
        DrawTextEx(g_font_small, buf, {12, 42}, 13, 1, {180, 200, 255, 255});
    }

    // Floor
    if (g_font_loaded) {
        char buf[32];
        snprintf(buf, sizeof(buf), "第%d/%d层", current_floor, MAX_FLOORS);
        DrawTextEx(g_font_small, buf, {220, 42}, 16, 1, {200, 200, 50, 255});
    }

    // Boss HP bar (B15: 更大更明显 + C1: pulse + Phase2 标记)
    if (boss) {
        float bw = 500, bh = 24;
        float bx = screen_w / 2 - bw / 2, by = 4;
        Color bar_bg = {30, 5, 5, 255};
        Color bar_fg = {220, 100, 30, 255};
        // B15: Phase2 时血条颜色偏红
        auto* bai = dynamic_cast<const BossAI*>(boss->ai);
        if (bai && bai->phase2) {
            bar_fg = {255, 40, 20, 255};
            bar_bg = {50, 5, 5, 255};
        }
        // C1: Boss 血条微弱呼吸脉冲
        float glow = 1.0f + sinf((float)GetTime() * 3) * 0.03f;
        DrawRectangleLinesEx({bx - 2, by - 2, bw + 4, bh + 4}, 1.5f,
                             Color{220, 100, 30, (unsigned char)(60 * glow)});
        draw_progress_bar({bx, by, bw, bh},
            (float)boss->combat.current_hp / boss->combat.max_hp,
            bar_fg, bar_bg);
        if (g_font_loaded) {
            char buf[80];
            const char* phase_tag = (bai && bai->phase2) ? "[Phase2] " : "";
            snprintf(buf, sizeof(buf), "%s%s HP:%d/%d",
                phase_tag, boss->name.c_str(), boss->combat.current_hp, boss->combat.max_hp);
            float tw = MeasureTextEx(g_font_small, buf, 18, 1).x;
            DrawTextEx(g_font_small, buf, {bx + (bw - tw) / 2, by + bh + 3}, 18, 1,
                       {255, 220, 100, 255});
        }
    }

    draw_skill_bar(player, game_time);
    draw_player_buffs(player);
    draw_player_relics(player);
    // D3 Step4: Build 流派显示 (rellic下方)
    {
        BuildScore bs = calculate_build(player);
        BuildType bt = bs.identify();
        if (bt != BuildType::NONE && g_font_loaded) {
            float y = 56.0f + player->skills.active_skills.size() * 28.0f + 4.0f;
            if (!player->active_buffs.empty())
                y += player->active_buffs.size() * 18.0f + 4.0f;
            if (!player->relics.empty())
                y += 22.0f;
            char buf[64];
            snprintf(buf, sizeof(buf), "流派: %s", bs.build_name());
            DrawTextEx(g_font_small, buf, {12, y}, 12, 1, {255, 220, 100, 230});
        }
    }
    if (show_relic_panel) draw_relic_panel(player, screen_w);

    // Key hints
    if (g_font_loaded) {
        const char* hint = "[R]圣物  [I]背包  [ESC]保存";
        float hw = MeasureTextEx(g_font_small, hint, 12, 1).x;
        DrawTextEx(g_font_small, hint,
                   {screen_w - hw - 14.0f, (float)screen_h - 26.0f},
                   12, 1, Color{140, 140, 160, 220});
    }

    draw_room_message(screen_w, screen_h, room_msg, room_msg_timer);
}
