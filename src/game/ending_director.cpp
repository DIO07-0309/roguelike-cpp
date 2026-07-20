#include "ending_director.h"
#include "relationship_system.h"
#include "quest_manager.h"
#include "data/ending_defs.h"    // G2.5
#include "core/event_bus.h"      // G2.5: ENDING_REACHED

void EndingDirector::begin(const WorldState& ws, BossRank rank, float coll_pct,
                            const RelationshipSystem& rels,
                            const QuestManager& qm) {
    (void)qm;
    _npc_endings.clear();

    // 判定EndingType (算法不变)
    if (ws.has(WorldFlag::True_Ending_Ready) && rank >= BossRank::S && coll_pct >= 0.90f)
        _type = EndingType::SECRET_END;
    else if (ws.has(WorldFlag::True_Ending_Ready) && rank >= BossRank::A)
        _type = EndingType::TRUE_END;
    else if (ws.has(WorldFlag::All_Boss_Defeated)
             && ws.has(WorldFlag::Saved_Prisoner)
             && ws.has(WorldFlag::Saved_Priest))
        _type = EndingType::GOOD_END;
    else if (ws.has(WorldFlag::Boss3_Defeated))
        _type = EndingType::NORMAL_END;
    else // 死亡或Boss未全杀
        _type = EndingType::BAD_END;

    // ── G2.5: flavor text / meta reward 从 registry 读取 (替代 WORLD_ENDINGS[5]) ──
    const EndingDef* def = get_ending_def((int)_type);
    if (def) {
        _texts.sky   = def->sky_color;
        _texts.line  = def->final_line;
        _texts.title = def->title;
        _world = { _type,
                   _texts.sky.c_str(),
                   _texts.line.c_str(),
                   _texts.title.c_str(),
                   def->meta_soul_bonus,
                   def->meta_knowledge_bonus };
    } else {
        _world = { _type, "灰暗", "一切归于沉寂。", "END", 0, 0 };
    }

    // G2.5: 记录已解锁结局 (去重)
    {
        int et = (int)_type;
        bool found = false;
        for (int v : _unlocked) if (v == et) { found = true; break; }
        if (!found) _unlocked.push_back(et);
        // EventBus emit (保持与 Quest/Attack/Skill Evolution 一致的风格)
        EventBus::inst().emit(GameEventType::ENDING_REACHED, nullptr, et);
    }

    // NPC 结局
    _build_npc_endings(ws, rels);
}

void EndingDirector::_build_npc_endings(const WorldState& ws,
                                         const RelationshipSystem& rels) {
    // 埃德加 (F2)
    if (ws.has(WorldFlag::Saved_Prisoner))
        _npc_endings.push_back({20, "埃德加", "幸存",
            "他被关押三十年后终于重见天日。他的第一个自由日——也是地牢的第一个自由日。"});
    else
        _npc_endings.push_back({20, "埃德加", "继续等待",
            "铁门仍然紧锁着。他将继续独自等待——也许下一个冒险者会发现他。"});

    // 泰伦斯 (F7)
    if (ws.has(WorldFlag::Saved_Priest)) {
        _npc_endings.push_back({70, "泰伦斯", "希望复苏",
            "神官的祈祷终于得到了回应。虽然神明仍未归来——但至少，神殿不再黑暗。"});
    } else {
        _npc_endings.push_back({70, "泰伦斯", "最后的守望",
            "他继续在腐化的神殿里独自祈祷。终有一天——也许再也没有人去听。"});
    }

    // 朝圣者索拉斯 (F9)
    if (rels.check_relation(90, RelationType::RESPECT, 1))
        _npc_endings.push_back({90, "索拉斯", "继续前行",
            "他终于不再迷失——而是成为了后来的冒险者的指引者。"});
    else
        _npc_endings.push_back({90, "索拉斯", "永远迷失",
            "朝圣者之路的尽头——他还在那里。继续等待着永远不会到来的终点。"});

    // 守望者 (F14)
    if (rels.check_relation(140, RelationType::RESPECT, 2))
        _npc_endings.push_back({140, "守望者", "使命终结",
            "守望者推开了起源之门——这是三千年来第一次，也是最后一次。\n他终于可以休息了。"});
    else
        _npc_endings.push_back({140, "守望者", "独自留守",
            "三千年又三千年——他将继续守着那扇永远不会被打开的门。"});
}

const char* EndingDirector::ending_name() const {
    switch (_type) {
        case EndingType::BAD_END:    return "BAD END";
        case EndingType::NORMAL_END: return "NORMAL END";
        case EndingType::GOOD_END:   return "GOOD END";
        case EndingType::TRUE_END:   return "TRUE END";
        case EndingType::SECRET_END: return "ABSOLUTE END";
    }
    return "?";
}

const char* EndingDirector::ending_title() const {
    return _world.credits_title;
}

const char* EndingDirector::final_line() const {
    return _world.final_line;
}

const char* EndingDirector::sky_color() const {
    return _world.sky_color;
}

int EndingDirector::meta_reward_soul() const { return _world.meta_soul_bonus; }
int EndingDirector::meta_reward_knowledge() const { return _world.meta_knowledge_bonus; }
