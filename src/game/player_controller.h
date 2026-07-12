#pragma once

class GameScene;
class InputMap;

// ============================================================
// D6 Step7: PlayerController — 玩家输入/攻击/技能/移动/交互
// 组合模式: 持有 GameScene 引用, 所有 player 行为集中于此
// GameScene 不再直接处理 WASD/攻击/技能/交互
// ============================================================
class PlayerController {
public:
    void bind(GameScene* gs) { _scene = gs; }

    // ── 每帧 tick (移动 + 房间发现 + 怪物AI触发) ──
    void tick(float dt);

    // ── 输入处理 (攻击/技能/拾取/交互/背包/下楼) ──
    void handle_input(const InputMap& input);

    // ── 单一动作 ──
    void player_attack();
    void use_skill(int index);

private:
    GameScene* _scene = nullptr;
};
