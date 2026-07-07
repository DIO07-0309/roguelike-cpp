// ============================================================
// CombatSystem - Buff + find_attack_target implementation
// ============================================================
#include "combat_system.h"
#include "player.h"
#include "monster.h"
#include "ai.h"
#include "core/logger.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <unordered_map>
#include <cstdio>
#include <cstring>

std::mt19937 rng(std::random_device{}());

// ---- Buff config table (populated by load_buff_defs) ----
static std::unordered_map<std::string, BuffDef> g_buff_defs;
static bool g_buff_defs_loaded = false;

// ============================================================
// Minimal JSON loader for buffs.json
// ============================================================

static const char* _skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static std::string _read_string(const char*& p) {
    p = _skip_ws(p);
    if (*p != '"') return "";
    p++;
    std::string out;
    while (*p && *p != '"') { out += *p; p++; }
    if (*p == '"') p++;
    return out;
}

static float _read_float(const char*& p) {
    p = _skip_ws(p);
    char* end;
    float v = (float)strtod(p, &end);
    if (end > p) p = end;
    return v;
}

static int _read_int(const char*& p) {
    p = _skip_ws(p);
    char* end;
    int v = (int)strtol(p, &end, 10);
    if (end > p) p = end;
    return v;
}

static std::string _parse_buff_obj(const char*& p, BuffDef& out) {
    p = _skip_ws(p);
    if (*p != '{') return "expected '{'";
    p++;
    while (*p) {
        p = _skip_ws(p);
        if (*p == '}') { p++; return ""; }
        if (*p == ',') { p++; continue; }

        std::string key = _read_string(p);
        p = _skip_ws(p);
        if (*p != ':') return "expected ':' after " + key;
        p++;

        if (key == "id")              out.id = _read_string(p);
        else if (key == "display_name") out.display_name = _read_string(p);
        else if (key == "short_name")   out.short_name = _read_string(p);
        else if (key == "duration")     out.duration = _read_float(p);
        else if (key == "max_stacks")   out.max_stacks = _read_int(p);
        else if (key == "tick_interval") out.tick_interval = _read_float(p);
        else if (key == "tick_damage")   out.tick_damage = _read_int(p);
        else if (key == "hud_color") {
            p = _skip_ws(p);
            if (*p != '[') return "expected '['";
            p++;
            out.hud_color_r = (unsigned char)_read_int(p);
            p = _skip_ws(p); if (*p == ',') p++;
            out.hud_color_g = (unsigned char)_read_int(p);
            p = _skip_ws(p); if (*p == ',') p++;
            out.hud_color_b = (unsigned char)_read_int(p);
            p = _skip_ws(p);
            if (*p == ']') p++;
        } else {
            p = _skip_ws(p);
            if (*p == '"') { _read_string(p); }
            else if (*p == '[') { while (*p && *p != ']') p++; if (*p == ']') p++; }
            else if (*p == '{') { int d = 1; p++; while (*p && d) { if (*p=='{') d++; if (*p=='}') d--; p++; } }
            else { while (*p && *p != ',' && *p != '}' && *p != '\n') p++; }
        }
    }
    return "unterminated";
}

static void _apply_defaults(BuffDef& d) {
    if (d.display_name.empty()) d.display_name = d.id;
    if (d.short_name.empty()) d.short_name = d.display_name;
}

