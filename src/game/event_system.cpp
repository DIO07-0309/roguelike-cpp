#include "event_system.h"
#include "player.h"
#include "item.h"
#include "combat_system.h"
#include "floor_config.h"
#include "build_score.h"
#include "core/logger.h"

// ============================================================
// D4 Step1: Event Text Pools (每种事件≥8条随机文本)
// ============================================================
static const char* TEXTS[][10] = {
    // MERCHANT (0)
    {"一个褴褛的商人从阴影中走出，背包叮当作响。",
     "商贩推着一辆破旧的推车，上面堆满了奇异的物品。",
     "\"欢迎！\" 一个苍老的声音从角落传来。",
     "你闻到了药水和旧皮革的味道——这里有商人。",
     "\"地牢里的生意很冷清，\" 商人朝你笑了笑。",
     "昏暗的火把下，一个摊位摆满了各式商品。",
     "商人从斗篷下掏出一件闪光的东西。",
     "\"陌生人，要不要看看我的珍藏？\""},
    // AMBUSH (1)
    {"脚步声从四面八方涌来——这是埋伏！",
     "天花板上的碎石突然落下，一群怪物从暗处冲出！",
     "你踩到了什么……地板突然塌陷，敌人从四周涌入。",
     "\"抓住他！\" 黑暗中传来嘶哑的呼喊。",
     "狭窄的走廊被两头怪物堵住了退路。",
     "墙上的火把突然熄灭——黑暗中亮起了无数双眼睛。",
     "你感到地面在震动，这不是普通的房间。",
     "陷阱！怪物从天花板上跳了下来！"},
    // CURSED_ROOM (2)
    {"空气中弥漫着古老的诅咒气息。",
     "墙壁上刻满了扭曲的符文，散发着暗紫色的光芒。",
     "一个嘶哑的声音在你脑中低语：\"拿吧……代价很公平。\"",
     "祭坛上放着一件辉煌的战利品，但你知道这不是免费的。",
     "你的手在颤抖——诅咒的力量在召唤。",
     "地板上画着一个法阵，其中的力量令人不安。",
     "\"所有力量都有代价，\" 墙上的文字闪烁。",
     "黑暗在你身边凝聚，等待着你的决定。"},
    // ALTAR_CHOICE (3)
    {"三束光芒从古老祭坛中升起。",
     "祭坛上摆放着三件贡品，你只能拿走一件。",
     "\"选择吧，\" 一个空灵的声音回荡。",
     "三个符号在空中闪烁——力量、生命、知识。",
     "祭坛的石刻显示着三个不同的祝福。",
     "你能感受到每件贡品中蕴含的力量——但只能选一个。",
     "\"命运女神只垂青果断之人，\" 铭文写道。",
     "三道光柱从地板上射出，每一道都在呼唤你。"},
    // STATUE (4)
    {"一座古老的雕像矗立在房间中央，它似乎在注视着你。",
     "雕像的双眼发出微弱的光芒。",
     "灰尘覆盖了雕像大部分表面，但基座上刻着什么。",
     "这座雕像雕刻得栩栩如生，仿佛下一刻就会动起来。",
     "\"触碰我，\" 一个来自远古的声音低语。",
     "雕像周围散落着古代的祭品——有人来过这里。",
     "石像的手中捧着一个发光的物体。",
     "你感到一股莫名的力量从雕像中涌出。"},
    // PRISONER (5)
    {"铁栏杆后蜷缩着一个身影。",
     "\"求你……救救我……\" 声音虚弱而绝望。",
     "囚笼里关着一个幸存者，看起来被困了很久。",
     "一个探险家的背包散落在囚笼旁边。",
     "\"我可以给你回报，\" 囚犯低声说道。",
     "铁链发出叮当声——囚犯在颤抖。",
     "黑暗中传来微弱的呼救声。",
     "你发现了一个被锁住的牢房，里面关着一个人。"},
    // LOST_CAMP (6)
    {"一个废弃的营地在角落散发着温暖的篝火余烬。",
     "篝火旁有一个破旧的背包和几张撕破的地图。",
     "营地虽小，但篝火带来了安全的感觉。",
     "你发现了一本日记，里面记录着上一个冒险者的最后旅程。",
     "帐篷里有一张铺好的床——这是休息的机会。",
     "\"如果还有后来者……\" 日记的第一页写道。",
     "营火虽然微弱，但足以驱散黑暗。",
     "一个小型的补给站，看来有人曾在这里避难。"},
    // TREASURE_GUARD (7)
    {"巨大的宝箱被一只沉睡的怪物守护着。",
     "\"谁敢碰我的财宝？！\" 一只巨兽从暗处出现。",
     "宝箱上镶满了宝石——但守护者不会轻易让路。",
     "你听到了沉重而缓慢的呼吸声——那个守卫不是普通的怪物。",
     "堆满金币的地板上站着一个庞大的身影。",
     "宝库的守卫者用冰冷的眼神注视着你。",
     "\"证明你的价值，\" 守卫发出低沉的怒吼。",
     "黄金的光芒从宝箱的缝隙中透出——但你必须先战胜它的守护者。"},
    // BLOOD_RITUAL (8)
    {"一个腥红色的祭坛在房间中央滴着血。",
     "墙壁上的血痕拼出了一个古老的契约图案。",
     "\"献血者将获得超凡的力量，\" 铭文刻在祭坛上。",
     "祭坛上的一把匕首似乎已经等待了千年。",
     "暗红色的雾气围绕着你——血祭的力量在召唤。",
     "\"你愿意付出多少？\" 一个低沉的声音问道。",
     "祭坛的血管遍布地板，仿佛在呼吸。",
     "这里曾经有过无数次献祭——空气中弥漫着铁锈的腥味。"},
    // NOTHING (9)
    {"这间房间空无一物，只有风吹过缝隙的声音。",
     "灰尘覆盖了地板——很久没有人来过这里了。",
     "墙壁上有一道长长的裂缝，透进一点微弱的光。",
     "除了寂静，这里什么都没有。",
     "一只老鼠窜过房间，消失在墙角的洞里。",
     "你听到了水滴的声音，但找不到来源。",
     "这里曾经可能有过什么，但现在只剩下遗迹。",
     "此地空无一物，但你感到一丝不安。"},
};

