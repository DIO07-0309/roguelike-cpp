#include <gtest/gtest.h>
#include "entities/player.h"
#include "entities/combat_stats.h"
#include <algorithm>

// Round-trip test: PersistenceScope + Floor Transition
TEST(RoundTripTest, FloorTransitionKeepsRunRelics) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.gold = 123;
    p.key_count = 4;
    p.relics.push_back({"relic_a", PersistenceScope::FLOOR});
    p.relics.push_back({"relic_b", PersistenceScope::RUN});
    p.relics.push_back({"relic_c", PersistenceScope::RUN});

    // Simulate floor transition
    p.relics.erase(
        std::remove_if(p.relics.begin(), p.relics.end(),
            [](const RelicInstance& r) {
                return r.scope == PersistenceScope::FLOOR;
            }),
        p.relics.end());

    // Verify
    EXPECT_EQ(p.gold, 123);
    EXPECT_EQ(p.key_count, 4);
    EXPECT_EQ(p.relics.size(), 2u);
    EXPECT_EQ(p.relics[0].id, "relic_b");
    EXPECT_EQ(p.relics[0].scope, PersistenceScope::RUN);
    EXPECT_EQ(p.relics[1].id, "relic_c");
    EXPECT_EQ(p.relics[1].scope, PersistenceScope::RUN);
}

TEST(RoundTripTest, AllFloorRelicsRemoved) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.relics.push_back({"a", PersistenceScope::FLOOR});
    p.relics.push_back({"b", PersistenceScope::FLOOR});

    p.relics.erase(
        std::remove_if(p.relics.begin(), p.relics.end(),
            [](const RelicInstance& r) {
                return r.scope == PersistenceScope::FLOOR;
            }),
        p.relics.end());

    EXPECT_EQ(p.relics.size(), 0u);
}

TEST(RoundTripTest, EmptyRelicsNoCrash) {
    Player p(0, 0, 200, 100, 10, 5, 3);

    p.relics.erase(
        std::remove_if(p.relics.begin(), p.relics.end(),
            [](const RelicInstance& r) {
                return r.scope == PersistenceScope::FLOOR;
            }),
        p.relics.end());

    EXPECT_EQ(p.relics.size(), 0u);
}
