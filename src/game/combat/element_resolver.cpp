#include "combat/element_resolver.h"
#include "entities/player.h"
#include "entities/monster.h"
#include "components/element_component.h"
#include "data/element_defs.h"
#include "systems/combat_system.h"
#include "core/event_bus.h"         // G10.3 VFX events

// ══════════════════════════════════════════════════════
// Fire: crit roll → damage ×multiplier
// ══════════════════════════════════════════════════════
static bool _resolve_fire(Player* p, Monster* m, int& damage, bool& is_crit) {
    // Always emit fire hit VFX
    EventBus::inst().emit(GameEventType::ELEMENT_FIRE_HIT, p, damage, 0.0f,
        m ? m->name.c_str() : nullptr);

    float chance = p->element.fire_crit_chance() / 100.0f;
    if ((rng() % 1000) / 1000.0f >= chance) return false;
    float mult = p->element.fire_crit_mult();
    damage = (int)(damage * mult);
    is_crit = true;
    // Fire crit VFX
    EventBus::inst().emit(GameEventType::ELEMENT_FIRE_CRITICAL, p, damage, mult,
        m ? m->name.c_str() : nullptr);
    return true;
}

// ══════════════════════════════════════════════════════
// Ice: apply slow, track freeze counter, trigger freeze
// ══════════════════════════════════════════════════════
static bool _resolve_ice(Player* p, Monster* m, bool& did_freeze) {
    // Always apply slow + emit VFX
    apply_buff(m, "slow", 1);
    EventBus::inst().emit(GameEventType::ELEMENT_ICE_SLOW, p, 0, 0.0f,
        m ? m->name.c_str() : nullptr);

    p->element.freeze_counter++;
    int max_slow = p->element.ice_freeze_counter_max();
    float freeze_chance = p->element.ice_freeze_chance() / 100.0f;
    bool roll_freeze = (rng() % 1000) / 1000.0f < freeze_chance;
    bool stack_freeze = (p->element.freeze_counter >= max_slow);

    if (roll_freeze || stack_freeze) {
        apply_buff(m, "freeze", 1);
        p->element.freeze_counter = 0;
        did_freeze = true;
        EventBus::inst().emit(GameEventType::ELEMENT_ICE_FREEZE, p, 0, 0.0f,
            m ? m->name.c_str() : nullptr);
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
    int stacks = std::max(1, dot_tick / 3);
    apply_buff(m, "poison", stacks);

    // Emit poison apply VFX
    EventBus::inst().emit(GameEventType::ELEMENT_POISON_APPLY, p, stacks, scale,
        m ? m->name.c_str() : nullptr);
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
        return _resolve_fire(player, monster, damage, is_crit);
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
