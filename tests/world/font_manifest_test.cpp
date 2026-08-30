// G10.2-B3B: Font Manifest — 候选 A (Fusion Pixel 12px zh_hans) 接入回归
// 验收 (B3B 指令):
//   font.game_regular → Manifest → Font Loader → Game UI → 实际文本字符集验证 → 无缺字/无系统依赖
// 核心: headless 解析 assets/fonts/game_regular.ttf 的 cmap 表,
//       验证 font_codepoints.h (游戏实际文本, 1835 码位) 全部覆盖。
#include <gtest/gtest.h>

#include "resources/resource_manager.h"
#include "core/font_codepoints.h"
#include <nlohmann/json.hpp>
#include "raylib.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// 与既有测试一致: main.cpp 的字体全局桩化
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

namespace {

// ── 最小 TTF cmap 解析 (headless, 只读覆盖检查) ──────────────
class TtfCmap {
public:
    bool load(const char* path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        data.assign(std::istreambuf_iterator<char>(in), {});
        if (data.size() < 12) return false;
        const uint16_t numTables = be16(4);
        const uint32_t cmapOff = find_table(tag('c', 'm', 'a', 'p'));
        if (cmapOff == 0) return false;
        const uint16_t numSub = be16(cmapOff + 2);
        for (uint16_t i = 0; i < numSub; i++) {
            const uint32_t rec = cmapOff + 4 + i * 8;
            const uint16_t plat = be16(rec), enc = be16(rec + 2);
            const uint32_t subOff = cmapOff + be32(rec + 4);
            if (subOff + 2 > size()) continue;
            const uint16_t fmt = be16(subOff);
            const bool bmp = (plat == 3 && enc == 1) || (plat == 0 && (enc == 3 || enc == 1));
            if (bmp && fmt == 4) _fmt4 = subOff;
            if (bmp && fmt == 12) _fmt12 = subOff;
        }
        return _fmt4 != 0 || _fmt12 != 0;
    }

    bool has(uint32_t cp) const {
        if (cp <= 0xFFFF && _fmt4 != 0) {
            const uint32_t s = _fmt4;
            const uint16_t segX2 = be16(s + 6);
            const uint16_t seg = (uint16_t)(segX2 / 2);
            const uint32_t endBase = s + 14;
            const uint32_t startBase = endBase + segX2 + 2;
            const uint32_t deltaBase = startBase + segX2;
            const uint32_t rangeBase = deltaBase + segX2;
            int lo = 0, hi = seg - 1;
            while (lo <= hi) {
                const int mid = (lo + hi) / 2;
                const uint16_t end = be16(endBase + mid * 2);
                if (cp > end) { lo = mid + 1; continue; }
                const uint16_t start = be16(startBase + mid * 2);
                if (cp < start) { hi = mid - 1; continue; }
                const uint16_t idDelta = be16(deltaBase + mid * 2);
                const uint16_t idRange = be16(rangeBase + mid * 2);
                if (idRange == 0) return (uint16_t)(idDelta + cp) != 0;
                const uint32_t gaddr = rangeBase + mid * 2 + idRange + (cp - start) * 2;
                if (gaddr + 2 > size()) return false;
                return be16(gaddr) != 0;
            }
            return false;
        }
        if (_fmt12 != 0) {
            const uint32_t s = _fmt12;
            const int nGroups = (int)be32(s + 12);
            int lo = 0, hi = nGroups - 1;
            while (lo <= hi) {
                const int mid = (lo + hi) / 2;
                const uint32_t g = s + 16 + mid * 12;
                const uint32_t start = be32(g), end = be32(g + 4);
                if (cp < start) { hi = mid - 1; continue; }
                if (cp > end) { lo = mid + 1; continue; }
                return be32(g + 8) != 0;
            }
            return false;
        }
        return false;
    }

private:
    uint32_t size() const { return (uint32_t)data.size(); }
    static uint32_t tag(char a, char b, char c, char d) {
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
    }
    uint16_t be16(uint32_t off) const {
        if (off + 2 > size()) return 0;
        const unsigned char* p = (const unsigned char*)data.data() + off;
        return (uint16_t)((p[0] << 8) | p[1]);
    }
    uint32_t be32(uint32_t off) const {
        if (off + 4 > size()) return 0;
        const unsigned char* p = (const unsigned char*)data.data() + off;
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }
    uint32_t find_table(uint32_t t) const {
        const uint16_t n = be16(4);
        for (uint16_t i = 0; i < n; i++) {
            const uint32_t rec = 12 + i * 16;
            if (be32(rec) == t) return be32(rec + 8);
        }
        return 0;
    }
    std::vector<char> data;
    uint32_t _fmt4 = 0, _fmt12 = 0;
};

}  // namespace

