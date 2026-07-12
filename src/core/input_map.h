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
    void add_action(const std::string& action, KeyboardKey key);

    bool is_action_pressed(const std::string& action) const;
    bool is_action_just_pressed(const std::string& action) const;
    Vector2 get_movement_axis() const;
    std::string get_key_name(const std::string& action) const;

    // ---- 默认游戏按键映射 ----
    void setup_defaults();

private:
    std::unordered_map<std::string, std::vector<KeyboardKey>> _bindings;

    static std::string _key_name(KeyboardKey k);
};
