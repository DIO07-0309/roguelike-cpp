#include "data/boss_defs.h"
#include "core/merge_patch.h"   // G4.3
#include <nlohmann/json.hpp>
#include <fstream>
#include <unordered_map>
#include <cstdio>

using json = nlohmann::json;

// ============================================================
// G1 Step6: BossDef 全局注册表
// ============================================================

static std::unordered_map<std::string, BossDef> g_boss_defs;
static std::unordered_map<std::string, std::string> _boss_src;  // G4.3
static bool g_boss_defs_loaded = false;

// ── 辅助: JSON → BossSkillDef ──
static BossSkillDef _parse_skill(const json& j) {
    BossSkillDef sk;
    if (j.contains("id"))           sk.id = j["id"].get<std::string>();
    if (j.contains("cooldown"))     sk.cooldown = j["cooldown"].get<float>();
    if (j.contains("damage_mult"))  sk.damage_mult = j["damage_mult"].get<float>();
    if (j.contains("windup"))       sk.windup = j["windup"].get<float>();
    if (j.contains("range"))        sk.range = j["range"].get<float>();
    return sk;
}

// ── 辅助: JSON → BossDef ──
static BossDef _parse_boss(const json& j) {
    BossDef def;
    def.id        = j.value("id", "");
    def.name      = j.value("name", "");
    def.title     = j.value("title", "");
    def.lore      = j.value("lore", "");
    def.visual_id = j.value("visual_id", "shadow_knight");
    def.floor     = j.value("floor", 5);
    def.is_summoner = j.value("is_summoner", false);
    def.is_defender = j.value("is_defender", false);

    def.hp   = j.value("hp", 250);
    def.atk  = j.value("atk", 15);
    def.pdef = j.value("pdef", 10);
    def.mdef = j.value("mdef", 4);

    // Phase2
    def.phase2_hp_threshold = j.value("phase2_hp_threshold", 0.50f);
    def.phase2_pause        = j.value("phase2_pause", 0.50f);
    def.phase2_speed_mult   = j.value("phase2_speed_mult", 1.50f);
    def.phase2_atk_mult     = j.value("phase2_atk_mult", 1.25f);
    def.phase2_cd_mult      = j.value("phase2_cd_mult", 0.70f);

    def.shield_pct      = j.value("shield_pct", 0.0f);
    def.summon_speed    = j.value("summon_speed", 1.0f);
    def.skill_cycle_bias = j.value("skill_cycle_bias", 6);

    // 技能覆盖数组
    if (j.contains("skills") && j["skills"].is_array()) {
        for (auto& sk : j["skills"])
            def.skill_overrides.push_back(_parse_skill(sk));
    }

    // ── G2.3: Arena 战场配置 (可选) ──
    if (j.contains("arena") && j["arena"].is_object()) {
        auto& a = j["arena"];
        def.arena.danger_type     = a.value("danger_type", "shadow_wall");
        def.arena.spawn_interval  = a.value("spawn_interval", 8.0f);
        def.arena.max_zones       = a.value("max_zones", 6);
        def.arena.zone_duration   = a.value("zone_duration", 3.0f);
        def.arena.spawn_radius    = a.value("spawn_radius", 120.0f);
    }

    return def;
}

// ── G4.1: from JSON text ──
int load_boss_defs_from_json(const char* json_text, MergeMode mode,
                              const char* id_namespace) {
    if (!json_text || !json_text[0]) return 0;
    json j;
    try { j = json::parse(json_text); }
    catch (const std::exception& e) { printf("[BOSS_DEFS] parse: %s\n", e.what()); return 0; }
    if (!j.is_array()) return 0;
    std::string ns = id_namespace?(std::string(id_namespace)+":"):"";
    int count=0,over=0,merged=0;
    for (auto& entry : j) {
        bool is_patch=(mode==MergeMode::MergePatch&&entry.value("__patch",false));
        std::string rid=entry.value("id","");if(!ns.empty())rid=ns+rid;
        bool exists=g_boss_defs.count(rid)>0;
        if(mode==MergeMode::Skip&&exists)continue;
        if(is_patch&&exists){json stored;try{stored=json::parse(_boss_src[rid]);}catch(...){continue;}
            auto def=_parse_boss(_merge_patch(stored,entry));def.id=rid;
            g_boss_defs[rid]=def;_boss_src[rid]=_merge_patch(stored,entry).dump();merged++;continue;}
        auto def=_parse_boss(entry);
        if(def.id.empty())continue;if(!ns.empty())def.id=ns+def.id;
        if(exists){if(mode==MergeMode::Replace||mode==MergeMode::MergePatch){g_boss_defs[def.id]=def;over++;_boss_src[def.id]=entry.dump();}continue;}
        g_boss_defs[def.id]=def;count++;_boss_src[def.id]=entry.dump();
    }
    g_boss_defs_loaded=true;
    printf("[BOSS_DEFS] +%d",count);if(over>0)printf(" ~%d",over);if(merged>0)printf(" %% %d",merged);printf("\n");
    return count+over+merged;
}

// ── 公开接口 ──

bool load_boss_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[BOSS_DEFS] ERROR: cannot open %s\n", json_path.c_str());
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load_boss_defs_from_json(text.c_str(), MergeMode::Skip) > 0;
}

const BossDef* get_boss_def(const std::string& id) {
    auto it = g_boss_defs.find(id);
    return (it != g_boss_defs.end()) ? &it->second : nullptr;
}

const std::unordered_map<std::string, BossDef>& get_all_boss_defs() {
    return g_boss_defs;
}

bool is_boss_defs_loaded() { return g_boss_defs_loaded; }

const BossDef* get_boss_def_for_floor(int floor) {
    // 返回该楼层默认 Boss (供 UI 展示 name)
    if (floor == 5)  return get_boss_def("shadow_knight");
    if (floor == 10) return get_boss_def("fire_demon");
    if (floor == 15) return get_boss_def("demon_lord");
    return nullptr;
}

// BossType enum int values: SHADOW_KNIGHT=0,FIRE_DEMON=1,DEMON_LORD=2,NECROMANCER=3,GOLEM=4,VAMPIRE=5
const BossDef* get_boss_def_for_type(int type_int) {
    switch (type_int) {
        case 0: return get_boss_def("shadow_knight");
        case 1: return get_boss_def("fire_demon");
        case 2: return get_boss_def("demon_lord");
        case 3: return get_boss_def("necromancer");
        case 4: return get_boss_def("golem");
        case 5: return get_boss_def("vampire");
        default: return get_boss_def("shadow_knight");
    }
}
