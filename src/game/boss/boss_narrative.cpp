#include "boss_narrative.h"
#include "relationship_system.h"
#include "quest_manager.h"

// ============================================================
// D4 Step5.5: Boss 对白数据 (22条)
// ============================================================
static std::vector<BossDialogue> _build_boss_dialogues() {
    std::vector<BossDialogue> out;

    // ── 暗影骑士 (F5) ──
    out.push_back({StoryStage::CHAPTER_1, 5, WorldFlag::NONE, BuildType::NONE, 0, 0,
        nullptr, // 默认intro用原有BossInfo
        "你变强了……但你也会像我一样。", nullptr, false});
    out.push_back({StoryStage::CHAPTER_1, 5, WorldFlag::Blood_Ritual, BuildType::NONE, 0, 0,
        "我闻到了鲜血——看来深渊已经接受了你。",
        "你的血……和我的血……已经分不清了。",
        "用鲜血换来的胜利——真的是胜利吗？", true});
    out.push_back({StoryStage::CHAPTER_1, 5, WorldFlag::Accepted_Curse, BuildType::NONE, 0, 0,
        "诅咒？很好。你已经属于这里了。",
        "黑暗不会只满足于一个灵魂。",
        nullptr, true});
    out.push_back({StoryStage::CHAPTER_1, 5, WorldFlag::Saved_Prisoner, BuildType::NONE, 0, 0,
        "你救了那个囚犯？他已经被关押了三十年——而你只用了三秒。",
        nullptr, nullptr, true});
    out.push_back({StoryStage::CHAPTER_1, 5, WorldFlag::NONE, BuildType::BERSERKER, 0, 0,
        "很好。来。面对面。",
        "你终于学会了用剑——但你永远学不会放手。",
        nullptr, false});
    out.push_back({StoryStage::CHAPTER_1, 5, WorldFlag::NONE, BuildType::POISON_MASTER, 0, 0,
        "毒？真是令人怀念。我曾被毒死过一次。",
        nullptr, nullptr, false});

    // ── 地狱火魔 (F10) ──
    out.push_back({StoryStage::CHAPTER_2, 10, WorldFlag::NONE, BuildType::NONE, 0, 0,
        nullptr,
        "火焰吞噬了你的武器。接下来——吞噬你。",
        "火焰终于熄灭了……但这个世界的火焰永远不会真正熄灭。", false});
    out.push_back({StoryStage::CHAPTER_2, 10, WorldFlag::Saved_Priest, BuildType::NONE, 0, 0,
        "那个祭司……居然还活着。他给了你力量——但你承受得起吗？",
        "神官的祈祷在这里是无效的——这里是烈焰的神殿。", nullptr, true});
    out.push_back({StoryStage::CHAPTER_2, 10, WorldFlag::Blood_Ritual, BuildType::NONE, 0, 0,
        "你献祭过自己的血？有趣。让我看看它有多烫。",
        nullptr, nullptr, true});
    out.push_back({StoryStage::CHAPTER_2, 10, WorldFlag::NONE, BuildType::FIRE_MAGE, 0, 0,
        "火焰？你根本不配使用它。",
        "你的火焰——太冷了。",
        "也许……你比我更适合驾驭火焰。", false});
    out.push_back({StoryStage::CHAPTER_2, 10, WorldFlag::NONE, BuildType::TIME_MASTER, 0, 0,
        "时间？你以为你能改变什么？",
        "这里的火已经烧了一千年——时间救不了你。", nullptr, false});

    // ── 深渊之主 (F15) ──
    out.push_back({StoryStage::FINAL, 15, WorldFlag::NONE, BuildType::NONE, 0, 0,
        "你终于来了。我等了你很久——我已经很久没有新的碎片了。",
        "你感受到了吗？这是你体内的黑暗——它渴望被释放。",
        "阳光照进来了……这是我死前最后一眼看到的东西吗？", false});
    out.push_back({StoryStage::FINAL, 15, WorldFlag::Saved_Priest, BuildType::NONE, 3, 70,
        "泰伦斯……那个傻瓜还在祈祷？三千年前我也听过他的祈祷——毫无用处。",
        nullptr, nullptr, true});
    out.push_back({StoryStage::FINAL, 15, WorldFlag::Saved_Prisoner, BuildType::NONE, 0, 0,
        "你救了那个囚犯？你知道他为什么会被关进来吗？因为他杀了一个国王。",
        nullptr, "连你救了的人都会成为我——这就是结局。", true});
    out.push_back({StoryStage::FINAL, 15, WorldFlag::Boss1_Defeated, BuildType::NONE, 0, 0,
        "你杀了狱卒。你杀了火魔。你很强——但你不是第一个。",
        nullptr, nullptr, false});
    out.push_back({StoryStage::FINAL, 15, WorldFlag::Boss2_Defeated, BuildType::NONE, 0, 0,
        "两个了。只剩你了，还有我。",
        "你的每一次胜利——都在给我力量。", nullptr, false});
    out.push_back({StoryStage::FINAL, 15, WorldFlag::Blood_Ritual, BuildType::NONE, 0, 0,
        "血祭……你已经成了深渊的一部分。来吧——回来。",
        "你的血液里流淌着我。你不可能打败你自己。",
        "黑暗说了一声谢谢——然后化为了虚无。", true});
    out.push_back({StoryStage::FINAL, 15, WorldFlag::Merchant_Killed, BuildType::NONE, 0, 0,
        "连商人都不放过。你比怪物更像怪物。",
        nullptr, nullptr, true});
    out.push_back({StoryStage::FINAL, 15, WorldFlag::NONE, BuildType::POISON_MASTER, 0, 0,
        "毒？你以为毒能伤到我？我就来自比毒更古老的东西。",
        "你一直在使用毒——但你有没有想过——你被感染了吗？", nullptr, false});
    out.push_back({StoryStage::FINAL, 15, WorldFlag::NONE, BuildType::TIME_MASTER, 0, 0,
        "时间？连神都无法控制时间——你能吗？",
        "你以为时间是你的武器——但它只是我的一根手指。", nullptr, false});
    out.push_back({StoryStage::FINAL, 15, WorldFlag::NONE, BuildType::BERSERKER, 0, 0,
        "狂战士？用愤怒来填补恐惧。这是最古老的战术。",
        nullptr, nullptr, false});
    out.push_back({StoryStage::FINAL, 15, WorldFlag::All_Boss_Defeated, BuildType::NONE, 0, 0,
        "狱卒。火魔。深渊之主。三个——你全都杀了。\n现在唯一的狱卒——是你。",
        nullptr, "你以为你赢了——但每一次胜利都让我的碎片更多。", false});

    return out;
}

