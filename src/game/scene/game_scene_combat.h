#pragma once
#include "systems/relic_effect_processor.h"

class GameScene;
struct Monster;

class GameSceneCombat {
public:
    explicit GameSceneCombat(GameScene& scene) : _s(scene) {}

    void on_monster_killed(Monster* m);
    void cleanup_dead_monsters();
    void apply_pending_damage();

    RelicEffectProcessor& relic_fx() { return _relic_fx; }

private:
    GameScene& _s;
    RelicEffectProcessor _relic_fx;
};
