#include "meta_progression.h"
#include "core/logger.h"
#include "data/meta_node_defs.h"  // G3.1
#include <cstdio>
#include <cstring>
#include <algorithm>

MetaSystem g_meta;

// G3.1: 构造器空 — 在 JSON 加载后由 load_from_defs() 填充
MetaSystem::MetaSystem() {
    _node_count = 0;
}

// G3.1: 从 MetaNodeDef registry 构建 (替代硬编码 10 个 MetaNode)
void MetaSystem::load_from_defs() {
    _node_texts.clear();
    static const char* ORDER[] = {
        "hp_bonus","atk_bonus","gold_start","potion_start","relic_bonus",
        "exp_bonus","buff_dur","skill_cd","build_speed","crit_bonus",nullptr
    };

    int count = 0;
    for (int i = 0; ORDER[i] && count < 10; i++) {
        const MetaNodeDef* def = get_meta_node_def(ORDER[i]);
        if (!def) continue;
        _node_texts.push_back(def->id);
        _node_texts.push_back(def->name);
        _node_texts.push_back(def->description);
        MetaNode& n = _nodes[count];
        n.id         = _node_texts[_node_texts.size() - 3].c_str();
        n.name       = _node_texts[_node_texts.size() - 2].c_str();
        n.desc       = _node_texts[_node_texts.size() - 1].c_str();
        n.max_level  = def->max_level;
        n.cost_base  = def->cost_base;
        n.cost_scale = def->cost_scale;
        count++;
    }
    _node_count = count;
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
    // G3.5: 记录 RUN_SUMMARY 奖励
    _reward_log.push_back({MetaRewardSource::RUN_SUMMARY,
        "Run结算: F" + std::to_string(rs.floor_reached) + " "
        + std::to_string(rs.bosses_killed) + "Boss "
        + std::to_string(rs.quests_done) + "Quest", earned});
    save();
    return earned;
}

void MetaSystem::add_currency(const MetaCurrency& c) {
    _save.currency.soul_fragments += c.soul_fragments;
    _save.currency.knowledge += c.knowledge;
    _save.currency.ancient_memory += c.ancient_memory;
}

// G3.5: 统一 Ending 奖励入口
void MetaSystem::reward_from_ending(const char* name, int soul, int knowledge,
                                     int ancient_memory) {
    MetaCurrency mc{soul, knowledge, ancient_memory};
    add_currency(mc);
    _reward_log.push_back({MetaRewardSource::ENDING,
        std::string("Ending: ") + name, mc});
    save();
}

void MetaSystem::clear_reward_log() { _reward_log.clear(); }

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