// ── 1. assets.font 类别 + font.game_regular 定义存在 ─────────
TEST(FontManifest, SectionAndFilePresent) {
    auto& rm = ResourceManager::inst();
    ASSERT_TRUE(rm.load_assets_config());
    const AssetDef* fr = rm.asset_by_id("font.game_regular");
    ASSERT_NE(fr, nullptr) << "font.game_regular 未装载";
    EXPECT_EQ(fr->path.substr(0, 7), "assets/") << "路径须 assets/ 开头";
    std::ifstream f(fr->path);
    EXPECT_TRUE(f.good()) << "字体文件缺失: " << fr->path;
}

// ── 2. fallback 声明: 主字体缺 → 系统链兜底 ─────────────────
TEST(FontManifest, FallbackDeclared) {
    auto& rm = ResourceManager::inst();
    ASSERT_TRUE(rm.load_assets_config());
    const AssetDef* fr = rm.asset_by_id("font.game_regular");
    ASSERT_NE(fr, nullptr);
    EXPECT_NE(fr->fallback.find("system:"), std::string::npos)
        << "font fallback 须声明 system: 链";
}

// ── 3. POINT 渲染规则 (像素字体硬要求, B3B 裁决) ────────────
TEST(FontManifest, PointRenderRule) {
    auto& rm = ResourceManager::inst();
    ASSERT_TRUE(rm.load_assets_config());
    ASSERT_NE(rm.asset_by_id("font.game_regular"), nullptr);
    std::ifstream in("resources/sprites.json");
    ASSERT_TRUE(in);
    std::string text((std::istreambuf_iterator<char>(in)), {});
    auto j = nlohmann::json::parse(text);
    const auto& fr_ = j["assets"]["font"]["game_regular"];
    ASSERT_TRUE(fr_.contains("render")) << "font.game_regular 须声明 render 规则";
    EXPECT_EQ(fr_["render"].value("filter", ""), "point")
        << "像素字体必须 POINT filter";
}

// ── 4. 实际文本字符集覆盖验证 (核心验收) ────────────────────
// font_codepoints.h 游戏实际文本 (1835 码位) vs TTF cmap 全覆盖
TEST(FontManifest, GameTextCoverage) {
    TtfCmap cmap;
    ASSERT_TRUE(cmap.load("assets/fonts/game_regular.ttf"))
        << "TTF 解析失败 (cmap 缺失)";
    EXPECT_TRUE(cmap.has(0x4E2D)) << "自检失败: '中' (U+4E2D)";
    EXPECT_TRUE(cmap.has(0x6E38)) << "自检失败: '游' (U+6E38)";
    EXPECT_TRUE(cmap.has(0x4F60)) << "自检失败: '你' (U+4F60)";

    std::vector<uint32_t> missing;
    for (int i = 0; i < FONT_CP_COUNT; i++) {
        const uint32_t cp = (uint32_t)FONT_CP_DATA[i];
        if (!cmap.has(cp)) missing.push_back(cp);
    }
    if (!missing.empty()) {
        std::string list;
        for (uint32_t cp : missing)
            list += ("U+" + ((std::ostringstream{} << std::hex << cp).str())) + " ";
        ADD_FAILURE() << "缺字 " << missing.size() << "/" << FONT_CP_COUNT << ": " << list;
    }
}

// ── 5. 无系统字体依赖: 主字体单独即全覆盖 ───────────────────
TEST(FontManifest, NoSystemDependency) {
    TtfCmap cmap;
    ASSERT_TRUE(cmap.load("assets/fonts/game_regular.ttf"));
    for (int i = 0; i < FONT_CP_COUNT; i++) {
        if (!cmap.has((uint32_t)FONT_CP_DATA[i])) {
            ADD_FAILURE() << "非覆盖码位 U+" << std::hex << (uint32_t)FONT_CP_DATA[i]
                          << " → 尚需系统回退 (违反验收)";
            return;
        }
    }
    SUCCEED();
}