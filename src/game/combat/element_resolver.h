#pragma once

class Player;
class Monster;

// ============================================================
// G10.3: ElementResolver — single entry for element combat effects
// No Manager. Pure functional. Composes with existing BuffSystem.
//
// Usage:
//   WeaponExecutor calculates damage →
//   ElementResolver::resolve(player, monster, &damage, &is_crit, &did_freeze) →
//   WeaponExecutor applies final damage + buffs
// ============================================================

class ElementResolver {
public:
    // Called after damage calculation, before take_damage.
    // May modify damage in-place (Fire crit) or apply buffs (Ice/Poison).
    // Returns true if a freeze triggered (for VFX/message).
    static bool resolve(Player* player, Monster* monster,
                        int& damage, bool& is_crit, bool& did_freeze);

    // Add element EXP on successful hit
    static void on_hit(Player* player, Monster* monster);

    // Add element EXP on kill (extra)
    static void on_kill(Player* player, Monster* monster);
};
