#pragma once

class GameScene;
struct Monster;

// 从 GameScene 提取的战斗善后模块
class GameSceneCombat {
public:
    explicit GameSceneCombat(GameScene& scene) : _s(scene) {}

    void on_monster_killed(Monster* m);
    void cleanup_dead_monsters();
    void apply_pending_damage();

private:
    GameScene& _s;
};
