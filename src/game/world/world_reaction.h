#pragma once
#include <string>
#include <vector>
#include "world_state.h"

// ============================================================
// D4 Step5.4: WorldReaction -- 世界实时响应层
// ============================================================
struct WorldReaction {
    WorldFlag required_flag;       // 哪个 flag 触发此 Reaction
    StoryStage stage = StoryStage::INTRO;  // 在哪个章节生效
    const char* narration = nullptr;   // 覆盖旁白
    int  tint_r = -1;                // 色调 overlay (-1=不覆盖)
    int  tint_g = -1;
    int  tint_b = -1;
    const char* ambience = nullptr;  // 环境音 hook
    const char* world_event = nullptr; // 世界事件 (替换 _tick_world_event 文本)
    bool once = false;              // 仅触发一次
};

class WorldReactionSystem {
public:
    WorldReactionSystem();

    // 根据当前 WorldState + StoryStage 查找匹配的 Reaction
    const WorldReaction* find(const WorldState& ws, StoryStage stage) const;

    // 当前旁白 (覆盖随机 narration)
    const char* current_narration(const WorldState& ws, StoryStage stage) const;

    // 当前色调
    void current_tint(const WorldState& ws, StoryStage stage,
                      unsigned char& r, unsigned char& g, unsigned char& b) const;

    // 当前世界事件
    const char* current_world_event(const WorldState& ws, StoryStage stage) const;

    // 当前环境音
    const char* current_ambience(const WorldState& ws, StoryStage stage) const;

private:
    std::vector<WorldReaction> _reactions;
};

// ---- 全局 accessor (game_scene 持有实例) ----
extern WorldReactionSystem g_reactions;
