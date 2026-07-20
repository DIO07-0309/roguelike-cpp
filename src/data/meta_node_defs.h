#pragma once
#include <string>
#include <unordered_map>
#include "core/registry_provider.h"  // G4.1: MergeMode

// ============================================================
// G3.1: MetaNodeDef — 永久成长节点配置 (纯数据)
// 属于 Data 层, MetaProgression runtime 持有 level 状态
// ============================================================

struct MetaNodeDef {
    std::string id;              // "hp_bonus" | "atk_bonus" | ...
    std::string name;            // "生命力"
    std::string description;     // "+2% 最大HP"
    int max_level = 1;
    int cost_base = 10;          // 第一级消耗 soul_fragments
    int cost_scale = 2;          // 每级递增
};

// ── 全局注册表 ──
bool load_meta_node_defs(const std::string& json_path);
int  load_meta_node_defs_from_json(const char* json_text, MergeMode mode,
                                     const char* id_namespace = nullptr);  // G4.2: namespace
const MetaNodeDef* get_meta_node_def(const std::string& id);
const std::unordered_map<std::string, MetaNodeDef>& get_all_meta_node_defs();
bool is_meta_node_defs_loaded();
