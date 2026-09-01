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
    int hover_index = -1;  // Q4.5: 鼠标悬停菜单项
    std::vector<MenuItem> items;

    void _enter_tree() override;
    void _ready() override;
    void _process(double delta) override;
    void _render() override;
    void _input(const InputMap& input) override;
    const char* get_bgm_name() const override { return "title"; }

private:
    bool _activate(const std::string& action);  // Q4.5: 键盘/鼠标共用动作分发
    void _draw_stage();   // G10.7-B2: 电影海报舞台层 (渐变/透视地板/拱门/vignette)
    struct StageTex { Texture2D wall{}, floor{}, door{}; bool loaded = false; }
        _stage_tex;
};
