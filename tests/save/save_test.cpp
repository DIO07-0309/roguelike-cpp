// G7.2: Save/Load roundtrip regression test
// v1.0.0 Save Stable 验收: 全字段 roundtrip + v1 旧版兼容 + 容错
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <direct.h>
#include <cstdio>
#include <vector>

#include "player.h"
#include "skill.h"
#include "item.h"
#include "save_manager.h"
#include "data/skill_defs.h"

extern bool load_biome_defs(const char* path);
extern bool load_encounter_defs(const char* path);

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

TEST(SaveFormat, RoundTripWriteRead) {
    _mkdir("saves");
    const char* path = "saves/test_save.json";
    std::ofstream w(path);
    w << "{\"test\":1}";
    w.close();
    std::string content = read_file(path);
    EXPECT_GT(content.size(), 2u);
    EXPECT_EQ(content[0], '{');
    std::remove(path);
}

TEST(SaveFormat, MetaSaveRoundTrip) {
    _mkdir("saves");
    const char* path = "saves/test_meta_save.json";
    std::ofstream w(path);
    w << "{\"meta\":true}";
    w.close();
    std::string content = read_file(path);
    EXPECT_GT(content.size(), 2u);
    EXPECT_EQ(content[0], '{');
    std::remove(path);
}

TEST(SaveRoundtrip, ResourcesAndSavesCoexist) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    _mkdir("saves");
    const char* path = "saves/test_save2.json";
    std::ofstream w(path);
    w << "{}";
    w.close();
    EXPECT_TRUE(std::ifstream(path).is_open()) << "Save missing after resource load";
    std::remove(path);
}

// ── v1.0.0: Save Stable 验收 ─────────────────────────────────

static bool write_raw_save(const std::string& content) {
    FILE* f = fopen("saves/save.json", "w");
    if (!f) return false;
    fwrite(content.c_str(), 1, content.size(), f);
    fclose(f);
    return true;
}

// 测试会覆写 saves/save.json — 备份真实存档并在结束时恢复
class SaveFileGuard {
public:
    SaveFileGuard() {
        _had = std::ifstream("saves/save.json").is_open();
        if (_had) std::rename("saves/save.json", "saves/save.json.bak_test");
    }
    ~SaveFileGuard() {
        std::remove("saves/save.json");
        if (_had) std::rename("saves/save.json.bak_test", "saves/save.json");
    }
private:
    bool _had = false;
};

// 旧版 (v1 只有核心字段) 存档 → 必须安全加载, v2/v3/新字段取默认值
TEST(SaveStable, V1LegacyLoadsWithDefaults) {
    _mkdir("saves");
    SaveFileGuard guard;
    const char* legacy =
        "v:1\n"
        "floor:3\nmaxf:3\nlv:2\nxp:30\nxpt:60\n"
        "mhp:100\nchp:70\natk:11\npd:5\nmd:3\n"
        "act:slash,1,0,0;\n"
        "pas:\ninv:\n"
        "eqw:\nwpn:\neqa:\n"
        "buf:\nseed:42\nspr:\nspd:\n";
    ASSERT_TRUE(write_raw_save(legacy));
    SaveData* d = SaveManager::load_save();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->current_floor, 3);
    EXPECT_EQ(d->max_unlocked_floor, 3);
    EXPECT_EQ(d->dungeon_seed, 42u);
    EXPECT_EQ(d->attack_evo_level, 1);            // v2 字段默认
    EXPECT_TRUE(d->quest_states.empty());         // v3 字段默认
    EXPECT_TRUE(d->unlocked_endings.empty());     // v3 字段默认
    EXPECT_FALSE(d->element_initialized);         // G10.1 默认
    EXPECT_TRUE(d->mirror_prior_alpha.empty());   // M4e 默认
    ASSERT_NE(d->player, nullptr);
    EXPECT_EQ(d->player->level, 2);
    EXPECT_EQ(d->player->combat.current_hp, 70);
    delete d;
}

