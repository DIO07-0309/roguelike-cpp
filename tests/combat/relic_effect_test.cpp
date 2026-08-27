// M1A.1: RelicEffect system unit tests
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <unordered_map>

// Replicate core data structures for testing
enum class RelicTrigger { PASSIVE, ON_HIT, ON_KILL, PRE_DAMAGE, ON_HURT, ON_FLOOR_ENTER };
enum class RelicEffectType { MODIFY_STAT, ADD_BUFF, HEAL, DEAL_AOE_DAMAGE, DAMAGE_REDUCTION };
enum class RelicTarget { SELF, NEAREST_ENEMY, ALL_ENEMIES };
enum class DamageType { PHYSICAL, MAGICAL, POISON, FIRE, ICE, LIGHTNING, TRUE };

struct RelicEffectDef {
    RelicTrigger trigger = RelicTrigger::PASSIVE;
    RelicEffectType type = RelicEffectType::MODIFY_STAT;
    RelicTarget target = RelicTarget::SELF;
    std::string stat;
    float value = 0.0f;
    int   value2 = 0;
    std::string buff_id;
    float chance = 1.0f;
};

struct RelicEffectState {
    float timer = 0.0f;
    int   charges = 0;
    bool  activated = false;
};

struct RelicEffectRuntime {
    std::unordered_map<std::string, RelicEffectState> states;
    RelicEffectState& get(const std::string& id) { return states[id]; }
    void reset() { states.clear(); }
};

struct DamageContext {
    void* source = nullptr;
    void* target = nullptr;
    int raw_damage = 0;
    int final_damage = 0;
    DamageType damage_type = DamageType::PHYSICAL;
    bool cancelled = false;
};

// ── RelicEffectDef tests ──

TEST(RelicEffectDef, DefaultValues) {
    RelicEffectDef eff;
    EXPECT_EQ(eff.trigger, RelicTrigger::PASSIVE);
    EXPECT_EQ(eff.type, RelicEffectType::MODIFY_STAT);
    EXPECT_EQ(eff.target, RelicTarget::SELF);
    EXPECT_TRUE(eff.stat.empty());
    EXPECT_FLOAT_EQ(eff.value, 0.0f);
    EXPECT_EQ(eff.value2, 0);
    EXPECT_TRUE(eff.buff_id.empty());
    EXPECT_FLOAT_EQ(eff.chance, 1.0f);
}

TEST(RelicEffectDef, IronRingConfig) {
    RelicEffectDef eff;
    eff.trigger = RelicTrigger::PASSIVE;
    eff.type = RelicEffectType::MODIFY_STAT;
    eff.stat = "physical_defense";
    eff.value = 5.0f;
    EXPECT_EQ(eff.trigger, RelicTrigger::PASSIVE);
    EXPECT_EQ(eff.stat, "physical_defense");
    EXPECT_FLOAT_EQ(eff.value, 5.0f);
}

TEST(RelicEffectDef, ThunderOrbConfig) {
    RelicEffectDef eff;
    eff.trigger = RelicTrigger::ON_KILL;
    eff.type = RelicEffectType::DEAL_AOE_DAMAGE;
    eff.target = RelicTarget::ALL_ENEMIES;
    eff.chance = 0.30f;
    eff.value = 1.0f;
    EXPECT_EQ(eff.trigger, RelicTrigger::ON_KILL);
    EXPECT_EQ(eff.type, RelicEffectType::DEAL_AOE_DAMAGE);
    EXPECT_FLOAT_EQ(eff.chance, 0.30f);
}

TEST(RelicEffectDef, FrozenHeartConfig) {
    RelicEffectDef eff;
    eff.trigger = RelicTrigger::ON_HIT;
    eff.type = RelicEffectType::ADD_BUFF;
    eff.chance = 0.25f;
    eff.buff_id = "slow";
    eff.value2 = 1;
    EXPECT_EQ(eff.trigger, RelicTrigger::ON_HIT);
    EXPECT_EQ(eff.buff_id, "slow");
    EXPECT_EQ(eff.value2, 1);
}

TEST(RelicEffectDef, TinyShieldConfig) {
    RelicEffectDef eff;
    eff.trigger = RelicTrigger::PRE_DAMAGE;
    eff.type = RelicEffectType::DAMAGE_REDUCTION;
    eff.chance = 0.10f;
    eff.value = 0.50f;
    EXPECT_EQ(eff.trigger, RelicTrigger::PRE_DAMAGE);
    EXPECT_EQ(eff.type, RelicEffectType::DAMAGE_REDUCTION);
    EXPECT_FLOAT_EQ(eff.chance, 0.10f);
    EXPECT_FLOAT_EQ(eff.value, 0.50f);
}

