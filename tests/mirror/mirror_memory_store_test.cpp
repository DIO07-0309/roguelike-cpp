// B3-M: MirrorMemoryStore — CloneTable 跨局持久化闭环测试
#include <gtest/gtest.h>
#include <cstdio>
#include "ai/mirror/behavior_clone_table.h"
#include "ai/mirror/mirror_memory_store.h"

namespace {
const char* kTestPath = "saves/test_mirror_memory.json";

struct TestPathGuard {
    TestPathGuard() { mirror::MirrorMemoryStore::set_path_for_test(kTestPath); }
    ~TestPathGuard() {
        std::remove(kTestPath);
        std::remove("saves/test_mirror_memory.json.tmp");
        mirror::MirrorMemoryStore::set_path_for_test(
            "saves/mirror_memory.json");   // 还原生产默认
    }
};

CloneContext lowHpClose() { return CloneContext::from_state(1.5f, 0.2f, 3); }

void bump(BehaviorCloneTable& t, PlayerIntention intent, int n) {
    for (int i = 0; i < n; i++) t.record_decision(lowHpClose(), intent);
}

const BehaviorCloneTable::Counts* find_entry(const BehaviorCloneTable& t) {
    auto it = t.table().find(lowHpClose().key());
    return it == t.table().end() ? nullptr : &it->second;
}
}  // namespace

TEST(MirrorMemoryStore, SaveLoadRoundTripKeepsPrediction) {
    TestPathGuard guard;
    BehaviorCloneTable src;
    bump(src, PlayerIntention::HEAL, 40);
    bump(src, PlayerIntention::ATTACK, 10);
    ASSERT_TRUE(mirror::MirrorMemoryStore::save_from(src));

    BehaviorCloneTable dst;
    ASSERT_TRUE(mirror::MirrorMemoryStore::load_into(dst));
    ClonePrediction p = dst.predict(1.5f, 0.2f, 3);
    EXPECT_EQ(p.best, PlayerIntention::HEAL);
    EXPECT_EQ(p.level, 0);                        // exact 命中
}

TEST(MirrorMemoryStore, LoadAppliesOncePerRunDecay) {
    TestPathGuard guard;
    BehaviorCloneTable src;
    bump(src, PlayerIntention::HEAL, 100);
    bump(src, PlayerIntention::ATTACK, 50);
    ASSERT_TRUE(mirror::MirrorMemoryStore::save_from(src));

    BehaviorCloneTable dst;
    ASSERT_TRUE(mirror::MirrorMemoryStore::load_into(dst));
    const auto* c = find_entry(dst);              // 100→99, 50→49 (×0.99 截断)
    ASSERT_NE(c, nullptr);
    EXPECT_EQ((*c)[(int)PlayerIntention::HEAL], 99);
    EXPECT_EQ((*c)[(int)PlayerIntention::ATTACK], 49);
}

TEST(MirrorMemoryStore, MergeAccumulatesOntoCurrentRun) {
    TestPathGuard guard;
    BehaviorCloneTable src;
    bump(src, PlayerIntention::HEAL, 100);
    ASSERT_TRUE(mirror::MirrorMemoryStore::save_from(src));

    BehaviorCloneTable dst;                       // 本局先学了 2 次
    bump(dst, PlayerIntention::HEAL, 2);
    ASSERT_TRUE(mirror::MirrorMemoryStore::load_into(dst));
    const auto* c = find_entry(dst);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ((*c)[(int)PlayerIntention::HEAL], 101);  // 2 + 99
}

TEST(MirrorMemoryStore, TinyOldMemoryDecaysOut) {
    TestPathGuard guard;
    BehaviorCloneTable src;
    bump(src, PlayerIntention::HEAL, 1);          // 单次证据
    ASSERT_TRUE(mirror::MirrorMemoryStore::save_from(src));

    BehaviorCloneTable dst;
    EXPECT_FALSE(mirror::MirrorMemoryStore::load_into(dst));  // 1×0.99→0
    EXPECT_EQ(dst.entries(), 0u);
}

TEST(MirrorMemoryStore, MissingAndCorruptFilesAreSafe) {
    TestPathGuard guard;
    BehaviorCloneTable dst;
    std::remove(kTestPath);
    EXPECT_FALSE(mirror::MirrorMemoryStore::load_into(dst));

    FILE* f = fopen(kTestPath, "wb");             // 垃圾内容不得崩溃
    ASSERT_NE(f, nullptr);
    fputs("{not json", f);
    fclose(f);
    EXPECT_FALSE(mirror::MirrorMemoryStore::load_into(dst));
    EXPECT_EQ(dst.entries(), 0u);
}
