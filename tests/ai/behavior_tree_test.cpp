// G8.1: Behavior Tree unit tests — pure framework, zero game deps
#include <gtest/gtest.h>
#include "ai/behavior_tree/behavior_tree.h"

using namespace bt;

// ── Blackboard ────────────────────────────────────────────

TEST(Blackboard, SetAndGet) {
    Blackboard bb;
    bb.set("hp", 50.0f);
    EXPECT_FLOAT_EQ(bb.get<float>("hp"), 50.0f);
}

TEST(Blackboard, DefaultValue) {
    Blackboard bb;
    EXPECT_EQ(bb.get("missing", 42), 42);
}

TEST(Blackboard, Overwrite) {
    Blackboard bb;
    bb.set("x", 10);
    bb.set("x", 20);
    EXPECT_EQ(bb.get<int>("x"), 20);
}

TEST(Blackboard, Has) {
    Blackboard bb;
    EXPECT_FALSE(bb.has("key"));
    bb.set("key", std::string("val"));
    EXPECT_TRUE(bb.has("key"));
}

// ── Condition ─────────────────────────────────────────────

TEST(Condition, TruePredicate) {
    auto cond = Condition("AlwaysTrue", [](Blackboard&) { return true; });
    Blackboard bb;
    EXPECT_EQ(cond.tick(bb), Status::SUCCESS);
}

TEST(Condition, FalsePredicate) {
    auto cond = Condition("AlwaysFalse", [](Blackboard&) { return false; });
    Blackboard bb;
    EXPECT_EQ(cond.tick(bb), Status::FAILURE);
}

TEST(Condition, ReadsBlackboard) {
    auto cond = Condition("HPLow", [](Blackboard& b) {
        return b.get<float>("hp") < 30.0f;
    });
    Blackboard bb;
    bb.set("hp", 20.0f);
    EXPECT_EQ(cond.tick(bb), Status::SUCCESS);
    bb.set("hp", 50.0f);
    EXPECT_EQ(cond.tick(bb), Status::FAILURE);
}

// ── Action ────────────────────────────────────────────────

TEST(Action, ExecutesAndSucceeds) {
    int called = 0;
    auto act = Action("Counter", [&](Blackboard&) { called++; });
    Blackboard bb;
    EXPECT_EQ(act.tick(bb), Status::SUCCESS);
    EXPECT_EQ(called, 1);
}

TEST(Action, WritesBlackboard) {
    auto act = Action("SetAction", [](Blackboard& b) {
        b.set<std::string>("action", "attack");
    });
    Blackboard bb;
    act.tick(bb);
    EXPECT_EQ(bb.get<std::string>("action"), "attack");
}

// ── Selector ──────────────────────────────────────────────

TEST(Selector, FirstSuccessWins) {
    std::vector<std::unique_ptr<Node>> children;
    children.push_back(std::make_unique<Condition>("F", [](Blackboard&) { return false; }));
    children.push_back(std::make_unique<Condition>("T", [](Blackboard&) { return true; }));
    children.push_back(std::make_unique<Condition>("F2", [](Blackboard&) { return false; }));
    Selector sel(std::move(children));
    Blackboard bb;
    EXPECT_EQ(sel.tick(bb), Status::SUCCESS);
}

TEST(Selector, AllFailReturnsFailure) {
    std::vector<std::unique_ptr<Node>> children;
    children.push_back(std::make_unique<Condition>("F1", [](Blackboard&) { return false; }));
    children.push_back(std::make_unique<Condition>("F2", [](Blackboard&) { return false; }));
    Selector sel(std::move(children));
    Blackboard bb;
    EXPECT_EQ(sel.tick(bb), Status::FAILURE);
}

TEST(Selector, SecondChildNotCalledIfFirstSucceeds) {
    int called = 0;
    std::vector<std::unique_ptr<Node>> children;
    children.push_back(std::make_unique<Condition>("T", [](Blackboard&) { return true; }));
    children.push_back(std::make_unique<Action>("C", [&](Blackboard&) { called++; }));
    Selector sel(std::move(children));
    Blackboard bb;
    sel.tick(bb);
    EXPECT_EQ(called, 0); // never reached
}

// ── Sequence ──────────────────────────────────────────────

TEST(Sequence, BothSucceedReturnsSuccess) {
    std::vector<std::unique_ptr<Node>> children;
    children.push_back(std::make_unique<Condition>("T1", [](Blackboard&) { return true; }));
    children.push_back(std::make_unique<Condition>("T2", [](Blackboard&) { return true; }));
    Sequence seq(std::move(children));
    Blackboard bb;
    EXPECT_EQ(seq.tick(bb), Status::SUCCESS);
}

TEST(Sequence, FirstFailStops) {
    int called = 0;
    std::vector<std::unique_ptr<Node>> children;
    children.push_back(std::make_unique<Condition>("F", [](Blackboard&) { return false; }));
    children.push_back(std::make_unique<Action>("C", [&](Blackboard&) { called++; }));
    Sequence seq(std::move(children));
    Blackboard bb;
    EXPECT_EQ(seq.tick(bb), Status::FAILURE);
    EXPECT_EQ(called, 0);
}

// ── Integrated: Boot agent behavior ───────────────────────

TEST(IntegratedTree, HealWhenLowHP) {
    // Sequence: HPLow → HealReady → UseHeal
    Blackboard bb;
    bb.set("hp_ratio", 0.20f);
    bb.set("heal_ready", true);

    std::vector<std::unique_ptr<Node>> children;
    children.push_back(std::make_unique<Condition>("HPLow",
        [](Blackboard& b) { return b.get<float>("hp_ratio") < 0.35f; }));
    children.push_back(std::make_unique<Condition>("HealOK",
        [](Blackboard& b) { return b.get("heal_ready", false); }));
    children.push_back(std::make_unique<Action>("Heal",
        [](Blackboard& b) { b.set<std::string>("action", "skill_3"); }));
    Sequence healSeq(std::move(children));

    EXPECT_EQ(healSeq.tick(bb), Status::SUCCESS);
    EXPECT_EQ(bb.get<std::string>("action"), "skill_3");
}

TEST(IntegratedTree, BossFightPriority) {
    // Selector: BossIntro → Stairs → Heal → Combat → Wander
    Blackboard bb;
    bb.set("boss_intro", true);
    bb.set("stairs_active", false);
    bb.set("hp_ratio", 0.80f);
    bb.set("enemy_near", true);

    std::vector<std::unique_ptr<Node>> root;
    root.push_back(std::make_unique<Condition>("BossIntro",
        [](Blackboard& b) { return b.get("boss_intro", false); }));
    root.push_back(std::make_unique<Condition>("Stairs",
        [](Blackboard& b) { return b.get("stairs_active", false); }));
    root.push_back(std::make_unique<Action>("Confirm",
        [](Blackboard& b) { b.set<std::string>("action", "confirm"); }));

    Selector sel(std::move(root));
    EXPECT_EQ(sel.tick(bb), Status::SUCCESS);
    // BossIntro succeeded but Confirm is separate — the selector returns
    // on first success (BossIntro condition), never reaching Confirm
}
