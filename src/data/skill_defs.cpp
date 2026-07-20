#include "data/skill_defs.h"
#include "core/merge_patch.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

// ============================================================
// G3.2: SkillDef 全局注册表
// ============================================================

static std::unordered_map<std::string, SkillDef> g_skill_defs;
static std::unordered_map<std::string, std::string> _skill_src;  // G4.3
static bool g_skill_defs_loaded = false;

static SkillTriggerDef _parse_trigger(const json& j) {
    SkillTriggerDef tr;
    tr.buff_id = j.value("buff", "");
    tr.stacks  = j.value("stacks", 1);
    tr.chance  = j.value("chance", 1.0f);
    tr.target  = j.value("target", "enemy");
    return tr;
}

static SkillEvoDef _parse_evo(const json& j) {
    SkillEvoDef ev;
    ev.level = j.value("level", 1);
    ev.name  = j.value("name", "");
    ev.desc  = j.value("desc", "");
    if (j.contains("required_tags") && j["required_tags"].is_array())
        for (auto& t : j["required_tags"])
            ev.required_tags.push_back(t.get<std::string>());
    return ev;
}

static SkillDef _parse_skill(const json& j) {
    SkillDef def;
    def.id         = j.value("id", "");
    def.class_type = j.value("class_type", "");
    def.name       = j.value("name", "");
    def.cooldown   = j.value("cooldown", 2.0f);
    def.max_level  = j.value("max_level", 3);

    if (j.contains("tags") && j["tags"].is_array())
        for (auto& t : j["tags"])
            def.tags.push_back(t.get<std::string>());

    if (j.contains("triggers") && j["triggers"].is_array())
        for (auto& t : j["triggers"])
            def.triggers.push_back(_parse_trigger(t));

    if (j.contains("evolutions") && j["evolutions"].is_array())
        for (auto& e : j["evolutions"])
            def.evolutions.push_back(_parse_evo(e));

    def.modifier_key = j.value("modifier_key", "");
    def.base_value   = j.value("base_value", 0);
    if (j.contains("values_per_level") && j["values_per_level"].is_array())
        for (auto& v : j["values_per_level"])
            def.values_per_level.push_back(v.get<int>());

    return def;
}

int load_skill_defs_from_json(const char* json_text, MergeMode mode,
                                const char* id_namespace) {
    if (!json_text || !json_text[0]) return 0;
    json j; try { j = json::parse(json_text); } catch(...) { return 0; }
    if (!j.is_array()) return 0;
    std::string ns=id_namespace?(std::string(id_namespace)+":"):"";
    int count=0,over=0,merged=0;
    for(auto& entry:j){
        bool is_patch=(mode==MergeMode::MergePatch&&entry.value("__patch",false));
        std::string rid=entry.value("id","");if(!ns.empty())rid=ns+rid;
        bool exists=g_skill_defs.count(rid)>0;
        if(mode==MergeMode::Skip&&exists)continue;
        if(is_patch&&exists){json stored;try{stored=json::parse(_skill_src[rid]);}catch(...){continue;}
            auto def=_parse_skill(_merge_patch(stored,entry));def.id=rid;
            g_skill_defs[rid]=def;_skill_src[rid]=_merge_patch(stored,entry).dump();merged++;continue;}
        auto def=_parse_skill(entry);
        if(def.id.empty())continue;if(!ns.empty())def.id=ns+def.id;
        if(exists){if(mode==MergeMode::Replace||mode==MergeMode::MergePatch){g_skill_defs[def.id]=def;over++;_skill_src[def.id]=entry.dump();}continue;}
        g_skill_defs[def.id]=def;count++;_skill_src[def.id]=entry.dump();
    }
    g_skill_defs_loaded=true;
    printf("[SKILL_DEFS] +%d",count);if(over>0)printf(" ~%d",over);if(merged>0)printf(" %% %d",merged);printf("\n");
    return count+over+merged;
}

bool load_skill_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[SKILL_DEFS] ERROR: cannot open %s\n", json_path.c_str());
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load_skill_defs_from_json(text.c_str(), MergeMode::Skip) > 0;
}

const SkillDef* get_skill_def(const std::string& id) {
    auto it = g_skill_defs.find(id);
    return (it != g_skill_defs.end()) ? &it->second : nullptr;
}

const std::unordered_map<std::string, SkillDef>& get_all_skill_defs() {
    return g_skill_defs;
}

bool is_skill_defs_loaded() { return g_skill_defs_loaded; }
