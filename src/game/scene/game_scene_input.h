#pragma once
#include "input_map.h"

class GameScene;

// 从 GameScene 提取的输入处理模块
class GameSceneInput {
public:
    explicit GameSceneInput(GameScene& scene) : _s(scene) {}

    // 主入口: 替代 GameScene::_input()
    void handle_input(const InputMap& input);

    // Debug 按键 (F1-F12)
    void handle_debug_keys();

    // 子处理器 (由 handle_input 内部分发)
    void handle_event_input(const InputMap& input);
    void handle_dialogue_input(const InputMap& input);

private:
    GameScene& _s;
};