BossNarrative::BossNarrative() : _dialogues(_build_boss_dialogues()) {}

// ---- 查询 ----
const BossDialogue* BossNarrative::find_intro(int boss_floor, const WorldState& ws,
                                                BuildType bt,
                                                const RelationshipSystem& rels,
                                                const StoryDirector& story) const {
    for (auto& d : _dialogues) {
        if (d.boss_floor != boss_floor) continue;
        if (d.stage != StoryStage::INTRO && d.stage != story.stage()) continue;
        if (d.required_flag != WorldFlag::NONE && !ws.has(d.required_flag)) continue;
        if (d.build != BuildType::NONE && d.build != bt) continue;
        if (d.relation_threshold > 0 && d.npc_id > 0) {
            if (!rels.check_relation(d.npc_id, RelationType::TRUST, d.relation_threshold))
                continue;
        }
        if (d.intro) return &d;
    }
    return nullptr;
}

const BossDialogue* BossNarrative::find_phase2(int boss_floor, const WorldState& ws,
                                                 BuildType bt) const {
    for (auto& d : _dialogues) {
        if (d.boss_floor != boss_floor) continue;
        if (d.required_flag != WorldFlag::NONE && !ws.has(d.required_flag)) continue;
        if (d.build != BuildType::NONE && d.build != bt) continue;
        if (d.phase2) return &d;
    }
    return nullptr;
}

const BossDialogue* BossNarrative::find_death(int boss_floor, const WorldState& ws,
                                                BuildType bt) const {
    for (auto& d : _dialogues) {
        if (d.boss_floor != boss_floor) continue;
        if (d.required_flag != WorldFlag::NONE && !ws.has(d.required_flag)) continue;
        if (d.build != BuildType::NONE && d.build != bt) continue;
        if (d.death) return &d;
    }
    return nullptr;
}