bool load_buff_defs(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        LOG_ERROR("[BUFF] Cannot open %s", path.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string buf(sz + 1, '\0');
    fread(&buf[0], 1, sz, f);
    fclose(f);

    const char* p = buf.c_str();
    p = _skip_ws(p);
    if (*p != '[') {
        LOG_ERROR("[BUFF] JSON root must be array");
        return false;
    }
    p++;

    int loaded = 0, skipped = 0;
    while (*p) {
        p = _skip_ws(p);
        if (*p == ']') { p++; break; }
        if (*p == ',') { p++; continue; }

        BuffDef def;
        std::string err = _parse_buff_obj(p, def);
        if (!err.empty()) {
            LOG_WARN("[BUFF] Parse error #%d: %s -- skip", loaded + skipped, err.c_str());
            int depth = 0;
            while (*p) {
                if (*p == '{' && !depth) depth = 1;
                else if (*p == '{') depth++;
                else if (*p == '}') { depth--; if (!depth) { p++; break; } }
                p++;
            }
            skipped++; continue;
        }
        if (def.id.empty()) {
            LOG_WARN("[BUFF] Missing id -- skip");
            skipped++; continue;
        }
        _apply_defaults(def);

        if (g_buff_defs.count(def.id))
            LOG_WARN("[BUFF] Duplicate id '%s' -- overwrite", def.id.c_str());

        LOG_INFO("[BUFF] Loaded '%s': %s dur=%.1f stacks=%d tick=%.2f dmg=%d",
            def.id.c_str(), def.display_name.c_str(), def.duration,
            def.max_stacks, def.tick_interval, def.tick_damage);
        g_buff_defs[def.id] = def;
        loaded++;
    }

    g_buff_defs_loaded = true;
    LOG_INFO("[BUFF] Loaded %d buffs, skipped %d", loaded, skipped);
    return loaded > 0;
}

const BuffDef* get_buff_def(const std::string& id) {
    if (g_buff_defs.empty()) return nullptr;
    auto it = g_buff_defs.find(id);
    return it != g_buff_defs.end() ? &it->second : nullptr;
}

Color get_buff_hud_color(const std::string& id) {
    const BuffDef* def = get_buff_def(id);
    if (def) return {def->hud_color_r, def->hud_color_g, def->hud_color_b, 255};
    return {200, 200, 200, 255};
}

// ---- find_attack_target ----
Monster* find_attack_target(Rectangle r, const std::vector<Monster*>& targets, float range) {
    float rpx = range * 32.0f;
    Monster* best = nullptr;
    float best_d = 1e9f;
    for (auto* t : targets) {
        if (!t || !t->combat.is_alive) continue;
        float dx = r.x + r.width/2  - t->entity.rect.x - t->entity.rect.width/2;
        float dy = r.y + r.height/2 - t->entity.rect.y - t->entity.rect.height/2;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist <= rpx && dist < best_d) { best = t; best_d = dist; }
    }
    return best;
}

// ============================================================
// Internal helpers
// ============================================================

static int _count_stacks(const std::vector<BuffInstance>& buffs, const std::string& id) {
    for (auto& b : buffs) if (b.id == id) return b.stacks;
    return 0;
}

static void _apply_impl(std::vector<BuffInstance>& buffs,
    const std::string& id, int stacks, const char* target_name,
    std::vector<BuffEvent>* events) {
    const BuffDef* def = get_buff_def(id);
    if (!def) return;
    for (auto& b : buffs) {
        if (b.id == id) {
            b.stacks = std::min(b.stacks + stacks, def->max_stacks);
            b.remaining = def->duration;
            if (def->tick_interval <= 0.0f) b.tick_timer = 0.0f;
            if (events) events->push_back({BuffEventType::APPLIED, id, target_name, b.stacks, 0});
            return;
        }
    }
    BuffInstance bi;
    bi.id = id; bi.stacks = std::min(stacks, def->max_stacks);
    bi.remaining = def->duration;
    bi.tick_timer = (def->tick_interval > 0) ? def->tick_interval : 0.0f;
    buffs.push_back(bi);
    if (events) events->push_back({BuffEventType::APPLIED, id, target_name, bi.stacks, 0});
}

