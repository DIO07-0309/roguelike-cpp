#include "scene_tree.h"
#include "vfx_server.h"
#include "logger.h"
#include "win_center.h"
#include "audio_server.h"
#include <algorithm>

SceneTree::SceneTree(int w, int h, const char* title) {
    InitWindow(w, h, title);
    SetExitKey(0);  // 禁用默认Esc退出，由场景自行处理
    center_active_window();
    SetTargetFPS(60);
    _input.setup_defaults();
    _audio = std::make_unique<AudioServer>();
    _vfx = std::make_unique<VFXServer>();
    _audio->init();
    _running = true;
}

SceneTree::~SceneTree() {
    if (_root) { _root->_propagate_exit_tree(); _root.reset(); }
    CloseWindow();
}

void SceneTree::change_scene(std::shared_ptr<Node> new_scene) {
    LOG_INFO("场景切换 -> %s", new_scene->name.c_str());
    // BGM 随场景切换
    const auto& name = new_scene->name;
    if (name == "TitleScene") _audio->play_bgm("title", 0.35f);
    else if (name == "GameScene") _audio->play_bgm("dungeon", 0.38f);
    else if (name == "BossIntroScene" || name.find("Boss") != std::string::npos) _audio->play_bgm("boss", 0.45f);
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

    _vfx->update((float)delta);
    if (_root) _root->_propagate_process(delta);
    _time += delta;
}

void SceneTree::_handle_input() {
    if (_skip_input > 0) { _skip_input--; return; }
    if (_input.is_action_just_pressed("fullscreen")) ToggleFullscreen();
    if (_current_scene) _current_scene->_input(_input);
}

void SceneTree::_render() { _vfx->draw(0, 0); }

void SceneTree::run() {
    double last = GetTime();
    while (_running && !WindowShouldClose()) {
        double now = GetTime();
        double dt = now - last;
        last = now;
        if (dt > 0.1) dt = 0.1;
        _handle_input();
        process_frame(dt);
        BeginDrawing();
        ClearBackground(BLACK);
        if (_root) _root->_render();
        _render();
        EndDrawing();
    }
}
