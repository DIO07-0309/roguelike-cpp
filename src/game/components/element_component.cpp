#include "components/element_component.h"

void ElementComponent::select(ElementType element) {
    type = element;
    level = 1;
    experience = 0;
    initialized = true;
}

void ElementComponent::add_exp(int amount) {
    if (!initialized || type == ElementType::NONE) return;
    experience += amount;
    while (experience >= xp_to_next()) {
        experience -= xp_to_next();
        level++;
    }
}

int ElementComponent::xp_to_next() const {
    return BASE_XP * level;
}
