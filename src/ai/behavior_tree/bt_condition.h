#pragma once
// G8.1: Condition — leaf node that checks a predicate on the blackboard.

#include "bt_node.h"

namespace bt {

class Condition : public Node {
public:
    using Predicate = std::function<bool(Blackboard&)>;

    Condition(std::string cond_name, Predicate pred)
        : _name(std::move(cond_name)), _pred(std::move(pred)) {}

    Status tick(Blackboard& board) override {
        return _pred(board) ? Status::SUCCESS : Status::FAILURE;
    }

    bool is_condition() const override { return true; }
    std::string name() const override { return _name; }

    std::unique_ptr<Node> clone() const override {
        return std::make_unique<Condition>(_name, _pred);
    }

private:
    std::string _name;
    Predicate _pred;
};

} // namespace bt
