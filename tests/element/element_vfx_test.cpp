// G10.3: Element VFX event tests
#include <gtest/gtest.h>
#include "core/event_types.h"
#include "components/element_component.h"
#include "data/element_defs.h"

// Verify all element VFX event types exist and are distinct

TEST(ElementVFX, AllEventTypesDefined) {
    GameEventType events[] = {
        GameEventType::ELEMENT_FIRE_HIT,
        GameEventType::ELEMENT_FIRE_CRITICAL,
        GameEventType::ELEMENT_ICE_SLOW,
        GameEventType::ELEMENT_ICE_FREEZE,
        GameEventType::ELEMENT_POISON_APPLY,
        GameEventType::ELEMENT_POISON_TICK,
        GameEventType::ELEMENT_LEVEL_UP,
    };
    for (auto e : events) {
        EXPECT_NE(e, GameEventType::NONE);
        EXPECT_NE(e, GameEventType::COUNT);
    }
}

TEST(ElementVFX, EventsAreDistinct) {
    int values[] = {
        (int)GameEventType::ELEMENT_FIRE_HIT,
        (int)GameEventType::ELEMENT_FIRE_CRITICAL,
        (int)GameEventType::ELEMENT_ICE_SLOW,
        (int)GameEventType::ELEMENT_ICE_FREEZE,
        (int)GameEventType::ELEMENT_POISON_APPLY,
        (int)GameEventType::ELEMENT_POISON_TICK,
        (int)GameEventType::ELEMENT_LEVEL_UP,
    };
    for (int i = 0; i < 7; i++)
        for (int j = i + 1; j < 7; j++)
            EXPECT_NE(values[i], values[j]);
}

// Verify VFX recipe IDs are loaded from elements.json

class ElementVFXData : public ::testing::Test {
protected:
    void SetUp() override { load_element_defs("resources/elements.json"); }
};

TEST_F(ElementVFXData, FireHasVFXRecipes) {
    const ElementDef* d = get_element_def("fire");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->vfx.hit, "fire_hit");
    EXPECT_EQ(d->vfx.critical, "fire_critical");
    EXPECT_EQ(d->vfx.level_up, "fire_upgrade");
}

TEST_F(ElementVFXData, IceHasVFXRecipes) {
    const ElementDef* d = get_element_def("ice");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->vfx.slow, "ice_slow");
    EXPECT_EQ(d->vfx.freeze, "ice_freeze");
}

TEST_F(ElementVFXData, PoisonHasVFXRecipes) {
    const ElementDef* d = get_element_def("poison");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->vfx.apply, "poison_apply");
    EXPECT_EQ(d->vfx.tick, "poison_tick");
}

// Verify GameEvent struct works for element payloads

TEST(ElementVFX, GameEventPayload) {
    GameEvent ev;
    ev.type = GameEventType::ELEMENT_FIRE_CRITICAL;
    ev.int_val = 150;       // damage
    ev.float_val = 1.5f;    // multiplier
    ev.str_val = "orc";     // target name
    EXPECT_EQ(ev.type, GameEventType::ELEMENT_FIRE_CRITICAL);
    EXPECT_EQ(ev.int_val, 150);
    EXPECT_FLOAT_EQ(ev.float_val, 1.5f);
    EXPECT_STREQ(ev.str_val, "orc");
}

TEST(ElementVFX, LevelUpEventPayload) {
    GameEvent ev;
    ev.type = GameEventType::ELEMENT_LEVEL_UP;
    ev.int_val = 5;             // new level
    ev.str_val = "fire";        // element name
    EXPECT_EQ(ev.int_val, 5);
    EXPECT_STREQ(ev.str_val, "fire");
}
