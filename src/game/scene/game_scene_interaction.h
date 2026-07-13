#pragma once
#include <string>
#include <vector>
#include "raylib.h"

struct InputMap;
class GameScene;
enum class EventType;

// 从 GameScene 提取的事件 + NPC/对话模块
class GameSceneInteraction {
public:
    explicit GameSceneInteraction(GameScene& scene) : _s(scene) {}

    // Event
    void start_event_presentation(EventType et);
    void tick_event_ui(float dt);
    void draw_event_ui(int sw, int sh);

    // NPC / Dialogue
    void spawn_floor_npcs(int floor, const std::vector<std::pair<int,int>>& rooms);
    void start_dialogue(int npc_index);
    void update_dialogue(float dt);
    void draw_dialogue(int sw, int sh);
    void draw_quest_log(int sw, int sh);

private:
    GameScene& _s;
};
