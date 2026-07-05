#pragma once
#include <memory>
#include <vector>
#include "raylib.h"
#include "node.h"
#include "input_map.h"

class AudioServer;
class VFXServer;

class SceneTree {
public:
    SceneTree(int width, int height, const char* title);
    ~SceneTree();

    void run();
    void change_scene(std::shared_ptr<Node> new_scene);
    Node* get_current_scene() const { return _current_scene; }
    void quit() { _running = false; }

    InputMap& get_input() { return _input; }
    AudioServer* get_audio() { return _audio.get(); }
    VFXServer* get_vfx()   { return _vfx.get(); }

    // 动态尺寸 (跟随窗口缩放) — 两个名字都可用
    int width()  const { return GetScreenWidth(); }
    int height() const { return GetScreenHeight(); }
    int get_width()  const { return width(); }
    int get_height() const { return height(); }

    void process_frame(double delta);

private:
    void _handle_input();
    void _render();

    bool _running = false;
    double _time = 0.0;

    InputMap _input;
    std::shared_ptr<Node> _root;
    Node* _current_scene = nullptr;

    std::unique_ptr<AudioServer> _audio;
    std::unique_ptr<VFXServer> _vfx;

    std::shared_ptr<Node> _pending_scene;
    bool _scene_changed = false;
    int  _skip_input = 0;  // 场景切换后跳过N帧输入(防Esc穿透)
};
