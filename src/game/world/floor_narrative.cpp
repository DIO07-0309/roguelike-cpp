#include "floor_narrative.h"
#include "item.h"  // rng
#include <cstring>

// ============================================================
// D4 Step3: 15层完整叙事配置
// 格式: floor, title, subtitle, desc, enter, exit, boss_hint,
//        {narr0, narr1, narr2, narr3, narr4}, ambience, R,G,B
// ============================================================

static const FloorNarrative NARRATIVES[15] = {
    // ── 第一章: 沉睡牢狱 (F1-5) ──
    { 1, "沉睡牢狱", "The Sleeping Prison",
      "古老的地牢在黑暗中沉睡了千年，\n如今，它醒了。",
      "你睁开眼睛。头顶是冰冷的石壁，身下是发霉的稻草。\n你已经记不清自己在这里被关了多久。",
      "你推开铁门，身后传来沉闷的回声。",
      nullptr,  // F1无伏笔
      { "墙上的火把燃尽了最后一个世纪的记忆。",
        "水滴从天花板滴落——这是唯一的时钟。",
        "牢房的铁栏杆早已锈蚀，但门锁仍然紧紧扣着。",
        "潮湿的空气中弥漫着绝望的气味。",
        nullptr },
      "drip", 70, 85, 110 },

    { 2, "破碎回廊", "The Broken Corridor",
      "长廊的柱子上刻满了囚徒的名字，\n有些已经被岁月磨平。",
      "回廊一直向前延伸，看不到尽头。\n两侧的牢房里偶尔传来低语。",
      nullptr, nullptr,
      { "柱子上刻着模糊的文字——也许是一个人的遗言。",
        "地面上散落着碎裂的骨头，踩上去发出清脆的声响。",
        "墙壁的缝隙里透出微弱的绿光。",
        "你听到有人在呼唤——也许是风，也许不是。",
        nullptr },
      "wind", 60, 75, 100 },

    { 3, "囚徒食堂", "The Prisoners' Hall",
      "巨大的石桌上摆放着早已朽烂的木碗，\n这个房间曾经属于一群人。",
      nullptr, nullptr, nullptr,
      { "石桌的边缘有一个小小的刻痕——也许是谁在记着日子。",
        "角落里散落着铁链，上面沾着暗红色的印记。",
        "炊火的残骸冰冷了不知多少年。",
        "空气中似乎还残留着腐烂食物的气味。",
        "一只老鼠匆匆穿过房间，它的眼睛里反射着火光。"},
      "chains", 55, 70, 95 },

    { 4, "泪水之井", "The Well of Tears",
      "一口深井坐落在房间中央，\n空气中弥漫着锈铁的味道。",
      "井口边的石板上刻着古老的符文——\n'这里囚禁着比死亡更可怕的东西。'",
      nullptr,
      "狱卒在等你。\n他已经等你很久了。",
      { "井底传来微弱的哭声。",
        "石板上不仅有符文，还有深深的爪痕。",
        "一滴水从井边滑落——它向上飞起来了。",
        "你能听到井底有人在倒数。",
        nullptr },
      "water", 65, 80, 105 },

    { 5, "狱卒大厅", "The Warden's Throne",
      "这曾是监狱的心脏。\n黑暗在这里最为浓烈。",
      nullptr, // 用Boss介绍
      "你击败了守护者。\n黑暗的帷幕裂开了一道缝隙。",
      nullptr,
      { "你会记住这一刻的——即使黑暗试图让你忘记。",
        nullptr },
      "silence", 40, 30, 70 },

    // ── 第二章: 腐化神殿 (F6-10) ──
    { 6, "苔藓洞穴", "The Moss Caves",
      "你顺着裂缝下坠，落入了一片幽绿的洞窟。\n荧光苔藓是这里唯一的光源。",
      "这里比上面更深，也更安静。\n安静得让人不安。",
      nullptr,
      "有什么东西在你身后。\n不，是有什么东西在你身体里。",
      { "苔藓——它们似乎在随着你的呼吸闪烁。",
        "洞穴深处传来某种古老的吟唱。",
        "你发现地上有一串脚印——它们消失在了岩壁里。",
        nullptr },
      "whisper", 60, 100, 70 },

    { 7, "腐化神殿", "The Corrupted Temple",
      "一座古老的神殿矗立在黑暗之中。\n它的柱子雕刻着已被遗忘的神明。",
      "神殿的大门敞开着。\n它在邀请——它在等待。",
      nullptr,
      "神像的眼睛在流血。\n它已经看了你很久。",
      { "神坛上放着一本打开的经书，书页漆黑如墨。",
        "墙上的壁画描绘着一场远古的献祭。",
        "空气中充满了灰色烟雾，它们在跳动——像心跳。",
        "你在神殿的深处感到一阵眩晕。",
        nullptr },
      "chant", 80, 65, 115 },

    { 8, "哭泣深渊", "The Weeping Chasm",
      "深渊中悬挂着无数铁链，\n每根铁链下都坠着一个生锈的囚笼。",
      nullptr, nullptr,
      "深渊并不是空的。\n它一直在注视你。",
      { "铁链在风中摇摆，发出刺耳的摩擦声。",
        "远处传来一阵撕裂的声音——也许是铁门，也许不是。",
        "深渊里有什么在发光。它越来越近了。",
        nullptr },
      "chains", 70, 55, 100 },

    { 9, "朝圣者之路", "The Pilgrim's Path",
      "石壁上刻着数以千计的足迹，\n它们指向同一个方向——更深处。",
      "这条路太安静了。\n就连风也停住了。",
      nullptr,
      "朝圣者的尽头不是神明——\n是深渊。",
      { "墙壁上刻满了祈祷文，但都被划掉了。",
        "这里的地面是温热的，像是有生命。",
        "你在一段文字的边缘看到了自己的名字。",
        "前方的通道变窄了——它一直在等你。",
        nullptr },
      "footsteps", 65, 80, 100 },

    { 10, "熔岩王座", "The Lava Throne",
      "熔岩在裂缝中翻滚，\n红光把黑暗烧成了灰烬。",
      nullptr, // Boss
      "火焰熄灭了。\n但真正的黑暗才刚刚开始。",
      nullptr,
      { "这里的温度让你的眼睛开始刺痛。",
        "熔岩的表面不时浮出一张张扭曲的脸。",
        "空气中回荡着嘶哑的怒吼。",
        nullptr },
      "lava", 120, 30, 25 },

    // ── 第三章: 深渊裂谷 (F11-15) ──
    { 11, "灰烬废墟", "The Ash Ruins",
      "这里曾是朝圣者的终站——\n如今只剩下焦黑的骨架。",
      "空气中弥漫着硫磺的味道。\n你不敢呼吸太重。",
      nullptr,
      "你走在别人的尸骨上。\n也走在别人的脚下。",
      { "重建这座城市大概需要一千年。\n建造它只用了七天。",
        "灰烬中有一面镜子——它映出了一个你不认识的人。",
        "远处传来爆炸的回声——也许是火山，也许不是。",
        nullptr },
      "rumble", 100, 40, 30 },

    { 12, "焦土裂谷", "The Scorched Rift",
      "大地上裂开了一道巨大的伤口，\n裂缝中流出暗红色的光。",
      nullptr, nullptr,
      "深渊在呼唤你。\n你也在呼唤深渊。",
      { "裂缝旁的石头上刻着整齐的符号——不像人类的手笔。",
        "热度让空气扭曲成了奇怪的形状。",
        "裂缝深处传来某种语言——不属于这个世界。",
        "地面开始在你脚下移动。",
        nullptr },
      "rumble", 110, 35, 25 },

    { 13, "愚者朝圣", "The Pilgrimage of Fools",
      "千百具骸骨面朝深处匍匐在地，\n他们用死亡完成了朝圣。",
      "尸骨之间散落着黄金和圣物。\n他们直到最后也没有放弃信仰。",
      nullptr,
      "最后一个朝圣者就是你。",
      { "这些尸骨的温度仍然温存。",
        "虔诚……某种意义上，也是另一种形式的疯狂。",
        "有一个小骷髅手里还握着一枚金币。",
        "你还记得自己为什么要来这里吗？",
        nullptr },
      "silence", 90, 35, 30 },

    { 14, "起源之门", "The Gate of Origin",
      "一座宏伟的石门伫立在你面前——\n门上刻着一个被遗忘的创世神话。",
      "浮雕上描绘着光与暗的第一次分离。\n在光的那一边，是你——在暗的这一边，是它。",
      nullptr,
      "推开这扇门后——\n你将不再是原来的你。",
      { "石门上刻着的文字在闪烁——它们在移动。",
        "门的中央有一道裂缝，从里面泻出黑光。",
        "你能感觉到门后有东西在注视你。",
        "门上刻着一双眼睛——它眨了。",
        nullptr },
      "silence", 80, 30, 35 },

    { 15, "深渊王座", "The Throne of the Abyss",
      "这是地牢的终点。\n黑暗在这里凝聚成了实体。",
      nullptr, // Boss intro
      nullptr, // Victory handled by GameScene
      nullptr,
      { "当这个世界醒来的时候——它还会记得你吗？",
        nullptr },
      "silence", 60, 15, 20 },
};

