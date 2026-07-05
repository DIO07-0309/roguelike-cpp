#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include "raylib.h"

// ============================================================
// InputMap — 抽象输入：动作 → 按键映射
// 游戏逻辑只关心"攻击"是否按下，不关心是空格还是J
// ============================================================
class InputMap {
public:
    // 绑定动作到按键
    void add_action(const std::string& action, KeyboardKey key) {
        _bindings[action].push_back(key);
    }

    // 轮询：当前帧是否按住
    bool is_action_pressed(const std::string& action) const {
        auto it = _bindings.find(action);
        if (it == _bindings.end()) return false;
        for (auto key : it->second) {
            if (IsKeyDown(key)) return true;
        }
        return false;
    }

    // 边沿触发：按下瞬间为 true
    bool is_action_just_pressed(const std::string& action) const {
        auto it = _bindings.find(action);
        if (it == _bindings.end()) return false;
        for (auto key : it->second) {
            if (IsKeyPressed(key)) return true;
        }
        return false;
    }

    // WASD → 归一化二维向量
    Vector2 get_movement_axis() const {
        float x = 0, y = 0;
        if (is_action_pressed("move_up"))    y -= 1;
        if (is_action_pressed("move_down"))  y += 1;
        if (is_action_pressed("move_left"))  x -= 1;
        if (is_action_pressed("move_right")) x += 1;
        // 归一化斜向
        if (x != 0 && y != 0) {
            float inv = 0.70710678f;  // 1/sqrt(2)
            x *= inv; y *= inv;
        }
        return {x, y};
    }

    // 获取绑定按键（用于 UI 提示）
    std::string get_key_name(const std::string& action) const {
        auto it = _bindings.find(action);
        if (it == _bindings.end() || it->second.empty()) return "?";
        return _key_name(it->second[0]);
    }

    // ---- 默认游戏按键映射 ----
    void setup_defaults() {
        add_action("move_up",    KEY_W);    add_action("move_up",    KEY_UP);
        add_action("move_down",  KEY_S);    add_action("move_down",  KEY_DOWN);
        add_action("move_left",  KEY_A);    add_action("move_left",  KEY_LEFT);
        add_action("move_right", KEY_D);    add_action("move_right", KEY_RIGHT);
        add_action("attack",     KEY_SPACE);
        add_action("pickup",     KEY_E);
        add_action("inventory",  KEY_I);
        add_action("skill_1",    KEY_ONE);  add_action("skill_2", KEY_TWO);
        add_action("skill_3",    KEY_THREE); add_action("skill_4", KEY_FOUR);
        add_action("descend",    KEY_PERIOD);
        add_action("confirm",    KEY_ENTER); add_action("confirm", KEY_KP_ENTER);
        add_action("cancel",     KEY_ESCAPE);
        add_action("fullscreen", KEY_F11);
    }

private:
    std::unordered_map<std::string, std::vector<KeyboardKey>> _bindings;

    static std::string _key_name(KeyboardKey k) {
        if (k >= KEY_A && k <= KEY_Z) return std::string(1, 'A' + (k - KEY_A));
        if (k >= KEY_ZERO && k <= KEY_NINE) return std::to_string(k - KEY_ZERO);
        if (k == KEY_SPACE) return "空格";
        if (k == KEY_ENTER) return "Enter";
        if (k == KEY_ESCAPE) return "Esc";
        if (k == KEY_UP) return "↑";
        if (k == KEY_DOWN) return "↓";
        if (k == KEY_LEFT) return "←";
        if (k == KEY_RIGHT) return "→";
        if (k == KEY_PERIOD) return ">";
        return "?";
    }
};
