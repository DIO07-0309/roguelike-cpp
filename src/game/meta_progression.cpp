#include "meta_progression.h"
#include "core/logger.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

MetaSystem g_meta;

// 10个永久成长节点
MetaSystem::MetaSystem() {
    _nodes[0] = {"hp_bonus",    "生命力",   "+2%% 最大HP",        5, 2, 2};
    _nodes[1] = {"atk_bonus",   "攻击力",   "+2%% 攻击力",        5, 2, 2};
    _nodes[2] = {"gold_start",  "初始金币", "+15 初始金币",       5, 2, 3};
    _nodes[3] = {"potion_start","初始药水", "+1 初始药水",        3, 4, 4};
    _nodes[4] = {"relic_bonus", "圣物掉落", "+2%% 圣物掉率",     5, 3, 2};
    _nodes[5] = {"exp_bonus",   "经验",     "+3%% 经验获取",      5, 2, 3};
    _nodes[6] = {"buff_dur",    "Buff持续", "+5%% Buff时间",      3, 4, 3};
    _nodes[7] = {"skill_cd",    "技能冷却", "-2%% 冷却缩减",      5, 3, 3};
    _nodes[8] = {"build_speed", "构筑速度", "+5%% Build成型速度", 3, 3, 4};
    _nodes[9] = {"crit_bonus",  "暴击率",   "+1.5%% 暴击率",     5, 3, 3};
    _node_count = 10;
}

int MetaSystem::node_level(const char* id) const {
    for (int i = 0; i < _node_count; i++)
        if (strcmp(_nodes[i].id, id) == 0)
            return _save.node_levels[i];
    return 0;
}

float MetaSystem::permanent_bonus(const char* id) const {
    int lv = node_level(id);
    // 每个节点返回倍率: hp_bonus=0.02/级, atk=0.02/级, etc.
    if (strcmp(id, "hp_bonus") == 0)    return lv * 0.02f;
    if (strcmp(id, "atk_bonus") == 0)   return lv * 0.02f;
    if (strcmp(id, "gold_start") == 0)  return lv * 15.0f;
    if (strcmp(id, "potion_start") == 0) return (float)lv;
    if (strcmp(id, "relic_bonus") == 0) return lv * 0.02f;
    if (strcmp(id, "exp_bonus") == 0)   return lv * 0.03f;
    if (strcmp(id, "buff_dur") == 0)    return lv * 0.05f;
    if (strcmp(id, "skill_cd") == 0)    return lv * -0.02f;
    if (strcmp(id, "build_speed") == 0) return lv * 0.05f;
    if (strcmp(id, "crit_bonus") == 0)  return lv * 0.015f;
    return 0;
}

bool MetaSystem::upgrade_node(const char* id, MetaCurrency& cost) {
    for (int i = 0; i < _node_count; i++) {
        if (strcmp(_nodes[i].id, id) != 0) continue;
        int lv = _save.node_levels[i];
        if (lv >= _nodes[i].max_level) return false;
        int need = _nodes[i].cost_base + lv * _nodes[i].cost_scale;
        if (_save.currency.soul_fragments < need) return false;
        _save.currency.soul_fragments -= need;
        cost.soul_fragments += need;
        _save.node_levels[i]++;
        save();
        return true;
    }
    return false;
}

MetaCurrency MetaSystem::end_run(const RunSummary& rs) {
    MetaCurrency earned = calc_run_reward(rs);
    _save.currency.soul_fragments += earned.soul_fragments;
    _save.currency.knowledge += earned.knowledge;
    _save.currency.ancient_memory += earned.ancient_memory;
    _save.total_runs++;
    save();
    return earned;
}

void MetaSystem::add_currency(const MetaCurrency& c) {
    _save.currency.soul_fragments += c.soul_fragments;
    _save.currency.knowledge += c.knowledge;
    _save.currency.ancient_memory += c.ancient_memory;
}

// ---- JSON save ----
bool MetaSystem::save() const {
    FILE* f = fopen("saves/meta_save.json", "w");
    if (!f) return false;
    fprintf(f, "{\n");
    fprintf(f, "  \"runs\":%d,\n", _save.total_runs);
    fprintf(f, "  \"soul\":%d,\n", _save.currency.soul_fragments);
    fprintf(f, "  \"know\":%d,\n", _save.currency.knowledge);
    fprintf(f, "  \"memory\":%d,\n", _save.currency.ancient_memory);
    fprintf(f, "  \"nodes\":[");
    for (int i = 0; i < _node_count; i++) {
        fprintf(f, "%d%s", _save.node_levels[i], (i < _node_count-1) ? "," : "");
    }
    fprintf(f, "]\n}\n");
    fclose(f);
    return true;
}

bool MetaSystem::load() {
    FILE* f = fopen("saves/meta_save.json", "r");
    if (!f) return false;
    // 简易解析
    char buf[512];
    fread(buf, 1, sizeof(buf)-1, f); fclose(f); buf[sizeof(buf)-1]=0;
    auto parse_int = [&](const char* key, int def) {
        const char* p = strstr(buf, key);
        if (!p) return def;
        p += strlen(key) + 1; // skip past '":'
        while (*p && (*p==' '||*p==':')) p++;
        return atoi(p);
    };
    _save.total_runs   = parse_int("\"runs\"", 0);
    _save.currency.soul_fragments = parse_int("\"soul\"", 0);
    _save.currency.knowledge      = parse_int("\"know\"", 0);
    _save.currency.ancient_memory = parse_int("\"memory\"", 0);
    // 解析 nodes 数组
    const char* np = strstr(buf, "\"nodes\"");
    if (np) { np = strstr(np, "["); if (np) { np++;
        for (int i = 0; i < _node_count && *np && *np != ']'; i++) {
            while (*np && *np != '-' && (*np < '0' || *np > '9')) np++;
            if (*np == ']') break;
            _save.node_levels[i] = atoi(np);
            while (*np && *np != ',' && *np != ']') np++;
            if (*np == ',') np++;
    }}}
    LOG_INFO("[META] Loaded: runs=%d soul=%d", _save.total_runs, _save.currency.soul_fragments);
    return true;
}

// 计算本局奖励
MetaCurrency calc_run_reward(const RunSummary& rs) {
    MetaCurrency m;
    m.soul_fragments = rs.floor_reached * 1
                     + rs.bosses_killed * 5
                     + rs.quests_done * 3
                     + rs.elite_kills;
    m.knowledge      = rs.relics_collected / 2
                     + (rs.combo_max >= 30 ? 3 : rs.combo_max >= 10 ? 1 : 0);
    m.ancient_memory = rs.bosses_killed > 0 ? 1 : 0;
    if (rs.bosses_killed >= 3) m.ancient_memory += 2;
    if (rs.floor_reached >= 15) m.soul_fragments += 10;
    return m;
}
