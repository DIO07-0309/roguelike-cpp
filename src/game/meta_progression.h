#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ============================================================
// D4.6 Step5: Meta Progression — 永久成长系统
// 跨Run持久数据: 天赋树/货币/成就/解锁
// ============================================================

struct MetaNode {
    const char* id;         // e.g. "hp_bonus"
    const char* name;       // "生命力强化"
    const char* desc;       // "+2% 最大HP"
    int  max_level = 5;
    int  cost_base = 3;     // 第一级消耗 Soul Fragments
    int  cost_scale = 2;    // 每级递增
};

// ═══ G3.5: Meta Reward Audit — 奖励来源 + 审计记录 ═══
enum class MetaRewardSource { RUN_SUMMARY, ENDING, QUEST, BOSS, DEBUG };

struct MetaCurrency {
    int soul_fragments = 0;
    int knowledge = 0;
    int ancient_memory = 0;
};

struct MetaRewardRecord {
    MetaRewardSource source;
    std::string detail;          // "F10 Boss" | "Quest: 救出囚犯" | "Run结算"
    MetaCurrency amount;
};

struct RunSummary {
    int  floor_reached = 0;
    int  bosses_killed = 0;
    int  total_kills = 0;
    int  elite_kills = 0;
    int  relics_collected = 0;
    int  quests_done = 0;
    int  combo_max = 0;
    int  total_dmg = 0;
    int  build_type = 0;       // BuildType int
    float play_time = 0.0f;
    // computed rewards
    MetaCurrency reward;
};

struct MetaSave {
    MetaCurrency currency;
    int node_levels[10] = {0};  // 每个MetaNode的等级
    int total_runs = 0;
};

// ---- 全局单例 ----
class MetaSystem {
public:
    MetaSystem();

    // 查询
    int   node_level(const char* id) const;
    float permanent_bonus(const char* id) const;  // 返回倍率
    const MetaCurrency& currency() const { return _save.currency; }
    int   total_runs() const { return _save.total_runs; }

    // 升级
    bool upgrade_node(const char* id, MetaCurrency& cost);

    // Run收尾
    MetaCurrency end_run(const RunSummary& summary);
    void add_currency(const MetaCurrency& c);

    // G3.5: 统一奖励入口 + 审计日志
    void reward_from_ending(const char* ending_name,
                            int soul, int knowledge, int ancient_memory = 2);
    void clear_reward_log();               // 新 Run 开始时调用
    const std::vector<MetaRewardRecord>& reward_log() const { return _reward_log; }

    // G3.1: 从 registry 重建 MetaNode (在 JSON 加载后调用)
    void load_from_defs();
    // 存档
    bool save() const;
    bool load();
    const MetaSave& data() const { return _save; }

private:
    MetaSave _save;
    MetaNode _nodes[10];  // 10个节点
    int     _node_count = 0;
    // G3.1: string storage for MetaNode const char* fields (from registry)
    std::vector<std::string> _node_texts;
    // G3.5: 本次 run 的奖励审计日志
    std::vector<MetaRewardRecord> _reward_log;
};

extern MetaSystem g_meta;

// ---- 构造函数填充 RunSummary ----
MetaCurrency calc_run_reward(const RunSummary& rs);
