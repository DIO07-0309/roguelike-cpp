#pragma once
#include "node.h"
#include "input_map.h"

class VictoryScene : public Node {
public:
    int final_level = 1;
    void _render() override;
    void _input(const InputMap& input) override;
};
