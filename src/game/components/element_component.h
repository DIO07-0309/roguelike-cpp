#pragma once
#include <string>

// ============================================================
// G10.1: ElementComponent — 玩家永久元素核心
// composited into Player, 新游戏时选择, 跨存档持久化
// ============================================================

enum class ElementType : int {
    NONE = 0,
    FIRE,
    ICE,
    POISON
};

inline const char* element_type_name(ElementType e) {
    switch (e) {
    case ElementType::FIRE:   return "fire";
    case ElementType::ICE:    return "ice";
    case ElementType::POISON: return "poison";
    default: return "none";
    }
}

inline ElementType element_from_string(const char* s) {
    if (!s) return ElementType::NONE;
    std::string t(s);
    if (t == "fire")   return ElementType::FIRE;
    if (t == "ice")    return ElementType::ICE;
    if (t == "poison") return ElementType::POISON;
    return ElementType::NONE;
}

// Lightweight data structure (composited, not inherited)
struct ElementComponent {
    ElementType type = ElementType::NONE;
    int level = 1;
    int experience = 0;
    bool initialized = false; // true after first element selection

    void select(ElementType element);
    void add_exp(int amount);
    int  xp_to_next() const;

    static constexpr int BASE_XP = 100;
};
