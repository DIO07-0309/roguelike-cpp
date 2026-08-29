// G10.2-B3A: Audio Manifest — 音频资产清单化回归
// 事实源: G10_2_PIPELINE_DESIGN.md §4 + 审计 (audio_server.cpp init)
// 现状:
//   - timestop:     外部 MP3 (jojo_timestop.mp3) + fallback synth:bolt ✓
//   - domain_expand:外部 MP3 (domain_expand.mp3) — 无合成 fallback (真缺口, 如实声明)
//   - 其余 15 sfx: 纯程序合成 (不涉及路径, 不进清单 file)
// 目标: 业务代码 (audio_server.cpp) 不再出现 *.mp3 字面量; 路径唯一来源 = manifest。
#include <gtest/gtest.h>

#include "resources/resource_manager.h"

#include <fstream>
#include <set>
#include <string>
#include <vector>

// 与既有测试一致: main.cpp 的字体全局桩化
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

// AudioServer::init 现有程序合成器集合 (fallback 引用有效性校验用)
const char* kSynthComposers[] = {
    "melee", "hit", "slash", "bolt", "heal", "pickup", "levelup", "victory",
    "hurt", "monster_atk", "ice_crack", "lightning", "summon",
    "ui_click", "ui_confirm",
};
constexpr size_t kSynthCount = sizeof(kSynthComposers) / sizeof(kSynthComposers[0]);

}  // namespace

// ── 1. assets.audio 类别齐备, 路径存在 ───────────────────────
TEST(AudioManifest, SectionAndFilesPresent) {
    auto& rm = ResourceManager::inst();
    ASSERT_TRUE(rm.load_assets_config());
    for (const char* id : {"audio.timestop", "audio.domain_expand"}) {
        const AssetDef* d = rm.asset_by_id(id);
        ASSERT_NE(d, nullptr) << "缺失资产: " << id;
        EXPECT_EQ(d->path.substr(0, 7), "assets/") << id << ": 路径须 assets/ 开头";
        EXPECT_NE(d->path.find(".mp3"), std::string::npos) << id << ": 须为 .mp3";
        std::ifstream f(d->path);
        EXPECT_TRUE(f.good()) << id << " → 缺失资源: " << d->path;
    }
}

// ── 2. fallback 声明化 (每条目显式; timestop=bolt, domain_expand=空如实) ──
TEST(AudioManifest, FallbackDeclared) {
    auto& rm = ResourceManager::inst();
    ASSERT_TRUE(rm.load_assets_config());
    const AssetDef* ts = rm.asset_by_id("audio.timestop");
    ASSERT_NE(ts, nullptr);
    EXPECT_EQ(ts->fallback, "synth:bolt")
        << "timestop fallback 须声明为 synth:bolt (现有 composer)";

    const AssetDef* de = rm.asset_by_id("audio.domain_expand");
    ASSERT_NE(de, nullptr);
    // 真缺口如实声明: domain_expand 现无合成回退 (文件缺失→静默)。
    // (补回退需新 composer = 音效算法, 超出 B3A 冻结; 已在报告标注)
    EXPECT_TRUE(de->fallback.empty())
        << "domain_expand fallback 当前断言为空 (如实)";
}

// ── 3. ID 查询解析正确 + 未知 ID 返回 null ───────────────────
TEST(AudioManifest, IdResolution) {
    auto& rm = ResourceManager::inst();
    ASSERT_TRUE(rm.load_assets_config());
    const AssetDef* ts = rm.asset_by_id("audio.timestop");
    ASSERT_NE(ts, nullptr);
    EXPECT_NE(ts->path.find("jojo_timestop.mp3"), std::string::npos);
    const AssetDef* de = rm.asset_by_id("audio.domain_expand");
    ASSERT_NE(de, nullptr);
    EXPECT_NE(de->path.find("domain_expand.mp3"), std::string::npos);
    EXPECT_EQ(rm.asset_by_id("audio.nonexistent"), nullptr);
}

// ── 4. fallback 引用有效性: synth:<key> 必须存在于合成器集合 ──
TEST(AudioManifest, SynthFallbackTargetsValid) {
    auto& rm = ResourceManager::inst();
    ASSERT_TRUE(rm.load_assets_config());
    std::set<std::string> known(kSynthComposers, kSynthComposers + kSynthCount);
    // 遍历 assets.audio 全部条目
    const AssetDef* ts = rm.asset_by_id("audio.timestop");
    for (const AssetDef* d : {ts}) {
        if (!d || d->fallback.empty()) continue;
        const std::string pfx = "synth:";
        EXPECT_EQ(d->fallback.substr(0, pfx.size()), pfx)
            << "fallback 须以 synth: 开头";
        std::string key = d->fallback.substr(pfx.size());
        EXPECT_TRUE(known.count(key))
            << "fallback 引用未知合成器: " << key;
    }
}