static void _tick_impl(std::vector<BuffInstance>& buffs,
    CombatStats* combat, float dt, const char* target_name,
    std::vector<BuffEvent>* events) {
    size_t i = 0;
    while (i < buffs.size()) {
        auto& b = buffs[i];
        const BuffDef* def = get_buff_def(b.id);
        bool removed = false;
        b.remaining -= dt;
        if (def && def->tick_interval > 0 && combat && combat->is_alive) {
            b.tick_timer -= dt;
            while (b.tick_timer <= 0 && b.remaining > 0 && combat->is_alive) {
                int dmg = def->tick_damage * b.stacks;
                combat->take_damage(dmg);
                if (events) events->push_back({BuffEventType::TICK_DAMAGE, b.id, target_name, b.stacks, dmg});
                b.tick_timer += def->tick_interval;
                if (!combat->is_alive) break;
            }
        }
        if (b.remaining <= 0) removed = true;
        if (removed) {
            if (events) events->push_back({BuffEventType::EXPIRED, b.id, target_name, b.stacks, 0});
            buffs[i] = buffs.back(); buffs.pop_back();
        } else { i++; }
    }
}

// ============================================================
// Public API
// ============================================================

void apply_buff(Player* p, const std::string& id, int stacks, std::vector<BuffEvent>* events) {
    if (p && p->combat.is_alive) _apply_impl(p->active_buffs, id, stacks, "Player", events);
}
void apply_buff(Monster* m, const std::string& id, int stacks, std::vector<BuffEvent>* events) {
    if (m && m->combat.is_alive) _apply_impl(m->active_buffs, id, stacks, m->name.c_str(), events);
}

void tick_buffs(Player* p, float dt, std::vector<BuffEvent>* events) {
    if (p && p->combat.is_alive) _tick_impl(p->active_buffs, &p->combat, dt, "Player", events);
}
void tick_buffs(Monster* m, float dt, std::vector<BuffEvent>* events) {
    if (m && m->combat.is_alive) _tick_impl(m->active_buffs, &m->combat, dt, m->name.c_str(), events);
}

int get_effective_attack(const Player* p) {
    if (!p) return 1;
    int base = p->combat.get_effective_attack();
    return std::max(1, (int)(base * (1.0f + 0.3f * _count_stacks(p->active_buffs, "attack_up"))));
}
int get_effective_attack(const Monster* m) {
    if (!m) return 1;
    int base = m->combat.get_effective_attack();
    return std::max(1, (int)(base * (1.0f + 0.3f * _count_stacks(m->active_buffs, "attack_up"))));
}

float get_effective_speed(const Player* p) {
    if (!p) return 200.0f;
    return p->speed * std::pow(0.7f, _count_stacks(p->active_buffs, "slow"));
}
float get_effective_speed(const Monster* m, float base_speed) {
    if (!m) return base_speed;
    return base_speed * std::pow(0.7f, _count_stacks(m->active_buffs, "slow"));
}

std::string get_buff_display_name(const std::string& id) {
    const BuffDef* def = get_buff_def(id);
    return def ? def->display_name : id;
}
std::string get_buff_short_name(const std::string& id) {
    const BuffDef* def = get_buff_def(id);
    return def ? def->short_name : id;
}
std::string format_buff_time(float sec) {
    char buf[16]; snprintf(buf, sizeof(buf), "%.1fs", sec);
    return buf;
}

// ---- BuffTrigger helpers ----
void apply_triggers(const std::vector<BuffTrigger>& triggers, Player* self, Monster* enemy) {
    for (auto& tr : triggers) {
        if (tr.target != BuffTarget::ENEMY) continue;
        if (tr.chance < 1.0f && (float)(rng() % 1000) / 1000.0f >= tr.chance) continue;
        apply_buff(enemy, tr.buff_id, tr.stacks);
    }
}
void apply_triggers_self(const std::vector<BuffTrigger>& triggers, Player* self) {
    for (auto& tr : triggers) {
        if (tr.target == BuffTarget::ENEMY) continue;
        if (tr.chance < 1.0f && (float)(rng() % 1000) / 1000.0f >= tr.chance) continue;
        apply_buff(self, tr.buff_id, tr.stacks);
    }
}
