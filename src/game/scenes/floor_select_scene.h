#pragma once
#include "node.h"
#include "input_map.h"

class FloorSelectScene : public Node {
public:
    int max_unlocked = 1;
    int cursor = 0;
    void _render() override;
    void _input(const InputMap& input) override;
};