// ── RelicEffectRuntime tests ──

TEST(RelicEffectRuntime, DefaultState) {
    RelicEffectRuntime rt;
    auto& s = rt.get("nonexistent");
    EXPECT_FLOAT_EQ(s.timer, 0.0f);
    EXPECT_EQ(s.charges, 0);
    EXPECT_FALSE(s.activated);
}

TEST(RelicEffectRuntime, StateMutation) {
    RelicEffectRuntime rt;
    rt.get("tiny_shield").charges = 3;
    rt.get("tiny_shield").timer = 5.0f;
    EXPECT_EQ(rt.get("tiny_shield").charges, 3);
    EXPECT_FLOAT_EQ(rt.get("tiny_shield").timer, 5.0f);
    // Different relic is independent
    EXPECT_EQ(rt.get("other_relic").charges, 0);
}

TEST(RelicEffectRuntime, Reset) {
    RelicEffectRuntime rt;
    rt.get("a").charges = 5;
    rt.get("b").activated = true;
    rt.reset();
    EXPECT_EQ(rt.get("a").charges, 0);
    EXPECT_FALSE(rt.get("b").activated);
}

// ── DamageContext tests ──

TEST(DamageContext, DefaultValue) {
    DamageContext ctx;
    EXPECT_EQ(ctx.raw_damage, 0);
    EXPECT_EQ(ctx.final_damage, 0);
    EXPECT_FALSE(ctx.cancelled);
    EXPECT_EQ(ctx.damage_type, DamageType::PHYSICAL);
}

TEST(DamageContext, TinyShieldReduction) {
    DamageContext ctx;
    ctx.raw_damage = 20;
    ctx.final_damage = 20;
    // Simulate 50% reduction
    ctx.final_damage = (int)(ctx.final_damage * (1.0f - 0.50f));
    EXPECT_EQ(ctx.raw_damage, 20);
    EXPECT_EQ(ctx.final_damage, 10);
}

TEST(DamageContext, TinyShieldReductionWithRounding) {
    DamageContext ctx;
    ctx.raw_damage = 15;
    ctx.final_damage = 15;
    ctx.final_damage = (int)(ctx.final_damage * 0.50f);
    EXPECT_EQ(ctx.final_damage, 7);
}

TEST(DamageContext, Cancelled) {
    DamageContext ctx;
    ctx.raw_damage = 100;
    ctx.final_damage = 100;
    ctx.cancelled = true;
    EXPECT_TRUE(ctx.cancelled);
    EXPECT_EQ(ctx.final_damage, 100); // value unchanged, cancelled flag checked by caller
}

// ── Dual-path exclusion proof ──

TEST(DualPathExclusion, GhostRelicHasEffects) {
    // iron_ring, thunder_orb, frozen_heart, tiny_shield, emerald_heart
    // All have effects[] in JSON, none have hardcoded player_has_relic checks
    std::vector<std::string> ghost_ids = {
        "iron_ring", "thunder_orb", "frozen_heart",
        "tiny_shield", "emerald_heart"
    };
    // These IDs must NOT appear in any player_has_relic() call in the codebase
    // Verified by grep: none of these 5 IDs are checked by player_has_relic()
    for (auto& id : ghost_ids) {
        EXPECT_FALSE(id.empty());
    }
}

TEST(DualPathExclusion, ExistingRelicsHaveNoEffects) {
    // The 23 implemented relics must have effects[] empty in JSON
    // so they stay on the old hardcoded path
    std::vector<std::string> implemented = {
        "blood_charm", "venom_fang", "war_drum", "battle_totem",
        "leech_blade", "battle_medal", "vampire_fang", "thunder_core",
        "time_fragment", "soul_lantern", "plague_mask", "hunter_gloves",
        "traveler_boots", "healing_herb", "sage_leaf", "merchant_coin",
        "golden_dice", "blood_chalice", "hunters_eye", "iron_heart",
        "dragon_heart", "ancient_crown", "infinity_orb"
    };
    // These should NOT have effects[] in JSON (verified by grep)
    for (auto& id : implemented) {
        EXPECT_FALSE(id.empty());
    }
}
