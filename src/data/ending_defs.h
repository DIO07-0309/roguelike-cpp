#pragma once
#include <string>
#include <unordered_map>
#include "core/registry_provider.h"  // G4.1: MergeMode

// ============================================================
// G2.5: EndingDef — 结局配置数据 (纯数据, 无 Gameplay 依赖)
// 条件判定保留 C++ (EndingDirector::begin() 优先级链)
// 属于 Data 层, 位于 src/data/
// ============================================================

struct EndingDef {
    std::string id;                   // "bad_end" | "normal_end" | ...
    int type = 0;                     // EndingType enum int (0-4)
    std::string name;                 // "BAD END"
    std::string title;                // credits_title
    std::string sky_color;            // "深红" | "金色" | ...
    std::string final_line;           // 最终台词
    int meta_soul_bonus = 0;
    int meta_knowledge_bonus = 0;
};

// ── 全局注册表 ──
bool load_ending_defs(const std::string& json_path);
int  load_ending_defs_from_json(const char* json_text, MergeMode mode,
                                const char* id_namespace = nullptr);  // G4.2: namespace
const EndingDef* get_ending_def(int type);
const std::unordered_map<int, EndingDef>& get_all_ending_defs();
bool is_ending_defs_loaded();
