#include "quest_manager.h"
#include "player.h"
#include "combat_system.h"
#include "relationship_system.h"
#include "core/logger.h"

// ============================================================
// D4 Step5.2: 7个预设任务 + D4 Step5.3: 关系奖励
// ============================================================
const int QUEST_SAVE_PRISONER   = 0;
const int QUEST_SAVE_PRIEST     = 1;
const int QUEST_KILL_WARDEN     = 2;
const int QUEST_KILL_FIRE_DEMON = 3;
const int QUEST_TALK_PILGRIM    = 4;
const int QUEST_TALK_WATCHER    = 5;
const int QUEST_KILL_ABYSS      = 6;

// helper: make QuestReward inline
static QuestReward REW(const char* rid, const char* bid, int bs, WorldFlag sf,
                        const char* ck, int ca, const char* sm,
                        int npc, int t, int r, int f, int g, int c) {
    QuestReward rw;
    rw.relic_id = rid; rw.buff_id = bid; rw.buff_stacks = bs;
    rw.set_flag = sf; rw.counter_name = ck; rw.counter_add = ca;
    rw.story_msg = sm;
    rw.relation.npc_id = npc;
    rw.relation.trust = t; rw.relation.respect = r;
    rw.relation.fear = f; rw.relation.gratitude = g; rw.relation.corruption = c;
    return rw;
}

QuestManager::QuestManager() {
    _quests = {
        {0, "救出囚犯", "F2的囚犯埃德加已经三十年没见过新面孔了。",
         QuestState::AVAILABLE, 1, false,
         WorldFlag::NONE, StoryStage::CHAPTER_1, 0, "",
         WorldFlag::NONE,
         REW("","",0,WorldFlag::Saved_Prisoner,"prisoner_saved",1,"囚犯的祝福似乎让你更勇敢了.",
             20,2,0,0,0,0),
         0, false},

        {1, "谒见神官", "F7神殿最后的祭司——泰伦斯。",
         QuestState::AVAILABLE, 1, false,
         WorldFlag::NONE, StoryStage::CHAPTER_2, 0, "",
         WorldFlag::NONE,
         REW("","",0,WorldFlag::Saved_Priest,"priest_trust",2,"神官的祈祷仍然在神殿中回荡。",
             70,3,1,0,0,0),
         0, false},

        {2, "击杀狱卒", "暗影骑士——第一狱守。",
         QuestState::AVAILABLE, 1, false,
         WorldFlag::NONE, StoryStage::CHAPTER_1, 0, "",
         WorldFlag::Boss1_Defeated,
         REW("","attack_up",1,WorldFlag::NONE,"boss_kills",1,"暗影骑士的盔甲化为了灰烬。",
             0,0,1,0,0,0),
         QUEST_TALK_PILGRIM, false},

        {3, "击败火魔", "地狱火魔——熔岩深渊的远古恶魔。",
         QuestState::LOCKED, 1, false,
         WorldFlag::Boss1_Defeated, StoryStage::CHAPTER_2, 0, "",
         WorldFlag::Boss2_Defeated,
         REW("","attack_up",2,WorldFlag::NONE,"boss_kills",1,"火焰终于熄灭了。",
             0,0,1,0,0,0),
         QUEST_TALK_WATCHER, false},

        {4, "对话朝圣者", "F9迷失的朝圣者索拉斯。",
         QuestState::LOCKED, 1, false,
         WorldFlag::Boss1_Defeated, StoryStage::CHAPTER_2, 0, "",
         WorldFlag::NONE,
         REW("","",0,WorldFlag::NONE,"pilgrim_met",1,"索拉斯的目光穿过黑暗看着你。",
             90,0,1,0,0,0),
         0, false},

        {5, "守望者之约", "F14铸光者最后的弟子。",
         QuestState::LOCKED, 1, false,
         WorldFlag::Boss2_Defeated, StoryStage::CHAPTER_3, 0, "",
         WorldFlag::NONE,
         REW("","",0,WorldFlag::NONE,"watcher_met",1,"你被守望了三千年。",
             140,1,2,0,0,0),
         QUEST_KILL_ABYSS, false},

        {6, "终结深渊", "决战深渊之主·终焉。",
         QuestState::LOCKED, 1, false,
         WorldFlag::Boss2_Defeated, StoryStage::FINAL, 0, "",
         WorldFlag::Boss3_Defeated,
         REW("","",0,WorldFlag::All_Boss_Defeated,"boss_kills",1,"深渊终于安静了。",
             0,0,3,0,0,0),
         0, false},
    };
}

