#pragma once
// G8.1: Selector — OR logic. Ticks children left→right, returns first SUCCESS/RUNNING.

#include "bt_node.h"

namespace bt {

class Selector : public Node {
public:
    Selector(std::vector<std::unique_ptr<Node>> children)
        : _children(std::move(children)) {}

    Status tick(Blackboard& board) override {
        for (auto& c : _children) {
            Status s = c->tick(board);
            if (s != Status::FAILURE) return s;
        }
        return Status::FAILURE;
    }

    bool is_composite() const override { return true; }
    std::string name() const override { return "Selector"; }
    const auto& children() const { return _children; }

    std::unique_ptr<Node> clone() const override {
        std::vector<std::unique_ptr<Node>> cloned;
        for (auto& c : _children) cloned.push_back(c->clone());
        return std::make_unique<Selector>(std::move(cloned));
    }

private:
    std::vector<std::unique_ptr<Node>> _children;
};

} // namespace bt
