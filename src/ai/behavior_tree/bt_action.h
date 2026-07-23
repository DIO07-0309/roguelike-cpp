#pragma once
// G8.1: Action — leaf node that executes an action and returns SUCCESS.

#include "bt_node.h"

namespace bt {

class Action : public Node {
public:
    using Executor = std::function<void(Blackboard&)>;

    Action(std::string action_name, Executor exec)
        : _name(std::move(action_name)), _exec(std::move(exec)) {}

    Status tick(Blackboard& board) override {
        _exec(board);
        return Status::SUCCESS;
    }

    bool is_action() const override { return true; }
    std::string name() const override { return _name; }

    std::unique_ptr<Node> clone() const override {
        return std::make_unique<Action>(_name, _exec);
    }

private:
    std::string _name;
    Executor _exec;
};

} // namespace bt
