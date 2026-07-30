#pragma once
#include <string>
#include <unordered_map>
#include "core/registry_provider.h"  // MergeMode
#include "game/components/element_component.h"

// ============================================================
// G10.1: ElementDef — element config data (read-only, JSON-driven)
// ============================================================

struct ElementDef {
    std::string id;          // "fire" | "ice" | "poison"
    std::string name;        // "火焰核心" | "冰霜核心" | "剧毒核心"
    std::string description; // one-line effect description
    int base_value = 0;      // effect magnitude
};

bool load_element_defs(const std::string& json_path);
int  load_element_defs_from_json(const char* json_text, MergeMode mode,
                                  const char* id_namespace = nullptr);
const ElementDef* get_element_def(const std::string& id);
const std::unordered_map<std::string, ElementDef>& get_all_element_defs();
bool is_element_defs_loaded();
