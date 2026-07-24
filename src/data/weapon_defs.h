#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "game/types/weapon_types.h"
#include "core/registry_provider.h"  // G4.1: MergeMode

// ============================================================
// G9: weapon_defs — weapon definition registry (data-driven)
// Follows pattern: item_defs.h, vfx_recipe.h
// ============================================================

// Load weapon definitions from JSON file
bool load_weapon_defs(const std::string& json_path);

// Load from in-memory JSON (for mod support + replay)
int  load_weapon_defs_from_json(const char* json_text, MergeMode mode,
                                 const char* id_namespace = nullptr);

// Lookup
const WeaponDef* get_weapon_def(const std::string& id);

// List all weapons of a given type
std::vector<const WeaponDef*> get_weapon_defs_for_type(WeaponType wt);

// Full registry access
const std::unordered_map<std::string, WeaponDef>& get_all_weapon_defs();
bool is_weapon_defs_loaded();
