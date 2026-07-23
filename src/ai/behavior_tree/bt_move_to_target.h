#pragma once
// G8.2: MoveToTarget — BT leaf that follows a path toward the target.
// Uses Blackboard: "target_x", "target_y", "player_x", "player_y", "action"

#include "bt_node.h"
#include "ai/navigation/pathfinder.h"

namespace bt {

class MoveToTarget : public Node {
public:
    // factory: create with a walkable-check lambda
    static std::unique_ptr<MoveToTarget> create(
        ai::WalkableFn is_walkable, int map_w, int map_h) {
        return std::unique_ptr<MoveToTarget>(
            new MoveToTarget(std::move(is_walkable), map_w, map_h));
    }

    Status tick(Blackboard& board) override {
        // Read target and current position from blackboard
        int tx = board.get("target_x", -1);
        int ty = board.get("target_y", -1);
        int px = board.get("player_x", -1);
        int py = board.get("player_y", -1);
        if (tx < 0 || ty < 0 || px < 0 || py < 0) return Status::FAILURE;

        // Already at target?
        if (px == tx && py == ty) return Status::SUCCESS;

        // Find path
        ai::PathResult result = ai::find_path(
            {px, py}, {tx, ty}, _is_walkable, _map_w, _map_h, 200);

        if (!result.reachable || result.path.size() < 2)
            return Status::FAILURE;

        // Next step is path[1] (path[0] = current position)
        auto next = result.path[1];
        int dx = next.x - px, dy = next.y - py;

        if      (dx > 0) board.set<std::string>("action", "move_right");
        else if (dx < 0) board.set<std::string>("action", "move_left");
        else if (dy > 0) board.set<std::string>("action", "move_down");
        else if (dy < 0) board.set<std::string>("action", "move_up");

        return Status::SUCCESS;
    }

    std::string name() const override { return "MoveToTarget"; }
    bool is_action() const override { return true; }

    std::unique_ptr<Node> clone() const override {
        return create(_is_walkable, _map_w, _map_h);
    }

private:
    ai::WalkableFn _is_walkable;
    int _map_w, _map_h;

    MoveToTarget(ai::WalkableFn wf, int mw, int mh)
        : _is_walkable(std::move(wf)), _map_w(mw), _map_h(mh) {}
};

} // namespace bt
