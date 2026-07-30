#pragma once
#include <string>
#include <unordered_map>
#include "core/registry_provider.h"  // MergeMode
#include "game/components/element_component.h"

// ============================================================
// G10.1: ElementDef — element config data (read-only, JSON-driven)
// ============================================================

struct ElementDef {
    std::string id;
    std::string name;
    std::string description;

    // ── Fire ──
    float crit_base = 15.0f;    // Lv1 crit chance (%)
    float crit_growth = 0.75f;  // +% per level
    float crit_multiplier = 1.5f;

    // ── Ice ──
    int   freeze_counter_max = 3;    // slow stacks needed for freeze
    float freeze_stage3 = 100.0f;    // Lv20 freeze chance
    float freeze_stage2 = 50.0f;
    float freeze_stage1 = 10.0f;

    // ── Poison ──
    float dot_scale_base = 0.05f;    // Lv1 = 5% of damage as DOT
    float dot_scale_growth = 0.005f; // +0.5% per level
    float dot_duration = 3.0f;
};

bool load_element_defs(const std::string& json_path);
int  load_element_defs_from_json(const char* json_text, MergeMode mode,
                                  const char* id_namespace = nullptr);
const ElementDef* get_element_def(const std::string& id);
const std::unordered_map<std::string, ElementDef>& get_all_element_defs();
bool is_element_defs_loaded();
