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

    InitAudioDevice();

    SceneTree tree(WINDOW_WIDTH, WINDOW_HEIGHT, "Roguelike - C++ Edition");
    LOG_INFO("窗口创建");

    // JSON 配置通过 ResourceManager 加载
    load_buff_defs("resources/buffs.json");
    load_relic_defs("resources/relics.json");

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
