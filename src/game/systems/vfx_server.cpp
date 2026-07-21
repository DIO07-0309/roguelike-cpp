#include "vfx_server.h"
#include "data/vfx_recipe.h"  // G5.8.5
#include <cmath>
#include <algorithm>

static Effect _ef(const char* k, float x, float y, float r, Color c, float d) {
    return {k, x, y, r, c, d, 0};
}

void VFXServer::update(float dt) {
    for (auto& e : effects) e.elapsed += dt;
    effects.erase(std::remove_if(effects.begin(), effects.end(),
        [](auto& e) { return e.elapsed >= e.duration; }), effects.end());
}

void VFXServer::draw(float cam_x, float cam_y) const {
    for (auto& e : effects) {
        float sx = e.world_x - cam_x, sy = e.world_y - cam_y;
        float alpha = 1.0f - e.elapsed / e.duration;
        if (alpha < 0) alpha = 0;
        Color c = e.color; c.a = (unsigned char)(c.a * alpha);

        if (e.kind == "ring") {
            float r = e.radius * e.elapsed / e.duration;
            DrawRing({sx, sy}, r * 0.85f, r, 0, 360, 24, c);
        } else if (e.kind == "spark") {
            DrawCircle(sx, sy, e.radius, c);
        } else if (e.kind == "bolt") {
            DrawLineEx({sx, sy}, {e.target_x - cam_x, e.target_y - cam_y}, 2.0f, c);
        } else if (e.kind == "flash") {
            DrawCircle(sx, sy, e.radius, c);
            DrawCircle(sx, sy, e.radius * 1.5f, Fade(c, 0.3f));
        } else if (e.kind == "smoke") {
            float r = e.radius * (0.3f + 0.7f * e.elapsed / e.duration);
            DrawCircle(sx, sy, r, Fade(c, 0.5f));
        } else if (e.kind == "shield_ring") {
            float pulse = 0.8f + 0.2f * sinf(e.elapsed * 8.0f);
            DrawRing({sx, sy}, e.radius * 0.7f, e.radius, 0, 360, 16,
                     Color{(unsigned char)c.r,(unsigned char)c.g,(unsigned char)c.b,(unsigned char)(c.a * pulse)});
        } else if (e.kind == "slash_arc") {
            // Directional arc: half-ring in facing direction
            float r = e.radius * (0.5f + 0.5f * e.elapsed / e.duration);
            int segs = 8;
            switch (e.direction) {
                case Direction::RIGHT: DrawRing({sx, sy}, r*0.5f, r, 315, 45, segs, c); break;
                case Direction::LEFT:  DrawRing({sx, sy}, r*0.5f, r, 135, 225, segs, c); break;
                case Direction::DOWN:  DrawRing({sx, sy}, r*0.5f, r, 45, 135, segs, c); break;
                case Direction::UP:    DrawRing({sx, sy}, r*0.5f, r, 225, 315, segs, c); break;
            }
        } else {
            // Generic: pulse/circle — expanding ring
            float r = e.radius * (0.5f + 0.5f * (e.elapsed / e.duration));
            DrawRing({sx, sy}, r * 0.6f, r, 0, 360, 12, c);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// G5.8.1 — 9 Composable VFX Primitives
// ═══════════════════════════════════════════════════════════════

void VFXServer::ring(float cx, float cy, float radius, Color c, int layers, float dur) {
    for (int i = 0; i < layers; i++)
        effects.push_back(_ef("ring", cx, cy, radius * (0.5f + i * 0.25f),
                              Color{c.r,c.g,c.b,(unsigned char)(c.a - (unsigned char)(i*40))}, dur));
}

void VFXServer::beam(float sx, float sy, float tx, float ty, Color c, float dur) {
    effects.push_back({"bolt", sx, sy, 0, c, dur, 0, Direction::DOWN, tx, ty, 1});
}

void VFXServer::lightning(float sx, float sy, float tx, float ty, int branches, Color c, float dur) {
    // Main bolt
    effects.push_back({"bolt", sx, sy, 0, c, dur, 0, Direction::DOWN, tx, ty, 1});
    // Jagged offset lines for electricity effect
    for (int j = 0; j < branches; j++) {
        float off = (float)((int)rng() % 12 - 6);
        Color dim = {c.r, c.g, c.b, (unsigned char)(c.a / 2)};
        effects.push_back({"bolt", sx + off, sy + off, 0, dim, dur * 0.6f, 0, Direction::DOWN, tx + off, ty + off});
    }
}

void VFXServer::explosion(float cx, float cy, float radius, Color c, int count, float dur) {
    ring(cx, cy, radius * 0.6f, c, 2, dur);
    for (int i = 0; i < count; i++) {
        float a = (float)(rng() % 360) * DEG2RAD;
        float d = radius * (0.3f + (float)(rng() % 70) / 100.0f);
        effects.push_back(_ef("spark", cx + cosf(a) * d, cy + sinf(a) * d,
                              2.0f + (float)(rng() % 4), c, dur * 0.8f));
    }
}

void VFXServer::shockwave(float cx, float cy, float radius, Color c, int layers, float dur) {
    ring(cx, cy, radius, c, layers, dur);
    effects.push_back(_ef("flash", cx, cy, radius * 0.4f,
                          Color{c.r,c.g,c.b,(unsigned char)(c.a / 3)}, dur * 0.5f));
}

void VFXServer::slash_arc(float cx, float cy, Direction dir, float radius, Color c, float dur) {
    effects.push_back({"slash_arc", cx, cy, radius, c, dur, 0, dir});
}

void VFXServer::smoke_puff(float cx, float cy, float radius, Color c, int count, float dur) {
    for (int i = 0; i < count; i++)
        effects.push_back(_ef("smoke", cx + (float)(rng()%24 - 12), cy + (float)(rng()%24 - 12),
                              radius * (0.5f + (float)(rng()%50)/100.0f), c, dur));
}

void VFXServer::spark_burst(float cx, float cy, int count, Color c, float dur) {
    for (int i = 0; i < count; i++) {
        float a = (float)(rng() % 360) * DEG2RAD;
        float d = 5.0f + (float)(rng() % 28);
        effects.push_back(_ef("spark", cx + cosf(a) * d, cy + sinf(a) * d,
                              1.5f + (float)(rng() % 4), c, dur));
    }
}

void VFXServer::aura_ring(float cx, float cy, float radius, Color c, float dur) {
    effects.push_back({"shield_ring", cx, cy, radius, c, dur, 0});
    spark_burst(cx, cy, 6, Color{c.r,c.g,c.b,(unsigned char)(c.a / 2)}, dur * 0.7f);
}

void VFXServer::flash(float cx, float cy, float radius, Color c, float dur) {
    effects.push_back(_ef("flash", cx, cy, radius, c, dur));
}

// ═══════════════════════════════════════════════════════════════
// G5.8.5: Color preset + Recipe dispatcher
// ═══════════════════════════════════════════════════════════════

Color VFXServer::preset_color(const std::string& name) {
    // skill build colors
    if (name == "ice")        return {100, 200, 255, 200};
    if (name == "fire")       return {255, 120, 30, 200};
    if (name == "lightning")  return {200, 220, 255, 240};
    if (name == "blood")      return {220, 30, 50, 220};
    if (name == "shadow")     return {80, 30, 160, 200};
    if (name == "nature")     return {100, 255, 100, 200};
    if (name == "holy")       return {255, 220, 80, 200};
    if (name == "void")       return {80, 20, 120, 200};
    if (name == "gold")       return {255, 200, 50, 200};
    if (name == "red")        return {255, 80, 80, 200};
    if (name == "white")      return {200, 200, 200, 160};
    return {200, 200, 200, 200};
}

void VFXServer::play_recipe(const char* recipe_id, float cx, float cy,
                             Direction dir, float tx, float ty, int level) {
    const VFXRecipe* recipe = get_vfx_recipe(recipe_id);
    if (!recipe) {
        // fallback: generic slash + spark
        slash_arc(cx, cy, dir, 56.0f, {255, 80, 80, 200});
        spark_burst(cx, cy, 6, {255, 120, 80, 255}, 0.30f);
        return;
    }

    for (auto& step : recipe->steps) {
        Color c = step.color_preset.empty()
                  ? Color{200, 200, 200, 200}
                  : preset_color(step.color_preset);
        int cnt = step.count + level / 2; // level scales count

        if (step.type == "ring")
            ring(cx, cy, step.radius, c, cnt, step.duration);
        else if (step.type == "beam") {
            float btx = tx ? tx : cx + cosf(step.direction_rad) * step.target_dist;
            float bty = ty ? ty : cy + sinf(step.direction_rad) * step.target_dist;
            beam(cx, cy, btx, bty, c, step.duration);
        }
        else if (step.type == "lightning") {
            float ltx = tx ? tx : cx + cosf(step.direction_rad) * step.target_dist;
            float lty = ty ? ty : cy + sinf(step.direction_rad) * step.target_dist;
            lightning(cx, cy, ltx, lty, cnt, c, step.duration);
        }
        else if (step.type == "explosion")
            explosion(cx, cy, step.radius, c, cnt, step.duration);
        else if (step.type == "shockwave")
            shockwave(cx, cy, step.radius, c, cnt, step.duration);
        else if (step.type == "slash_arc")
            slash_arc(cx, cy, dir, step.radius, c, step.duration);
        else if (step.type == "smoke")
            smoke_puff(cx, cy, step.radius, c, cnt, step.duration);
        else if (step.type == "spark")
            spark_burst(cx, cy, cnt, c, step.duration);
        else if (step.type == "aura")
            aura_ring(cx, cy, step.radius, c, step.duration);
        else if (step.type == "flash")
            flash(cx, cy, step.radius, c, step.duration);
    }
}

// ═══════════════════════════════════════════════════════════════
// Backward-compat wrappers — all rewritten as primitive compositions
// ═══════════════════════════════════════════════════════════════

void VFXServer::player_attack(float cx, float cy, float range, const AttackEvolutionState& evo) {
    Color c = (evo.level == 1) ? Color{255,200,50,200}
            : (evo.level == 2) ? Color{255,150,40,220} : Color{255,200,40,240};
    int n = (evo.level == 1) ? 4 : (evo.level == 2) ? 6 : 10;
    ring(cx, cy, range * (0.6f + evo.level * 0.3f), c, 1, 0.20f + evo.level * 0.05f);
    spark_burst(cx, cy, n, {255,220,100,255}, 0.25f);
    if (evo.level >= 3) flash(cx, cy, range * 0.8f, {255,220,60,100}, 0.20f);
}

void VFXServer::slash_skill(float cx, float cy, Direction dir, int level) {
    float r = level == 1 ? 56.0f : level == 2 ? 72.0f : 88.0f;
    slash_arc(cx, cy, dir, r, {255,80,80,200});
    if (level >= 3) spark_burst(cx, cy, 8, {255,120,80,255}, 0.35f);
}

void VFXServer::fireball(float cx, float cy, float tx, float ty, int level) {
    beam(cx, cy, tx, ty, {255,100,50,200}, 0.4f);
    for (int i = 0; i < level; i++)
        ring(tx, ty, 16.0f + i * 8.0f, {255,180,60,150}, 1, 0.35f + i * 0.1f);
}

void VFXServer::heal(float cx, float cy, int) {
    ring(cx, cy, 40, {100,255,100,180}, 1, 0.5f);
    spark_burst(cx, cy, 6, {150,255,150,255}, 0.4f);
}

void VFXServer::monster_attack(float mx, float my, float px, float py, Color c) {
    beam(mx, my, px, py, c, 0.25f);
}

void VFXServer::boss_cone(float cx, float cy)   { ring(cx, cy, 96, {200,40,40,200}, 1, 0.5f); }
void VFXServer::boss_circle(float cx, float cy)  { ring(cx, cy, 80, {220,50,50,200}, 1, 0.5f); }
void VFXServer::boss_summon(float cx, float cy)  { ring(cx, cy, 64, {150,50,200,200}, 1, 0.5f); }
void VFXServer::time_stop(float cx, float cy)    { ring(cx, cy, 200, {180,180,200,150}, 1, 0.8f); }
void VFXServer::hit_flash(float x, float y, float s) { flash(x, y, s*0.8f, {255,255,255,200}, 0.15f); }

// ── G5.8 Signature Skills ──

void VFXServer::ice_nova(float cx, float cy, float radius, int level) {
    shockwave(cx, cy, radius, {100,200,255,200}, 3, 0.50f);
    spark_burst(cx, cy, 12 + level * 3, {200,230,255,255}, 0.35f);
    explosion(cx, cy, radius * 0.6f, {180,240,255,220}, 6, 0.40f);
}

void VFXServer::chain_lightning(float sx, float sy, float tx, float ty, int bounces) {
    lightning(sx, sy, tx, ty, 3, {200,220,255,240}, 0.25f);
    flash(tx, ty, 20.0f, {200,220,255,180}, 0.15f);
    spark_burst(tx, ty, 4, {180,200,255,255}, 0.20f);
}

void VFXServer::shadow_strike(float fx, float fy, float tx, float ty, int level) {
    smoke_puff(tx, ty, 14.0f, {60,20,120,180}, 3 + level, 0.30f);
    slash_arc(tx, ty, Direction::DOWN, 48.0f, {140,40,220,200}, 0.35f);
    flash(tx, ty, 28.0f, {180,60,255,150}, 0.20f);
    spark_burst(tx, ty, 6, {120,40,200,200}, 0.30f);
}

void VFXServer::blood_frenzy(float cx, float cy, float radius, int hit_count) {
    ring(cx, cy, radius, {200,30,40,180}, 1, 0.40f);
    for (int i = 0; i < 15 + hit_count * 3; i++) {
        float a = (float)(rng() % 360) * DEG2RAD;
        float d = radius * (float)(rng() % 100) / 100.0f;
        effects.push_back(_ef("spark", cx + cosf(a) * d, cy + sinf(a) * d, 2.5f, {220,30,50,220}, 0.35f));
    }
    for (int i = 0; i < hit_count; i++)
        effects.push_back(_ef("spark", cx - 20 + (float)(rng()%40), cy - 30, 4.0f, {100,255,100,220}, 0.45f));
}

void VFXServer::summon_spirit(float cx, float cy, int count) {
    ring(cx, cy, 60.0f, {120,180,255,180}, 2, 0.60f);
    ring(cx, cy, 40.0f, {150,200,255,160}, 1, 0.50f);
    spark_burst(cx, cy, count * 6, {180,200,255,200}, 0.50f);
}

// ── G5.8 Enemy Archetype VFX ──

void VFXServer::sniper_line(float sx, float sy, float tx, float ty) {
    beam(sx, sy, tx, ty, {255,60,30,200}, 0.8f);
    ring(tx, ty, 16.0f, {255,40,20,180}, 1, 0.8f);
}

void VFXServer::controller_zone(float x, float y, float radius) {
    ring(x, y, radius, {180,50,200,160}, 3, 0.70f);
    flash(x, y, radius * 0.3f, {200,60,200,120}, 0.50f);
}

void VFXServer::ambush_smoke(float x, float y) {
    smoke_puff(x, y, 16.0f, {40,20,60,140}, 5, 0.60f);
}

void VFXServer::guardian_aura_enemy(float cx, float cy, float radius) {
    aura_ring(cx, cy, radius, {60,140,255,200}, 0.80f);
}

// ── G5.8 Boss Phase2 VFX ──

void VFXServer::boss_phase2_flash(float cx, float cy, Color tint) {
    flash(cx, cy, 300.0f, {tint.r,tint.g,tint.b,180}, 0.60f);
    shockwave(cx, cy, 100.0f, {tint.r,tint.g,tint.b,200}, 4, 0.55f);
    explosion(cx, cy, 80.0f, {tint.r,tint.g,tint.b,240}, 20, 0.50f);
}

void VFXServer::boss_gravity_pull(float cx, float cy, float px, float py) {
    ring(cx, cy, 60.0f, {80,20,120,200}, 1, 0.70f);
    beam(px, py, cx, cy, {120,40,180,160}, 0.40f);
    spark_burst(cx, cy, 12, {140,60,200,200}, 0.45f);
}
