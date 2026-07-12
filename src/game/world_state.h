#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

// ============================================================
// D4 Step5.1: WorldFlag — 世界状态标记 (唯一权威来源)
// ============================================================
enum class WorldFlag {
    NONE = 0,
    // ── NPC 互动 ──
    Saved_Prisoner,     // F2 救了囚犯
    Saved_Priest,       // F7 见了神官 (关系≥3)
    Saved_Merchant,     // 保护了商人
    // ── 事件选择 ──
    Accepted_Curse,     // 诅咒房选了诅咒
    Blood_Ritual,       // 血祭已执行
    Ignored_NPC,        // 跳过了某个NPC
    Merchant_Killed,    // 杀了商人
    // ── Boss 进度 ──
    Boss1_Defeated,     // F5  暗影骑士
    Boss2_Defeated,     // F10 地狱火魔
    Boss3_Defeated,     // F15 深渊之主
    All_Boss_Defeated,  // 三Boss全击杀
    // ── 结局条件 ──
    True_Ending_Ready,  // 好感+不诅咒+不血祭+不杀商人+全Boss
};

namespace std {
    template<> struct hash<WorldFlag> {
        size_t operator()(WorldFlag f) const noexcept { return (size_t)f; }
    };
}

// ============================================================
// WorldState — 跨楼层持久, 唯一的世界状态来源
// ============================================================
class WorldState {
public:
    bool has(WorldFlag f) const;
    void set(WorldFlag f);
    void clear(WorldFlag f);

    // 计数器查询 (所有数值追踪走这里)
    int  counter(const std::string& name) const;
    void add_counter(const std::string& name, int v = 1);

private:
    std::unordered_set<WorldFlag> _flags;
    std::unordered_map<std::string, int> _counters;
};

// ============================================================
// StoryStage — 剧情阶段
// ============================================================
enum class StoryStage { INTRO, CHAPTER_1, CHAPTER_2, CHAPTER_3, FINAL, ENDING };

// ============================================================
// StoryDirector — 唯一的剧情推进入口
// ============================================================
class StoryDirector {
public:
    StoryStage stage() const { return _stage; }
    void enter_floor(int floor);
    void boss_dead(int floor);
    void update(float dt);  // ~30s tick timer
    bool should_trigger_story() const { return _trigger; }

    // 世界事件文字 (每30s自动触发)
    const char* tick_world_event(const WorldState& ws);
    // NPC 自适应对话消息
    static const char* npc_adaptive_text(int npc_floor, const WorldState& ws);
    // 环境旁白 (根据WorldFlag)
    static const char* world_flag_narration(const WorldState& ws);

private:
    StoryStage _stage = StoryStage::INTRO;
    float _timer = 30.0f;
    bool  _trigger = false;
    int   _bosses_dead = 0;
};

// ============================================================
// D4 Step5.1 Hook Fields (只加字段, 不改行为)
// ============================================================

// Quest hook
struct QuestHook {
    WorldFlag  required_flag = WorldFlag::NONE;
    StoryStage required_stage = StoryStage::INTRO;
};

// NPC hook
struct NPCHook {
    WorldFlag  required_flag = WorldFlag::NONE;
    StoryStage required_stage = StoryStage::INTRO;
};

// Narrative hook: 条件旁白
struct NarrativeHook {
    WorldFlag  required_flag = WorldFlag::NONE;
    const char* alternative_text = nullptr;
};

// Boss hook (暂不实现逻辑)
struct BossHook {
    WorldFlag  required_flag = WorldFlag::NONE;
    StoryStage required_stage = StoryStage::INTRO;
    int        build_type_hint = 0;
    int        quest_id = 0;
};
