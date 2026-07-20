#include "data/item_defs.h"
#include "core/merge_patch.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

// ============================================================
// G3.3: ItemDef 全局注册表
// ============================================================

static RarityConfig g_rarity;
static std::unordered_map<std::string, ItemDef> g_item_defs_registry;
static std::unordered_map<std::string, std::string> _item_src;  // G4.3
static bool g_item_defs_loaded = false;

static ItemDef _parse_template(const json& j) {
    ItemDef def;
    def.id       = j.value("id", "");
    def.category = j.value("category", "");
    def.name     = j.value("name", "");

    def.atk_min  = j.value("atk_min", 5);
    def.atk_max  = j.value("atk_max", 12);
    def.pdef_min = j.value("pdef_min", 0);
    def.pdef_max = j.value("pdef_max", 3);
    def.mdef_min = j.value("mdef_min", 0);
    def.mdef_max = j.value("mdef_max", 4);

    def.effect_type = j.value("effect_type", "");
    def.heal_min    = j.value("heal_min", 15);
    def.heal_max    = j.value("heal_max", 40);
    def.buff_id     = j.value("buff_id", "");

    def.skill_id   = j.value("skill_id", "");
    def.cd_bonus   = j.value("cd_bonus", 0.15f);
    def.power_bonus = j.value("power_bonus", 0.40f);

    return def;
}

int load_item_defs_from_json(const char* json_text, MergeMode mode,
                               const char* id_namespace) {
    if (!json_text || !json_text[0]) return 0;
    json j; try { j = json::parse(json_text); } catch(...) { return 0; }
    if (!j.contains("items") || !j["items"].is_array()) return 0;
    if(mode==MergeMode::Skip && j.contains("rarity")&&j["rarity"].is_object()){
        auto& r=j["rarity"];
        for(size_t i=0;i<r["mults"].size()&&i<4;i++) g_rarity.mults[i]=r["mults"][i].get<float>();
        for(size_t i=0;i<r["weights"].size()&&i<4;i++) g_rarity.weights[i]=r["weights"][i].get<int>();
    }
    std::string ns=id_namespace?(std::string(id_namespace)+":"):"";
    int count=0,over=0,merged=0;
    for(auto& entry:j["items"]){
        bool is_patch=(mode==MergeMode::MergePatch&&entry.value("__patch",false));
        std::string rid=entry.value("id","");if(!ns.empty())rid=ns+rid;
        bool exists=g_item_defs_registry.count(rid)>0;
        if(mode==MergeMode::Skip&&exists)continue;
        if(is_patch&&exists){json stored;try{stored=json::parse(_item_src[rid]);}catch(...){continue;}
            auto def=_parse_template(_merge_patch(stored,entry));def.id=rid;
            g_item_defs_registry[rid]=def;_item_src[rid]=_merge_patch(stored,entry).dump();merged++;continue;}
        auto def=_parse_template(entry);
        if(def.id.empty())continue;if(!ns.empty())def.id=ns+def.id;
        if(exists){if(mode==MergeMode::Replace||mode==MergeMode::MergePatch){g_item_defs_registry[def.id]=def;over++;_item_src[def.id]=entry.dump();}continue;}
        g_item_defs_registry[def.id]=def;count++;_item_src[def.id]=entry.dump();
    }
    g_item_defs_loaded=true;
    printf("[ITEM_DEFS] +%d",count);if(over>0)printf(" ~%d",over);if(merged>0)printf(" %% %d",merged);printf("\n");
    return count+over+merged;
}

bool load_item_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[ITEM_DEFS] ERROR: cannot open %s\n", json_path.c_str());
        return false;
    }
    json j;
    try { f >> j; }
    catch (const std::exception& e) {
        printf("[ITEM_DEFS] JSON parse error: %s\n", e.what());
        return false;
    }

    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load_item_defs_from_json(text.c_str(), MergeMode::Skip) > 0;
}

const RarityConfig& get_rarity_config() { return g_rarity; }

const ItemDef* get_item_def(const std::string& id) {
    auto it = g_item_defs_registry.find(id);
    return (it != g_item_defs_registry.end()) ? &it->second : nullptr;
}

const std::unordered_map<std::string, ItemDef>& get_all_item_defs() {
    return g_item_defs_registry;
}

bool is_item_defs_loaded() { return g_item_defs_loaded; }
