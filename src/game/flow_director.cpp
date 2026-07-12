#include "flow_director.h"
#include "item.h"  // rng

// ============================================================
// D4.6 Step3: FlowDirector 实现
// ============================================================

void FlowDirector::tick(float dt) {
    _timer.combat += dt;
    _timer.reward += dt;
    _timer.story  += dt;
    _timer.event  += dt;
}

float FlowDirector::worst_gap() const {
    float m = _timer.combat;
    if (_timer.reward > m) m = _timer.reward;
    if (_timer.story  > m) m = _timer.story;
    if (_timer.event  > m) m = _timer.event;
    return m;
}

const char* FlowDirector::auto_spawn_suggestion() {
    // 优先级: 战斗 > 奖励 > 事件 > 剧情
    if (_timer.combat >= COMBAT_MAX && (rng() % 100) < 40) {
        _timer.combat = COMBAT_MAX * 0.5f;  // 半重置, 不归零
        // 80% 普通巡邏, 20% 精英巡逻
        if ((rng() % 100) < 20)
            return "AUTO_ELITE";    // 精英巡逻队
        else
            return "AUTO_PATROL";   // 普通巡邏怪
    }
    if (_timer.reward >= REWARD_MAX && (rng() % 100) < 15) {
        _timer.reward = REWARD_MAX * 0.5f;
        return "AUTO_REWARD";  // 动态宝箱
    }
    if (_timer.story >= STORY_MAX && (rng() % 100) < 10) {
        _timer.story = STORY_MAX * 0.5f;
        return "AUTO_STORY";  // 世界旁白
    }
    return nullptr;
}
