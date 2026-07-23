// ============================================================
// Roguelike C++ Edition
// 重庆大学大数据与软件学院 · 程序设计实训
// 开发者：ruozhiDIO
// ============================================================
#include "raylib.h"
#include "core/scene_tree.h"
#include "core/logger.h"
#include "scenes/game_scene.h"
#include "scenes/title_scene.h"
#include "save/save_manager.h"
#include "systems/combat_system.h"
#include "resources/resource_manager.h"
#include "core/service_locator.h"
#include "core/event_bus.h"
#include "config.h"
#include "core/registry_builder.h"     // G4.1
#include "core/builtin_provider.h"     // G4.1
#include "core/mod_provider.h"         // G4.1
#include "core/mod_dependency.h"      // G4.2
// G6: World layer data
#include "world/biome.h"
#include "world/landmark.h"
#include "world/hazard.h"
#include "world/encounter.h"
#include "data/vfx_recipe.h"
#include "core/sim/sim_runner.h"      // G5.6
#include "core/mod_manager.h"         // G4.4
#include "data/enemy_defs.h"
#include "data/boss_defs.h"
#include "data/dialogue_defs.h"
#include "data/quest_defs.h"
#include "data/ending_defs.h"
#include "data/meta_node_defs.h"
#include "data/skill_defs.h"
#include "data/item_defs.h"
#include "data/vfx_recipe.h"      // G5.8.5
#include "systems/combat_system.h"
#include <cstdio>
#include <memory>
#include <exception>
#include <cstring>

// 全局字体 (向后兼容 — 由 ResourceManager 管理)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

static void load_fonts() {
    ResourceManager::inst().load_all();
    g_font = ResourceManager::inst().font(32);
    g_font_small = ResourceManager::inst().font_small();
    g_font_loaded = ResourceManager::inst().font_loaded();
}

static void _terminate_handler() {
    FILE* cf = fopen("crash.log", "w");
    if (cf) {
        time_t now = time(nullptr); struct tm tm_buf;
        localtime_s(&tm_buf, &now); char ts[32];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);
        fprintf(cf, "[%s] CRASH\nC++ std::terminate\n开发者：ruozhiDIO\n", ts);
        fclose(cf);
    }
    fprintf(stderr, "\n致命错误! 详见 crash.log\n按 Enter 退出...");
    getchar();
}

