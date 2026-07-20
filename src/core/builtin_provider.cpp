#include "builtin_provider.h"
#include "core/logger.h"
#include <fstream>

// ═══════════════════════════════════════════════════════════════
// G4.1: BuiltinProvider — 硬编码内置资源映射
// ═══════════════════════════════════════════════════════════════

BuiltinProvider::BuiltinProvider() {
    _module_paths = {
        {"buff",     "resources/buffs.json"},
        {"relic",    "resources/relics.json"},
        {"enemy",    "resources/enemies.json"},
        {"boss",     "resources/bosses.json"},
        {"dialogue", "resources/dialogues.json"},
        {"quest",    "resources/quests.json"},
        {"ending",   "resources/endings.json"},
        {"meta",     "resources/meta_nodes.json"},
        {"skill",    "resources/skills.json"},
        {"item",     "resources/items.json"},
    };
}

bool BuiltinProvider::has_module(const std::string& module_name) const {
    return _module_paths.count(module_name) > 0;
}

ModuleData BuiltinProvider::load_module(const std::string& module_name) {
    ModuleData md;
    md.provider = "Builtin";
    auto it = _module_paths.find(module_name);
    if (it == _module_paths.end()) return md;

    md.source = it->second;
    std::ifstream f(md.source);
    if (!f.is_open()) {
        LOG_ERROR("[Builtin] Cannot open %s", md.source.c_str());
        return md;
    }
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    md.text = std::move(content);
    return md;
}