// ---- 查询 ----
const FloorNarrative* get_floor_narrative(int floor) {
    if (floor < 1 || floor > 15) return &NARRATIVES[0];
    return &NARRATIVES[floor - 1];
}

const char* get_chapter_title(int ch) {
    switch (ch) {
        case 0: return "第一章";
        case 1: return "第二章";
        case 2: return "第三章";
        default: return "???";
    }
}

const char* get_chapter_subtitle(int ch) {
    switch (ch) {
        case 0: return "Chapter I";
        case 1: return "Chapter II";
        case 2: return "Chapter III";
        default: return "???";
    }
}

const char* pick_random_narration(int floor, NarrativeState& state) {
    const FloorNarrative* fn = get_floor_narrative(floor);
    if (!fn) return nullptr;

    // 收集所有非nullptr叙事
    const char* pool[5]; int count = 0;
    for (int i = 0; i < 5; i++) {
        if (fn->narrations[i]) pool[count++] = fn->narrations[i];
    }
    if (count == 0) return nullptr;

    // 随机选择, 避免与上次重复
    int idx = (int)(rng() % (uint32_t)count);
    if (count >= 2 && idx == state.last_narration_idx)
        idx = (idx + 1) % count;
    state.last_narration_idx = idx;
    return pool[idx];
}