int main() {
    Logger::inst().init();
#ifdef _WIN32
    install_seh_handler();
#endif
    std::set_terminate(_terminate_handler);

    LOG_INFO("Roguelike C++ Edition 启动 %s %s", __DATE__, __TIME__);
    SetTraceLogLevel(LOG_WARNING);

    // ── G4.5/G7.3: CLI replay/record/sim flags ──
    bool replay_mode = false, record_mode = false, sim_mode = false;
    std::string replay_path, record_path;
    int sim_runs = 100;
    uint32_t sim_seed_start = 0;
    std::string sim_build;
#ifdef _WIN32
    for (int i = 1; i < __argc; i++) {
        std::string arg = __argv[i];
        if (arg == "--replay" && i + 1 < __argc) {
            replay_mode = true;
            replay_path = __argv[++i];
        } else if (arg == "--record" && i + 1 < __argc) {
            record_mode = true;
            record_path = __argv[++i];
        } else if (arg == "--sim" && i + 1 < __argc) {
            sim_mode = true;
            sim_runs = atoi(__argv[++i]);
        } else if (arg == "--sim-seed" && i + 1 < __argc) {
            sim_seed_start = (uint32_t)atoi(__argv[++i]);
        } else if (arg == "--sim-build" && i + 1 < __argc) {
            sim_build = __argv[++i];
        }
    }
#endif

    // ── G4.5/G7.3: Set replay/record/sim config ──
    GameScene::g_record_mode = record_mode;
    GameScene::g_replay_mode = replay_mode;
    GameScene::g_record_path = record_path;
    GameScene::g_replay_path = replay_path;
    GameScene::g_sim_mode  = sim_mode;    // G5.6
    GameScene::g_sim_runs  = sim_runs;    // G5.6
    if (record_mode) LOG_INFO("Replay: recording to %s", record_path.c_str());
    if (replay_mode) LOG_INFO("Replay: playing from %s", replay_path.c_str());
    if (sim_mode) {
        SimulationConfig sim_cfg;
        sim_cfg.runs = sim_runs;
        sim_cfg.seed_start = sim_seed_start;
        if (!sim_build.empty()) {
            sim_cfg.random_build = false;
            sim_cfg.fixed_build = true;
            sim_cfg.fixed_build_id = sim_build;
        }
        SimRunner::inst().begin(sim_cfg);
        LOG_INFO("Sim: %d runs", sim_runs);
    }

    InitAudioDevice();

    SceneTree tree(WINDOW_WIDTH, WINDOW_HEIGHT, "Roguelike - C++ Edition");
    LOG_INFO("窗口创建");

    // ═══ G4.1: RegistryBuilder — Provider 驱动加载 ─═
    LOG_INFO("── Registry Build ──");
    RegistryBuilder builder;
    builder.add_provider(std::make_unique<BuiltinProvider>());

    // ── 注册模块加载器 (各 Def 模块) ──
    builder.register_module("buff",     load_buff_defs_from_json);
    builder.register_module("relic",    load_relic_defs_from_json);
    builder.register_module("enemy",    load_enemy_defs_from_json);
    builder.register_module("boss",     load_boss_defs_from_json);
    builder.register_module("dialogue", load_dialogue_defs_from_json);
    builder.register_module("quest",    load_quest_defs_from_json);
    builder.register_module("ending",   load_ending_defs_from_json);
    builder.register_module("meta",     load_meta_node_defs_from_json);
    builder.register_module("skill",    load_skill_defs_from_json);
    builder.register_module("item",     load_item_defs_from_json);

    // ── G4.4: ModManager 驱动 + 依赖解析 ──
    {
        ModManager mod_mgr;
        mod_mgr.scan("mods");

        // Collect enabled mod providers
        std::vector<std::unique_ptr<ModProvider>> mods_temp;
        std::vector<ModDepInfo> dep_info;
        for (auto& mi : mod_mgr.all()) {
            if (!mi.enabled || !mi.valid) continue;
            auto mp = ModProvider::create(mi.dir_path);
            if (!mp) continue;
            ModDepInfo di;
            di.id = mp->manifest().id;
            di.requires_ids = mp->manifest().requires_ids;
            di.load_after = mp->manifest().load_after;
            dep_info.push_back(di);
            mods_temp.push_back(std::move(mp));
        }

        // Dependency resolve
        auto dep_result = DependencyResolver::resolve(dep_info);
        for (auto& id : dep_result.ordered_ids) {
            for (auto& mp : mods_temp) {
                if (mp && mp->manifest().id == id) {
                    builder.add_provider(std::move(mp));
                    break;
                }
            }
        }
        for (auto& s : dep_result.skipped)
            LOG_INFO("  [SKIP] Mod '%s': dependency missing", s.c_str());
        for (auto& c : dep_result.cycle_info)
            LOG_INFO("  [SKIP] Mod '%s': cyclic dependency", c.c_str());

        LOG_INFO("── Mods: %d/%d enabled ──",
                 mod_mgr.enabled_count(), mod_mgr.total_count());
    }

    if (!builder.build_all())
        LOG_ERROR("  [FAIL] Registry build");
    for (auto& rec : builder.log())
        LOG_INFO("  [%s] %s ← %s%s",
            rec.module.c_str(), rec.provider.c_str(), rec.source.c_str(),
            rec.is_merge ? " (merge)" : "");
    {
        auto& fs = builder.findings();
        if (!fs.empty()) {
            LOG_INFO("── Validator: %zu issues ──", fs.size());
            for (auto& f : fs)
                LOG_INFO("  [%s] %s/%s: %s",
                    f.severity == ValSeverity::Error ? "ERR" : "WARN",
                    f.module.c_str(), f.entry_id.c_str(), f.message.c_str());
        }
    }
    g_meta.load_from_defs();  // G3.1: MetaNode 需在 meta 模块加载后重建
    load_vfx_recipes("resources/vfx_recipes.json");  // G5.8.5: VFX recipe registry

    // G6.1-G6.7: World layer data loading
    load_biome_defs("resources/biomes.json");
    load_landmark_defs("resources/landmarks.json");
    load_hazard_defs("resources/hazards.json");
    load_encounter_defs("resources/encounters.json");

    // Font 通过 ResourceManager 加载
    load_fonts();

    // D7 Step6: 注册全局服务
    ServiceLocator::provide(&ResourceManager::inst());
    ServiceLocator::provide(&EventBus::inst());
    LOG_INFO("ServiceLocator: 全局服务已注册");

    bool has_save = SaveManager::save_exists();
    if (has_save) { auto* d = SaveManager::load_save(); delete d; }
    LOG_INFO(has_save ? "存档存在" : "暂无存档");

    auto title = std::make_shared<TitleScene>();
    title->name = "TitleScene";
    title->has_save = has_save;
    tree.change_scene(title);
    tree.run();

    ServiceLocator::remove_all();
    ResourceManager::inst().unload_all();
    CloseAudioDevice();
    Logger::inst().close();
    return 0;
}