static const char* pick_text(int type, std::mt19937& rng) {
    if (type < 0 || type > 9) return "???";
    int idx = rng() % 8;
    return TEXTS[type][idx];
}

std::vector<std::string> event_text_pool(int idx) {
    std::vector<std::string> out;
    if (idx < 0 || idx > 9) return out;
    for (int i = 0; i < 8; i++) out.push_back(TEXTS[idx][i]);
    return out;
}

const char* event_type_name(EventType t) {
    switch (t) {
        case EventType::MERCHANT:       return "旅行商人";
        case EventType::AMBUSH:         return "伏击!";
        case EventType::CURSED_ROOM:    return "诅咒房间";
        case EventType::ALTAR_CHOICE:   return "三选祭坛";
        case EventType::STATUE:         return "神秘雕像";
        case EventType::PRISONER:       return "囚犯";
        case EventType::LOST_CAMP:      return "迷失营地";
        case EventType::TREASURE_GUARD: return "宝库守卫";
        case EventType::BLOOD_RITUAL:   return "血祭仪式";
        case EventType::NOTHING:        return "寂静房间";
        default:                        return "???";
    }
}

// ============================================================
// Event Generation — 按章节权重随机
// ============================================================
DungeonEvent generate_event(int floor, const ChapterConfig& ch, std::mt19937& rng) {
    DungeonEvent ev;
    int chap = ch.chapter;

    // 权重表: {MERCHANT,AMBUSH,CURSED,ALTAR,STATUE,PRISONER,CAMP,GUARD,BLOOD,NOTHING}
    int weights[10];
    if (chap == 0) {
        // 序章: 多商人/营地, 少诅咒
        int w[] = {30, 10, 5, 8, 8, 10, 15, 0, 0, 14};
        for (int i = 0; i < 10; i++) weights[i] = w[i];
    } else if (chap == 1) {
        // 深渊: 诅咒/雕像增多
        int w[] = {20, 15, 12, 8, 12, 8, 10, 5, 5, 5};
        for (int i = 0; i < 10; i++) weights[i] = w[i];
    } else {
        // 炼狱: 伏击/守护/血祭增多
        int w[] = {15, 18, 12, 8, 10, 8, 8, 8, 8, 5};
        for (int i = 0; i < 10; i++) weights[i] = w[i];
    }

    // 总权重
    int total = 0;
    for (int i = 0; i < 10; i++) total += weights[i];
    int roll = (int)(rng() % (uint32_t)total);
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += weights[i];
        if (roll < sum) {
            ev.type = (EventType)(i + 1); // skip NONE=0
            break;
        }
    }
    if (ev.type == EventType::NONE) ev.type = EventType::STATUE; // fallback

    ev.title  = event_type_name(ev.type);
    ev.text   = pick_text((int)ev.type - 1, rng);
    ev.desc   = ev.text;
    return ev;
}

