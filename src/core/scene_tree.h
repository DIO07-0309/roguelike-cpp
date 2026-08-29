#pragma once
#include <memory>
#include <vector>
#include "raylib.h"
#include "node.h"
#include "input_map.h"
#include "config.h"

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

    // 逻辑分辨率 (始终 960×640，全屏时由 RenderTexture 缩放)
    int width()  const { return WINDOW_WIDTH; }
    int height() const { return WINDOW_HEIGHT; }
    int get_width()  const { return width(); }
    int get_height() const { return height(); }

    void process_frame(double delta);
    bool is_running() const { return _running; }  // Q3.1: headless sim 循环
    void process_input() { _handle_input(); }    // Q3.1: headless sim 输入分发

private:
    void _handle_input();

    bool _running = false;
    double _time = 0.0;

    InputMap _input;
    std::shared_ptr<Node> _root;
    Node* _current_scene = nullptr;

    std::unique_ptr<AudioServer> _audio;

    RenderTexture2D _target = {0};  // Batch 3H: 960×640 render target

    std::shared_ptr<Node> _pending_scene;
    bool _scene_changed = false;
    int  _skip_input = 0;  // 场景切换后跳过N帧输入(防Esc穿透)
};
