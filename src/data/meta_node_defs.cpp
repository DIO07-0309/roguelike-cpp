#include "data/meta_node_defs.h"
#include "core/merge_patch.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

// ============================================================
// G3.1: MetaNodeDef 全局注册表
// ============================================================

static std::unordered_map<std::string, MetaNodeDef> g_meta_node_defs;
static std::unordered_map<std::string, std::string> _meta_node_src;
static bool g_meta_node_defs_loaded = false;

static MetaNodeDef _parse_meta_node(const json& j) {
    MetaNodeDef def;
    def.id          = j.value("id", "");
    def.name        = j.value("name", "");
    def.description = j.value("description", "");
    def.max_level   = j.value("max_level", 1);
    def.cost_base   = j.value("cost_base", 10);
    def.cost_scale  = j.value("cost_scale", 2);
    return def;
}

int load_meta_node_defs_from_json(const char* json_text, MergeMode mode,
                                     const char* id_namespace) {
    if (!json_text || !json_text[0]) return 0;
    json j; try { j = json::parse(json_text); } catch(...) { return 0; }
    if (!j.is_array()) return 0;
    std::string ns=id_namespace?(std::string(id_namespace)+":"):"";
    int count=0,over=0,merged=0;
    for(auto& entry:j){
        bool is_patch=(mode==MergeMode::MergePatch&&entry.value("__patch",false));
        std::string rid=entry.value("id","");if(!ns.empty())rid=ns+rid;
        bool exists=g_meta_node_defs.count(rid)>0;
        if(mode==MergeMode::Skip&&exists)continue;
        if(is_patch&&exists){json stored;try{stored=json::parse(_meta_node_src[rid]);}catch(...){continue;}
            auto def=_parse_meta_node(_merge_patch(stored,entry));def.id=rid;
            g_meta_node_defs[rid]=def;_meta_node_src[rid]=_merge_patch(stored,entry).dump();merged++;continue;}
        auto def=_parse_meta_node(entry);
        if(def.id.empty())continue;if(!ns.empty())def.id=ns+def.id;
        if(exists){if(mode==MergeMode::Replace||mode==MergeMode::MergePatch){g_meta_node_defs[def.id]=def;over++;_meta_node_src[def.id]=entry.dump();}continue;}
        g_meta_node_defs[def.id]=def;count++;_meta_node_src[def.id]=entry.dump();
    }
    g_meta_node_defs_loaded=true;
    printf("[META_NODE_DEFS] +%d",count);if(over>0)printf(" ~%d",over);if(merged>0)printf(" %% %d",merged);printf("\n");
    return count+over+merged;
}

bool load_meta_node_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[META_NODE_DEFS] ERROR: cannot open %s\n", json_path.c_str());
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load_meta_node_defs_from_json(text.c_str(), MergeMode::Skip) > 0;
}

const MetaNodeDef* get_meta_node_def(const std::string& id) {
    auto it = g_meta_node_defs.find(id);
    return (it != g_meta_node_defs.end()) ? &it->second : nullptr;
}

const std::unordered_map<std::string, MetaNodeDef>& get_all_meta_node_defs() {
    return g_meta_node_defs;
}

bool is_meta_node_defs_loaded() { return g_meta_node_defs_loaded; }