// ============================================================
// Event Execution — 返回结果消息
// ============================================================
std::string execute_event(DungeonEvent& ev, Player* player, int floor) {
    if (ev.triggered || !player) return "";
    ev.triggered = true;

    switch (ev.type) {
    case EventType::MERCHANT: {
        // 随机给药水或 Relic 或 Buff
        int r = rng() % 3;
        if (r == 0) {
            auto pot = std::make_shared<ConsumableItem>("商人之药", Rarity::RARE, "heal", 40);
            player->inventory.add(pot, player);
            return "RELIC:商人给了你一瓶上等药水。";
        } else if (r == 1) {
            apply_buff(player, "attack_up", 1);
            return "MSG:商人给予了你攻击祝福。";
        } else {
            // 尝试给 relic
            auto ids = get_all_relic_ids();
            std::vector<std::string> avail;
            for (auto& id : ids)
                if (!player_has_relic(player, id)) avail.push_back(id);
            if (!avail.empty()) {
                std::string chosen = avail[rng() % avail.size()];
                player->relics.push_back({chosen});
                return "RELIC:" + std::string(get_relic_def(chosen)->name);
            }
            return "MSG:商人已无货可卖。";
        }
    }
    case EventType::AMBUSH: {
        // 经验 +50% buff (通过 apply buff 模拟)
        apply_buff(player, "attack_up", 1);
        return "MSG:你从伏击中杀出重围！经验加成+50%。";
    }
    case EventType::CURSED_ROOM: {
        // 诅咒: poison stack + 奖励 relic
        apply_buff(player, "poison", 2);
        auto ids = get_all_relic_ids();
        std::vector<std::string> avail;
        for (auto& id : ids)
            if (!player_has_relic(player, id)) avail.push_back(id);
        if (!avail.empty()) {
            std::string chosen = avail[rng() % avail.size()];
            player->relics.push_back({chosen});
            return "RELIC:" + std::string(get_relic_def(chosen)->name) + " (受诅咒:中毒)";
        }
        return "MSG:你拒绝了诅咒的诱惑。";
    }
    case EventType::ALTAR_CHOICE: {
        // 随机: heal / attack_up / 技能进化(若有可进化技能)
        int r = rng() % 3;
        if (r == 0) {
            heal_player(player, (int)(player->combat.max_hp * 0.30f));
            return "MSG:祭坛恢复了你30%的生命。";
        } else if (r == 1) {
            apply_buff(player, "attack_up", 2);
            return "MSG:祭坛赐予了你强大的攻击力。";
        } else {
            // 尝试进化一个技能
            for (auto& sk : player->skills.active_skills) {
                if (sk->can_evolve()) {
                    std::string evo_name = sk->evolve();
                    if (!evo_name.empty())
                        return "MSG:祭坛进化了你的技能: " + evo_name;
                }
            }
        }
        return "MSG:祭坛的光芒消散了。";
    }
    case EventType::STATUE: {
        int r = rng() % 5;
        if (r == 0) { apply_buff(player, "attack_up", 1); return "MSG:雕像赐予你攻击提升。"; }
        if (r == 1) { apply_buff(player, "slow", 1); return "MSG:雕像施放了减速诅咒。"; }
        if (r == 2) { heal_player(player, 15); return "MSG:雕像治愈了你。"; }
        // r==3 relic
        auto ids = get_all_relic_ids();
        std::vector<std::string> avail;
        for (auto& id : ids)
            if (!player_has_relic(player, id)) avail.push_back(id);
        if (!avail.empty() && r == 3) {
            std::string ch = avail[rng() % avail.size()];
            player->relics.push_back({ch});
            return "RELIC:" + std::string(get_relic_def(ch)->name);
        }
        return "MSG:雕像似乎什么都没有发生。";
    }
    case EventType::PRISONER: {
        // 50% 回血 / 50% 给药水
        if (rng() % 2 == 0) {
            heal_player(player, (int)(player->combat.max_hp * 0.20f));
            return "MSG:囚犯感激地拍了拍你，分享了他最后的食物。";
        } else {
            auto p = std::make_shared<ConsumableItem>("囚犯的谢礼", Rarity::RARE, "heal", 30);
            player->inventory.add(p, player);
            return "MSG:囚犯给了你一件物品作为答谢。";
        }
    }
    case EventType::LOST_CAMP: {
        heal_player(player, (int)(player->combat.max_hp * 0.25f));
        return "MSG:你在篝火旁休息，恢复了体力。";
    }
    case EventType::TREASURE_GUARD: {
        // 给 legend weapon + relic
        auto wp = std::make_shared<EquipmentItem>("宝库之剑", Rarity::LEGENDARY, "weapon", 15, 2, 1);
        player->inventory.add(wp, player);
        auto ids = get_all_relic_ids();
        std::vector<std::string> avail;
        for (auto& id : ids)
            if (!player_has_relic(player, id)) avail.push_back(id);
        if (!avail.empty()) {
            std::string ch = avail[rng() % avail.size()];
            player->relics.push_back({ch});
            return "RELIC:" + std::string(get_relic_def(ch)->name);
        }
        return "RELIC:宝库之剑";
    }
    case EventType::BLOOD_RITUAL: {
        int loss = (int)(player->combat.current_hp * 0.20f);
        if (loss < 1) loss = 1;
        player->combat.take_damage(loss);
        apply_buff(player, "attack_up", 3);
        return "MSG:鲜血浸染祭坛，你获得了强大的力量。";
    }
    case EventType::NOTHING:
        return "MSG:这里什么也没有发生。";
    default: break;
    }
    return "";
}
