#pragma once
// G6.5: Encounter Framework — unified NPC/event/trade/quest_giver data
// G6.7: conditions + action string/dict support
#include <string>
#include <vector>
#include <unordered_map>

struct EncounterChoice {
    std::string text, next = "end", effect = "none", risk = "none";
};

struct EncounterNode {
    std::string id, text;
    std::string type = "dialogue";     // "dialogue" | "trade"
    std::vector<std::string> trade_items;
    std::string trade_cost;
    std::vector<EncounterChoice> choices;
};

struct EncounterDef {
    std::string id, type, name, biome;
    std::string trigger = "floor_enter";   // "floor_enter"|"room_enter"|"wall_interact"
    std::string room;                      // "" = any, "broken_cell" = specific
    bool repeatable = true;
    std::string narrative;
    std::vector<std::string> conditions;   // G6.7: "flag:X","floor>=N","biome:Y"
    std::vector<EncounterNode> dialogue;
};

extern std::vector<EncounterDef> g_encounters;
extern std::unordered_map<std::string, EncounterDef*> g_encounter_by_id;
extern std::unordered_map<std::string, std::vector<EncounterDef*>> g_encounters_by_biome;

bool load_encounter_defs(const char* json_path = "resources/encounters.json");
EncounterDef* get_encounter(const std::string& eid);
EncounterDef* pick_encounter_for_biome(const std::string& biome_id);
EncounterDef* pick_encounter_by_trigger(const std::string& biome_id, const std::string& trigger);