// 坏条目 (未知技能/坏 buff/未知行) → 跳过不崩溃
TEST(SaveStable, CorruptLinesTolerated) {
    _mkdir("saves");
    ASSERT_TRUE(load_skill_defs("resources/skills.json"));
    SaveFileGuard guard;
    const char* junk =
        "v:3\n"
        "floor:1\nmaxf:1\nlv:1\nxp:0\nxpt:50\n"
        "mhp:100\nchp:100\natk:11\npd:5\nmd:3\n"
        "act:slash,1,0,0;not_a_skill,2,0,0;\n"
        "buf:poison,1,5.00,0.00;BAD;;;\n"
        "unknown_field:whatever\n"
        "seed:7\nspr:!!\nspd:\n";
    ASSERT_TRUE(write_raw_save(junk));
    SaveData* d = SaveManager::load_save();
    ASSERT_NE(d, nullptr);
    ASSERT_NE(d->player, nullptr);
    EXPECT_GE(d->player->skills.active_skills.size(), 1u);  // slash 恢复
    delete d;
}

// 全字段 roundtrip: save_game → load_save 逐字段一致
TEST(SaveStable, FullRoundtrip) {
    _mkdir("saves");
    ASSERT_TRUE(load_skill_defs("resources/skills.json"));
    SaveFileGuard guard;
    Player p(64.0f, 64.0f, 220.0f, 100, 11, 5, 3);
    p.combat.current_hp = 77;
    p.level = 4;
    p.xp = 120;
    p.xp_to_next = 200;
    auto sk = skill_factory_create("slash");
    ASSERT_NE(sk, nullptr);
    p.skills.learn(std::move(sk));
    p.inventory.items.push_back(
        std::make_shared<ConsumableItem>("治疗药水", Rarity::RARE, "heal", 30, ""));
    p.inventory.equipped["weapon"] = std::make_shared<EquipmentItem>(
        "铁剑", Rarity::COMMON, "weapon", 5, 0, 0, false);
    p.inventory.equipped["weapon"]->weapon_def_id = "iron_sword";
    p.element.select(ElementType::FIRE);
    p.element.add_exp(45);

    bool ok = SaveManager::save_game(&p, 5, 5, 99u,
        {true, false}, {false, true}, {{"rule_1", 3}},
        {{101, 2}}, {1, 2}, {0.5f, 1.0f}, {2.0f, 3.0f});
    ASSERT_TRUE(ok);

    SaveData* d = SaveManager::load_save();
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->current_floor, 5);
    EXPECT_EQ(d->max_unlocked_floor, 5);
    EXPECT_EQ(d->dungeon_seed, 99u);
    EXPECT_EQ(d->attack_evo_level, 1);
    EXPECT_EQ(d->quest_states[101], 2);
    ASSERT_EQ(d->unlocked_endings.size(), 2u);
    EXPECT_EQ(d->unlocked_endings[0], 1);
    EXPECT_EQ(d->unlocked_endings[1], 2);
    ASSERT_EQ(d->mirror_prior_alpha.size(), 2u);
    EXPECT_FLOAT_EQ(d->mirror_prior_alpha[1], 1.0f);
    ASSERT_EQ(d->mirror_prior_beta.size(), 2u);
    EXPECT_FLOAT_EQ(d->mirror_prior_beta[0], 2.0f);
    EXPECT_TRUE(d->element_initialized);
    EXPECT_EQ(d->element_type, (int)ElementType::FIRE);

    ASSERT_NE(d->player, nullptr);
    EXPECT_EQ(d->player->level, 4);
    EXPECT_EQ(d->player->xp, 120);
    EXPECT_EQ(d->player->combat.current_hp, 77);
    EXPECT_GE(d->player->skills.active_skills.size(), 1u);
    EXPECT_EQ(d->player->element.type, ElementType::FIRE);
    delete d;
}
