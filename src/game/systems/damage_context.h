#pragma once
#include <string>

enum class DamageType {
    PHYSICAL,
    MAGICAL,
    POISON,
    FIRE,
    ICE,
    LIGHTNING,
    TRUE,
};

struct DamageContext {
    void* source = nullptr;
    void* target = nullptr;
    int raw_damage = 0;
    int final_damage = 0;
    DamageType damage_type = DamageType::PHYSICAL;
    bool cancelled = false;
};
