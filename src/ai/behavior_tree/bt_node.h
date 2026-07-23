#pragma once
// G8.1: Behavior Tree — base node + status enum
// Pure C++ framework, zero game dependencies.

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace bt {

enum class Status { SUCCESS, FAILURE, RUNNING };

// Forward declare
class Blackboard;

// ── Base node ─────────────────────────────────────────────
class Node {
public:
    virtual ~Node() = default;
    virtual Status tick(Blackboard& board) = 0;
    virtual std::string name() const { return "Node"; }

    // Composite helpers
    virtual bool is_composite() const { return false; }
    // Leaf helpers
    virtual bool is_condition() const { return false; }
    virtual bool is_action()    const { return false; }

    // G8.3: clone for MCTS simulation
    virtual std::unique_ptr<Node> clone() const = 0;
};

} // namespace bt
