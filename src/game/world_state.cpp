#include "world_state.h"
#include "item.h"  // rng

// ============================================================
// WorldState 实现
// ============================================================
bool WorldState::has(WorldFlag f) const {
    return _flags.count(f) > 0;
}
void WorldState::set(WorldFlag f) {
    _flags.insert(f);
}
void WorldState::clear(WorldFlag f) {
    _flags.erase(f);
}
int WorldState::counter(const std::string& name) const {
    auto it = _counters.find(name);
    return it != _counters.end() ? it->second : 0;
}
void WorldState::add_counter(const std::string& name, int v) {
    _counters[name] = _counters.count(name) ? _counters[name] + v : v;
}

// ============================================================
// StoryDirector 状态机
// ============================================================
void StoryDirector::enter_floor(int floor) {
    _trigger = false;
    if      (floor <= 5)  _stage = StoryStage::CHAPTER_1;
    else if (floor <= 10) _stage = StoryStage::CHAPTER_2;
    else if (floor < 15)  _stage = StoryStage::CHAPTER_3;
    else if (floor == 15) _stage = StoryStage::FINAL;
    // Boss战后判定 (由 boss_dead 单独触发)
}

void StoryDirector::boss_dead(int floor) {
    _bosses_dead++;
    if (_bosses_dead >= 3) _stage = StoryStage::ENDING;
    (void)floor;
}

void StoryDirector::update(float dt) {
    _timer -= dt;
    if (_timer <= 0) { _timer = 30.0f; _trigger = true; }
    else _trigger = false;
}

// ============================================================
// 世界事件 10条文本池
// ============================================================
static const char* WORLD_EVENTS[] = {
    "远处传来一声沉闷的咆哮——它在变强。",
    "墙壁上的裂缝又多了一条。",
    "你听到远处有脚步声——但很快又消失了。",
    "头顶的石板在微微颤动——也许只是你的错觉。",
    "黑暗中有什么东西在呼吸。",
    "水滴声停了一秒——然后又开始了。",
    "你的影子——它多了一个。",
    "远处传来铁链被拖动的声音。",
    "角落里的碎石自己滚动了半寸。",
    "一只老鼠匆匆穿过走廊，它的眼睛里映着火光。",
};

const char* StoryDirector::tick_world_event(const WorldState& ws) {
    (void)ws;
    if ((rng() % 100) < 20) return nullptr;  // 20%不触发
    return WORLD_EVENTS[(int)(rng() % 10)];
}

// ============================================================
// NPC 自适应对话 (根据 WorldFlag)
// ============================================================
const char* StoryDirector::npc_adaptive_text(int npc_floor, const WorldState& ws) {
    if (npc_floor == 7 && ws.has(WorldFlag::Saved_Priest))
        return "泰伦斯注视着你: \"或许……你和其他人不一样。\"";
    if (npc_floor == 14 && ws.has(WorldFlag::Saved_Prisoner))
        return "守望者微笑了: \"你救了他。三千年来——你是第一个。\"";
    if (npc_floor == 14 && ws.has(WorldFlag::Blood_Ritual))
        return "守望者沉默了: \"你身上有深渊的气味。\"";
    if (npc_floor == 11 && ws.has(WorldFlag::Accepted_Curse))
        return "灵魂颤抖着: \"诅咒……它也在我身上。现在在你身上了。\"";
    if (npc_floor == 9 && ws.has(WorldFlag::Boss1_Defeated)
        && ws.has(WorldFlag::Boss2_Defeated))
        return "朝圣者跪下了: \"两个……只剩一个了。\"";
    if (npc_floor == 9 && ws.has(WorldFlag::Boss1_Defeated))
        return "朝圣者抬头看着你: \"你杀了狱卒。下一个是谁？\"";
    return nullptr;
}

// ============================================================
// 环境旁白 (根据 WorldFlag, 60s周期)
// ============================================================
const char* StoryDirector::world_flag_narration(const WorldState& ws) {
    if (ws.has(WorldFlag::Blood_Ritual))
        return "你的手还在微微颤抖——祭坛的记忆挥之不去。";
    if (ws.has(WorldFlag::Accepted_Curse))
        return "诅咒的力量在血管中流淌——你能感觉到。";
    if (ws.has(WorldFlag::Saved_Prisoner))
        return "囚犯的祝福似乎给了你更多的勇气。";
    if (ws.has(WorldFlag::Merchant_Killed))
        return "商人的货物散落在地上——再也没有人会来买了。";
    if (ws.has(WorldFlag::All_Boss_Defeated))
        return "黑暗开始褪去——这是从未有过的事情。";
    return nullptr;
}
