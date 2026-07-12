#pragma once
#include "node.h"
#include "input_map.h"
#include <string>

class DeathScene : public Node {
public:
    int final_floor = 1, final_level = 1;
    std::string ending_name;
    std::string final_line;
    int meta_soul = 0, meta_knowledge = 0;
    void _render() override;
    void _input(const InputMap& input) override;
};
