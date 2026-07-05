#pragma once
#include "node.h"
#include "input_map.h"

class DeathScene : public Node {
public:
    int final_floor = 1, final_level = 1;
    void _render() override;
    void _input(const InputMap& input) override;
};
