#pragma once
#include "node.h"
#include "input_map.h"
#include "raylib.h"
#include <vector>
#include <string>

struct MenuItem { std::string key, label, action; Color color; };

class TitleScene : public Node {
public:
    bool has_save = false;
    int max_floor = 1;
    float anim_time = 0;
    std::vector<MenuItem> items;

    void _enter_tree() override;
    void _ready() override;
    void _process(double delta) override;
    void _render() override;
    void _input(const InputMap& input) override;
    const char* get_bgm_name() const override { return "title"; }
};
