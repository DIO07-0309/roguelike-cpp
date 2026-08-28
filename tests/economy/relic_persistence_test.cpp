#include <gtest/gtest.h>
#include "entities/combat_stats.h"

TEST(PersistenceScopeTest, FloorDefault) {
    RelicInstance r{"test_id"};
    EXPECT_EQ(r.scope, PersistenceScope::FLOOR);
}

TEST(PersistenceScopeTest, RunScope) {
    RelicInstance r{"boss_relic", PersistenceScope::RUN};
    EXPECT_EQ(r.scope, PersistenceScope::RUN);
}

TEST(PersistenceScopeTest, FloorScopeExplicit) {
    RelicInstance r{"temp_relic", PersistenceScope::FLOOR};
    EXPECT_EQ(r.scope, PersistenceScope::FLOOR);
}

TEST(PersistenceScopeTest, ScopeValues) {
    EXPECT_EQ(static_cast<uint8_t>(PersistenceScope::FLOOR), 0);
    EXPECT_EQ(static_cast<uint8_t>(PersistenceScope::RUN), 1);
}
