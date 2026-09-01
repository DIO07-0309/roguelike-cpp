#include "scene_tree.h"
#include "logger.h"
#include "win_center.h"
#include "audio_server.h"
#include <algorithm>
#include <cstring>
#include <cmath>

SceneTree::SceneTree(int w, int h, const char* title) {
    // G10.9: 高分屏 + 可缩放 — 必须在 InitWindow 前设置
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(w, h, title);
    // G10.9: 按 DPI 放大窗口物理尺寸 (960×640 逻辑 × DPI), 字体物理像素更清晰
    {
        Vector2 dpi = GetWindowScaleDPI();
        if (dpi.x > 1.0f) {
            SetWindowSize((int)(w * dpi.x), (int)(h * dpi.y));
        }
        SetWindowMinSize(w / 2, h / 2);
    }
    // G10.7-B1: 窗口图标 — 与 exe 图标同源 (剑与门, assets/brand/roguelike.png)
    {
        Image icon = LoadImage("assets/brand/roguelike.png");
        if (icon.width > 0) {
            SetWindowIcon(icon);
            UnloadImage(icon);
        }
    }
    InitAudioDevice();
    SetExitKey(0);
    center_active_window();
    SetTargetFPS(60);
    _input.setup_defaults();
    _audio = std::make_unique<AudioServer>();
    _audio->init();
    _target = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT);
    // G10.9: blit 放大用双线性, 高分屏上文字平滑而非锯齿块
    SetTextureFilter(_target.texture, TEXTURE_FILTER_BILINEAR);
    _running = true;
}

SceneTree::~SceneTree() {
    if (_root) { _root->_propagate_exit_tree(); _root.reset(); }
    UnloadRenderTexture(_target);
    CloseWindow();
}

void SceneTree::change_scene(std::shared_ptr<Node> new_scene) {
    LOG_INFO("场景切换 -> %s", new_scene->name.c_str());
    // BGM 由场景自身声明 (组合优于硬编码映射表)
    const char* bgm = new_scene->get_bgm_name();
    if (bgm) {
        float vol = 0.38f;
        if (strcmp(bgm, "title") == 0) vol = 0.35f;
        else if (strcmp(bgm, "boss") == 0) vol = 0.45f;
        _audio->play_bgm(bgm, vol);
    }
    _pending_scene = new_scene;
    _scene_changed = true;
}

void SceneTree::process_frame(double delta) {
    if (_scene_changed && _pending_scene) {
        if (_root) _root->_propagate_exit_tree();
        _root = _pending_scene;
        _current_scene = _pending_scene.get();
        _root->_set_tree(this);
        _root->_propagate_enter_tree();
        _root->_propagate_ready();
        _pending_scene = nullptr;
        _scene_changed = false;
        _skip_input = 5;  // 跳过5帧防Esc穿透 (SetExitKey(0) 已彻底禁用默认Esc退出)
    }

    if (_root) {
        auto& children = _root->get_children();
        children.erase(std::remove_if(children.begin(), children.end(),
            [](auto& c) { return c->_queued_free; }), children.end());
    }

    if (_root) _root->_propagate_process(delta);
    _audio->update((float)delta);  // Q4.2: BGM 循环重播检测
    _time += delta;
}

// G10.9: 计算窗口客户区内保持 3:2 的 letterbox blit 矩形
Rectangle SceneTree::_blit_dst(Rectangle window_rect, bool fullscreen) const {
    if (fullscreen) {
        // 全屏: 用显示器尺寸, 保持比例居中
        int mw = GetMonitorWidth(GetCurrentMonitor());
        int mh = GetMonitorHeight(GetCurrentMonitor());
        float scale = fminf((float)mw / WINDOW_WIDTH, (float)mh / WINDOW_HEIGHT);
        float dw = WINDOW_WIDTH * scale, dh = WINDOW_HEIGHT * scale;
        return {(mw - dw) / 2, (mh - dh) / 2, dw, dh};
    }
    // 窗口模式: 客户区 (GetRenderWidth/Height 已含 HiDPI 物理像素)
    float vw = (float)GetRenderWidth(), vh = (float)GetRenderHeight();
    float scale = fminf(vw / WINDOW_WIDTH, vh / WINDOW_HEIGHT);
    float dw = WINDOW_WIDTH * scale, dh = WINDOW_HEIGHT * scale;
    return {(vw - dw) / 2, (vh - dh) / 2, dw, dh};
}

// G10.9: 鼠标物理坐标 → 960×640 逻辑坐标
Vector2 SceneTree::get_mouse_logical() const {
    Vector2 raw = GetMousePosition();
    bool fs = IsWindowFullscreen();
    Rectangle dst = _blit_dst({}, fs);
    if (dst.width <= 0 || dst.height <= 0) return raw;
    float lx = (raw.x - dst.x) * (WINDOW_WIDTH / dst.width);
    float ly = (raw.y - dst.y) * (WINDOW_HEIGHT / dst.height);
    return {lx, ly};
}

void SceneTree::_handle_input() {
    if (_skip_input > 0) { _skip_input--; return; }
    if (_input.is_action_just_pressed("fullscreen")) ToggleFullscreen();
    if (_current_scene) _current_scene->_input(_input);
}

void SceneTree::run() {
    double last = GetTime();
    while (_running && !WindowShouldClose()) {
        double now = GetTime();
        double dt = now - last;
        last = now;
        if (dt > 0.1) dt = 0.1;
        _handle_input();
        process_frame(dt);

        // Batch 3H + G10.9: 渲染到 960×640 逻辑缓冲, blit 到客户区
        // letterbox 保持 3:2 (消除任意拉伸变形), BILINEAR 平滑放大
        BeginTextureMode(_target);
        ClearBackground(BLACK);
        if (_root) _root->_render();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        // Source: entire texture (flipped vertically because OpenGL)
        Rectangle src = {0, 0, (float)_target.texture.width, -(float)_target.texture.height};
        Rectangle dst = _blit_dst({}, IsWindowFullscreen());
        DrawTexturePro(_target.texture, src, dst, {0, 0}, 0, WHITE);
        EndDrawing();
    }
}
