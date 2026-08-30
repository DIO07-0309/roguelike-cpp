#include "scene_tree.h"
#include "logger.h"
#include "win_center.h"
#include "audio_server.h"
#include <algorithm>
#include <cstring>
#include <cmath>

SceneTree::SceneTree(int w, int h, const char* title) {
    InitWindow(w, h, title);
    InitAudioDevice();
    SetExitKey(0);
    center_active_window();
    SetTargetFPS(60);
    _input.setup_defaults();
    _audio = std::make_unique<AudioServer>();
    _audio->init();
    _target = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT);
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

        // Batch 3H: Render to 960×640 texture, then blit scaled
        BeginTextureMode(_target);
        ClearBackground(BLACK);
        if (_root) _root->_render();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        // Source: entire texture (flipped vertically because OpenGL)
        Rectangle src = {0, 0, (float)_target.texture.width, -(float)_target.texture.height};
        if (IsWindowFullscreen()) {
            int mw = GetMonitorWidth(0);
            int mh = GetMonitorHeight(0);
            float scale = fminf((float)mw / WINDOW_WIDTH, (float)mh / WINDOW_HEIGHT);
            float dw = WINDOW_WIDTH * scale;
            float dh = WINDOW_HEIGHT * scale;
            Rectangle dst = {(mw - dw) / 2, (mh - dh) / 2, dw, dh};
            DrawTexturePro(_target.texture, src, dst, {0, 0}, 0, WHITE);
        } else {
            DrawTexturePro(_target.texture, src,
                {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                {0, 0}, 0, WHITE);
        }
        EndDrawing();
    }
}
