#pragma once
#include "relic_effect.h"
#include "relic_effect_runtime.h"
#include "damage_context.h"
#include <vector>

class Player;
class Monster;

class RelicEffectProcessor {
public:
    void set_enabled(bool e) { _enabled = e; }
    bool is_enabled() const { return _enabled; }
    void reset_runtime() { _runtime.reset(); }

    void on_kill(Player* player, Monster* monster,
                 std::vector<Monster*>& all_monsters);
    void on_hit(Player* player, Monster* target);
    void on_pre_damage(DamageContext& ctx, Player* player);
    void on_hurt(Player* player, int final_damage);
    void on_floor_enter(Player* player);
    void tick(Player* player, float dt);

    // Static entry points (follows ElementResolver pattern)
    static void static_on_hit(Player* player, Monster* target);
    static void static_on_kill(Player* player, Monster* monster);
    static void static_on_pre_damage(DamageContext& ctx, Player* player);

private:
    bool _enabled = true;
    RelicEffectRuntime _runtime;

    void _apply_passive_stat(Player* player, const RelicEffectDef& eff);
    void _apply_on_kill(Player* player, Monster* monster,
                        std::vector<Monster*>& all_monsters,
                        const RelicEffectDef& eff);
    void _apply_on_hit(Player* player, Monster* target,
                       const RelicEffectDef& eff);
    void _apply_pre_damage(DamageContext& ctx, const RelicEffectDef& eff);
    void _apply_on_floor_enter(Player* player, const RelicEffectDef& eff);

    float _rng_float();
};
