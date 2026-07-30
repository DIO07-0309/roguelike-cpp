#include "components/element_component.h"
#include "data/element_defs.h"

void ElementComponent::select(ElementType element) {
    type = element;
    level = 1;
    experience = 0;
    freeze_counter = 0;
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

// ── G10.3: Combat stat queries ──

float ElementComponent::fire_crit_chance() const {
    const ElementDef* d = get_element_def(element_type_name(type));
    if (!d) return 0.0f;
    return d->crit_base + d->crit_growth * (level - 1);
}

float ElementComponent::fire_crit_mult() const {
    const ElementDef* d = get_element_def(element_type_name(type));
    return d ? d->crit_multiplier : 1.5f;
}

float ElementComponent::ice_freeze_chance() const {
    const ElementDef* d = get_element_def(element_type_name(type));
    if (!d) return 0.0f;
    float t = (float)(level - 1) / 19.0f; // 0 at Lv1, 1 at Lv20
    return d->freeze_stage1 + (d->freeze_stage3 - d->freeze_stage1) * t;
}

int ElementComponent::ice_freeze_counter_max() const {
    const ElementDef* d = get_element_def(element_type_name(type));
    return d ? d->freeze_counter_max : 3;
}

float ElementComponent::poison_dot_scale() const {
    const ElementDef* d = get_element_def(element_type_name(type));
    if (!d) return 0.0f;
    return d->dot_scale_base + d->dot_scale_growth * (level - 1);
}

float ElementComponent::poison_dot_duration() const {
    const ElementDef* d = get_element_def(element_type_name(type));
    return d ? d->dot_duration : 3.0f;
}