// ============================================================
// D5 Step1: calc_modifier() — 正式根据WorldState/Quest/Build/Relation修改Boss
// ============================================================
BossModifierHook BossNarrative::calc_modifier(int boss_floor, const WorldState& ws,
                                                BuildType bt,
                                                const RelationshipSystem& rels) {
    BossModifierHook m;

    // ── Boss1 (F5): 暗影骑士 ──
    if (boss_floor == 5) {
        // Saved_Prisoner → HP-20%
        if (ws.has(WorldFlag::Saved_Prisoner)) {
            m.hp_scale = 0.80f;
            m.modifier_text = "HP -20% (囚犯的祝福)";
        }
        // Blood_Ritual → HP+25%, 伤害+15%
        if (ws.has(WorldFlag::Blood_Ritual)) {
            m.hp_scale = 1.25f; m.damage_scale = 1.15f;
            m.modifier_text = "HP+25%% ATK+15%% (血祭)";
        }
        // BuildType: BERSERKER → Boss 格挡意识提升 (disable_skill逻辑)
        if (bt == BuildType::BERSERKER) {
            // 不影响 modifier (仅透传到AI), 标记为"对Boss近战格挡+"
        }
        // POISON → Boss中毒减半
        if (bt == BuildType::POISON_MASTER) {
            m.hp_scale += 0.10f;  // slight HP boost
            if (!m.modifier_text) m.modifier_text = "毒抗+ (毒术大师)";
        }
    }

    // ── Boss2 (F10): 地狱火魔 ──
    else if (boss_floor == 10) {
        // Saved_Priest → 禁用一次AOE技能
        if (ws.has(WorldFlag::Saved_Priest)) {
            m.disable_skill = true;
            m.modifier_text = "AOE禁用 (神官祝福)";
        }
        // Blood_Ritual → HP+20%
        if (ws.has(WorldFlag::Blood_Ritual)) {
            m.hp_scale = 1.20f;
            m.modifier_text = m.modifier_text ? " " : "";
            m.modifier_text = "HP+20%% (血祭)";
        }
        // Accepted_Curse → Boss获得新Debuff技能 (e.g. 上curse)
        if (ws.has(WorldFlag::Accepted_Curse)) {
            m.damage_scale += 0.10f;
        }
        // BuildType: FIRE_MAGE → Boss火抗+ (HP+)
        if (bt == BuildType::FIRE_MAGE) {
            m.hp_scale += 0.15f;
        }
        // Priest trust≥3 → 禁用一次技能
        if (rels.check_relation(70, RelationType::TRUST, 3)) {
            m.disable_skill = true;
        }
    }

    // ── Boss3 (F15): 深渊之主 ──
    else if (boss_floor == 15) {
        // Blood_Ritual → HP+30%, 伤害+20%
        if (ws.has(WorldFlag::Blood_Ritual)) {
            m.hp_scale = 1.30f; m.damage_scale = 1.20f;
            m.modifier_text = "HP+30%% ATK+20%% (血祭融合)";
        }
        // Saved_Priest → Phase2延迟 (降低HP间接影响)
        if (ws.has(WorldFlag::Saved_Priest)) {
            m.hp_scale = 0.85f;
            m.modifier_text = !m.modifier_text ? "HP-15%% (神官祈祷)" : m.modifier_text;
        }
        // Watcher respect≥3 → Phase2时间减少
        if (rels.check_relation(140, RelationType::RESPECT, 3)) {
            m.damage_scale -= 0.10f;
        }
        // Merchant_Killed → 额外精英
        if (ws.has(WorldFlag::Merchant_Killed)) {
            m.add_minions = true;
            if (!m.modifier_text) m.modifier_text = "额外精英 (商人咒骂)";
        }
        // Build: POISON → Boss腐化免疫++
        if (bt == BuildType::POISON_MASTER) {
            m.hp_scale += 0.15f;
        }
        // Build: TIME → Boss Phase切换加速
        if (bt == BuildType::TIME_MASTER) {
            m.damage_scale += 0.10f;
        }
        // Priest trust≥5 → Boss禁用一次技能
        if (rels.check_relation(70, RelationType::TRUST, 5)) {
            m.disable_skill = true;
        }
        // Fear高 (任意NPC fear≥5)
        for (auto& r : rels.all()) {
            if (r.fear >= 5) { m.damage_scale += 0.08f; break; }
        }
    }

    // Clamp for sanity
    if (m.hp_scale < 0.50f) m.hp_scale = 0.50f;
    if (m.hp_scale > 1.80f) m.hp_scale = 1.80f;
    if (m.damage_scale < 0.70f) m.damage_scale = 0.70f;
    if (m.damage_scale > 1.50f) m.damage_scale = 1.50f;

    return m;
}