// ---- 自动解锁 & 自动完成 ----
void QuestManager::update(WorldState& ws, const StoryDirector& story) {
    for (auto& q : _quests) {
        _auto_unlock(q, ws, story);
        _auto_complete(q, ws);
    }
}

void QuestManager::_auto_unlock(Quest& q, const WorldState& ws, const StoryDirector& story) {
    if (q.state != QuestState::LOCKED) return;
    if (q.required_stage != StoryStage::INTRO && (int)story.stage() < (int)q.required_stage)
        return;
    if (q.required_flag != WorldFlag::NONE && !ws.has(q.required_flag))
        return;
    if (q.required_counter > 0 && !q.counter_key.empty()
        && ws.counter(q.counter_key) < q.required_counter)
        return;
    q.state = QuestState::AVAILABLE;
    if (q.auto_accept) q.state = QuestState::ACCEPTED;
    LOG_INFO("[QUEST] Unlocked #%d: %s", q.id, q.title.c_str());
}

void QuestManager::_auto_complete(Quest& q, WorldState& ws) {
    if (q.state != QuestState::ACCEPTED && q.state != QuestState::AVAILABLE) return;
    if (q.complete_flag == WorldFlag::NONE) return;
    if (!ws.has(q.complete_flag)) return;
    q.state = QuestState::COMPLETED;
    if (q.reward.set_flag != WorldFlag::NONE) ws.set(q.reward.set_flag);
    if (!q.reward.counter_name.empty()) ws.add_counter(q.reward.counter_name, q.reward.counter_add);
    if (_rels && q.reward.relation.npc_id != 0) _rels->apply_reward(q.reward.relation);
    LOG_INFO("[QUEST] Completed #%d: %s", q.id, q.title.c_str());
}

// ---- 玩家操作 ----
bool QuestManager::accept(int quest_id, WorldState& ws) {
    Quest* q = find(quest_id);
    if (!q || q->state != QuestState::AVAILABLE) return false;
    q->state = QuestState::ACCEPTED;
    (void)ws;
    return true;
}

void QuestManager::complete(int quest_id, WorldState& ws, Player* player) {
    Quest* q = find(quest_id);
    if (!q || q->state != QuestState::ACCEPTED) return;
    q->state = QuestState::COMPLETED;
    grant_reward(q->reward, player, ws);
}

void QuestManager::fail(int quest_id) {
    Quest* q = find(quest_id);
    if (q) q->state = QuestState::FAILED;
}

// ---- 奖励 ----
void QuestManager::grant_reward(const QuestReward& r, Player* player, WorldState& ws) {
    if (!player) return;
    if (!r.relic_id.empty()) {
        const RelicDef* def = get_relic_def(r.relic_id);
        if (def && !player_has_relic(player, r.relic_id))
            player->relics.push_back({r.relic_id});
    }
    if (!r.buff_id.empty()) apply_buff(player, r.buff_id, r.buff_stacks);
    if (r.set_flag != WorldFlag::NONE) ws.set(r.set_flag);
    if (!r.counter_name.empty()) ws.add_counter(r.counter_name, r.counter_add);
    if (_rels && r.relation.npc_id != 0) _rels->apply_reward(r.relation);
}

// ---- 查询 ----
Quest* QuestManager::find(int quest_id) {
    for (auto& q : _quests) if (q.id == quest_id) return &q;
    return nullptr;
}

Quest* QuestManager::get_npc_quest(int npc_floor) {
    if (npc_floor == 2) return find(0);
    if (npc_floor == 7) return find(1);
    if (npc_floor == 9) return find(4);
    if (npc_floor == 14) return find(5);
    return nullptr;
}

int QuestManager::available_count() const {
    int n = 0;
    for (auto& q : _quests) if (q.state == QuestState::AVAILABLE) n++;
    return n;
}

int QuestManager::completed_count() const {
    int n = 0;
    for (auto& q : _quests) if (q.state == QuestState::COMPLETED) n++;
    return n;
}
