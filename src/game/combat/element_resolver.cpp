#include "combat/element_resolver.h"
#include "entities/player.h"
#include "entities/monster.h"
#include "components/element_component.h"
#include "systems/combat_system.h"  // apply_buff, rng

// ══════════════════════════════════════════════════════
// Fire: crit roll → damage ×multiplier
// ══════════════════════════════════════════════════════
static bool _resolve_fire(Player* p, int& damage, bool& is_crit) {
    float chance = p->element.fire_crit_chance() / 100.0f;
    if ((rng() % 1000) / 1000.0f >= chance) return false;
    float mult = p->element.fire_crit_mult();
    damage = (int)(damage * mult);
    is_crit = true;
    return true;
}

// ══════════════════════════════════════════════════════
// Ice: apply slow, track freeze counter, trigger freeze
// ══════════════════════════════════════════════════════
static bool _resolve_ice(Player* p, Monster* m, bool& did_freeze) {
    // Always apply slow
    apply_buff(m, "slow", 1);
    p->element.freeze_counter++;

    int max_slow = p->element.ice_freeze_counter_max();
    float freeze_chance = p->element.ice_freeze_chance() / 100.0f;
    bool roll_freeze = (rng() % 1000) / 1000.0f < freeze_chance;
    bool stack_freeze = (p->element.freeze_counter >= max_slow);

    if (roll_freeze || stack_freeze) {
        apply_buff(m, "freeze", 1);
        p->element.freeze_counter = 0;
        did_freeze = true;
        return true;
    }
    return false;
}

// ══════════════════════════════════════════════════════
// Poison: apply DOT based on damage × scale
// ══════════════════════════════════════════════════════
static void _resolve_poison(Player* p, Monster* m, int damage) {
    float scale = p->element.poison_dot_scale();
    int dot_tick = std::max(1, (int)(damage * scale));
    float dur = p->element.poison_dot_duration();
    // Create poison buff with scaled tick damage
    // Re-use the existing poison buff: tick_damage defaults to 3
    // But we need it scaled. We'll apply extra stacks to mimic scaling.
    int stacks = std::max(1, dot_tick / 3);
    apply_buff(m, "poison", stacks);
    (void)dur;
}

// ══════════════════════════════════════════════════════

bool ElementResolver::resolve(Player* player, Monster* monster,
                               int& damage, bool& is_crit, bool& did_freeze) {
    did_freeze = false;
    if (!player || !monster) return false;
    if (!player->element.initialized || player->element.type == ElementType::NONE)
        return false;

    switch (player->element.type) {
    case ElementType::FIRE:
        return _resolve_fire(player, damage, is_crit);
    case ElementType::ICE:
        return _resolve_ice(player, monster, did_freeze);
    case ElementType::POISON:
        _resolve_poison(player, monster, damage);
        return true;
    default: return false;
    }
}

void ElementResolver::on_hit(Player* player, Monster* monster) {
    if (!player || !monster) return;
    if (!player->element.initialized) return;
    int exp = monster->is_boss ? 10 : 1;
    player->element.add_exp(exp);
}

void ElementResolver::on_kill(Player* player, Monster* monster) {
    if (!player || !player->element.initialized) return;
    player->element.add_exp(5);
    (void)monster;
}
