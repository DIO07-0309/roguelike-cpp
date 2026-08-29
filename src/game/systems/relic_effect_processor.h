#pragma once
#include "relic_effect.h"
#include "relic_effect_runtime.h"
#include "damage_context.h"
#include <vector>
#include <string>
#include <unordered_set>

class Player;
class Monster;

class RelicEffectProcessor {
public:
    void set_enabled(bool e) { _enabled = e; }
    bool is_enabled() const { return _enabled; }
    void reset_runtime() { _runtime.reset(); _passive_applied.clear(); }

    void on_kill(Player* player, Monster* monster,
                 std::vector<Monster*>& all_monsters);
    void on_hit(Player* player, Monster* target);
    void on_pre_damage(DamageContext& ctx, Player* player);
    void on_hurt(Player* player, int final_damage);
    void on_floor_enter(Player* player);
    void on_relic_acquired(Player* player, const std::string& relic_id);
    void on_relic_removed(Player* player, const std::string& relic_id);
    void tick(Player* player, float dt);

    // Static entry points (follows ElementResolver pattern)
    static void static_on_hit(Player* player, Monster* target);
    static void static_on_kill(Player* player, Monster* monster);
    static void static_on_pre_damage(DamageContext& ctx, Player* player);
    // Batch 3H: static PASSIVE apply/remove for use from RewardManager etc.
    static void apply_passive_for_relic(Player* player, const std::string& relic_id);
    static void remove_passive_for_relic(Player* player, const std::string& relic_id);

private:
    bool _enabled = true;
    RelicEffectRuntime _runtime;
    // Batch 3H: track which relics have had PASSIVE stats applied (apply-once)
    std::unordered_set<std::string> _passive_applied;

    void _apply_passive_stat(Player* player, const RelicEffectDef& eff);
    void _remove_passive_stat(Player* player, const RelicEffectDef& eff);
    void _apply_on_kill(Player* player, Monster* monster,
                        std::vector<Monster*>& all_monsters,
                        const RelicEffectDef& eff);
    void _apply_on_hit(Player* player, Monster* target,
                       const RelicEffectDef& eff);
    void _apply_pre_damage(DamageContext& ctx, const RelicEffectDef& eff);
    void _apply_on_floor_enter(Player* player, const RelicEffectDef& eff);

    float _rng_float();
};
