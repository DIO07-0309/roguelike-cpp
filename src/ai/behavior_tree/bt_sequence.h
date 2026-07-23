#pragma once
// G8.1: Sequence — AND logic. Ticks children left→right, stops on FAILURE/RUNNING.

#include "bt_node.h"

namespace bt {

class Sequence : public Node {
public:
    Sequence(std::vector<std::unique_ptr<Node>> children)
        : _children(std::move(children)) {}

    Status tick(Blackboard& board) override {
        for (auto& c : _children) {
            Status s = c->tick(board);
            if (s != Status::SUCCESS) return s;
        }
        return Status::SUCCESS;
    }

    bool is_composite() const override { return true; }
    std::string name() const override { return "Sequence"; }

    std::unique_ptr<Node> clone() const override {
        std::vector<std::unique_ptr<Node>> cloned;
        for (auto& c : _children) cloned.push_back(c->clone());
        return std::make_unique<Sequence>(std::move(cloned));
    }

private:
    std::vector<std::unique_ptr<Node>> _children;
};

} // namespace bt
