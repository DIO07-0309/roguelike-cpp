#pragma once
// ============================================================
// D7 Step3: world_types.h — 世界系统共享数据结构
// 引用 world/ 目录中定义的权威类型。
// 后续新模块统一通过此文件获取所有 world 类型。
// ============================================================

// 核心类型 (world/ 中定义的权威定义)
#include "event_system.h"    // EventType, DungeonEvent
#include "world_state.h"     // WorldFlag, WorldState, StoryStage
#include "floor_config.h"    // FloorConfig, ChapterConfig
#include "floor_narrative.h" // FloorNarrative, NarrativeState
#include "npc_system.h"      // NPCData, NPCState, DialogueState, DialoguePage
#include "growth_curve.h"    // GrowthCurve
#include "flow_director.h"   // FlowTimer
#include "quest_manager.h"   // Quest, QuestReward, QuestState
#include "relationship_system.h"  // NPCRelation, RelationReward
#include "world_reaction.h"  // WorldReaction
