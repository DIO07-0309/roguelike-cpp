#include "mod_provider.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#undef ERROR  // windows.h defines ERROR as macro, conflicts with LogLevel::ERROR
#endif

using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════
// G4.1: ModProvider — 单个 Mod 目录
// ═══════════════════════════════════════════════════════════════

std::unique_ptr<ModProvider> ModProvider::create(const std::string& mod_dir) {
    // 读取 mod.json
    std::string manifest_path = mod_dir + "/mod.json";
    std::ifstream f(manifest_path);
    if (!f.is_open()) {
        LOG_INFO("[ModProvider] Skipping %s: no mod.json", mod_dir.c_str());
        return nullptr;
    }

    json j;
    try { f >> j; }
    catch (const std::exception& e) {
        LOG_ERROR("[ModProvider] JSON error in %s: %s", manifest_path.c_str(), e.what());
        return nullptr;
    }

    auto mp = std::unique_ptr<ModProvider>(new ModProvider());
    mp->_manifest.id      = j.value("id", "");
    mp->_manifest.name    = j.value("name", mp->_manifest.id);
    mp->_manifest.version = j.value("version", "0.1.0");
    mp->_manifest.priority = j.value("priority", 100);
    mp->_manifest.enabled  = j.value("enabled", true);
    mp->_manifest.dir_path = mod_dir;

    if (mp->_manifest.id.empty()) {
        LOG_ERROR("[ModProvider] Missing 'id' in %s — skipping", manifest_path.c_str());
        return nullptr;
    }

    if (j.contains("provides") && j["provides"].is_array())
        for (auto& p : j["provides"])
            mp->_manifest.provides.push_back(p.get<std::string>());

    // G4.2: dependency fields
    if (j.contains("requires") && j["requires"].is_array()) {
        for (auto& r : j["requires"]) {
            if (r.is_object() && r.contains("id"))
                mp->_manifest.requires_ids.push_back(r["id"].get<std::string>());
            else if (r.is_string())
                mp->_manifest.requires_ids.push_back(r.get<std::string>());
        }
    }
    if (j.contains("load_after") && j["load_after"].is_array())
        for (auto& la : j["load_after"])
            mp->_manifest.load_after.push_back(la.get<std::string>());

    LOG_INFO("[ModProvider] + %s (v%s) pri=%d provides=%zu",
        mp->_manifest.id.c_str(), mp->_manifest.version.c_str(),
        mp->_manifest.priority, mp->_manifest.provides.size());
    return mp;
}

bool ModProvider::has_module(const std::string& module_name) const {
    return std::find(_manifest.provides.begin(), _manifest.provides.end(), module_name)
           != _manifest.provides.end();
}

ModuleData ModProvider::load_module(const std::string& module_name) {
    ModuleData md;
    md.provider = _manifest.id;  // G4.2: use mod id (not display name) for namespace
    md.source = _manifest.dir_path + "/" + module_name + "s.json";
    // Note: module names are singular (enemy, boss), file names are plural (enemies.json, bosses.json)
    // We try with the standard plural form
    std::ifstream f(md.source);
    if (!f.is_open()) {
        // Try alternative: <module>.json (no plural)
        md.source = _manifest.dir_path + "/" + module_name + ".json";
        f.open(md.source);
        if (!f.is_open()) {
            md.text.clear();
            return md;
        }
    }
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    md.text = std::move(content);
    return md;
}
