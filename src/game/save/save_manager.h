#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>

class Player;

struct SaveData {
    std::unique_ptr<Player> player;
    int current_floor = 1;
    int max_unlocked_floor = 1;
    uint32_t dungeon_seed = 0;                  // B8: 当前楼层地牢种子
    std::vector<bool> special_triggered;         // B8: 特殊房间触发状态
    std::vector<bool> special_discovered;        // B10: 特殊房间发现状态
    // Batch 3A: Relic save/load via PersistenceScope (v4+)
    // ── G1 Step7: Save v2 新增 ──
    int attack_evo_level = 1;                    // 普攻进化等级
    std::unordered_map<std::string, int> rule_counters; // rule_* counter
    // ── G2.4: Save v3 新增 ──
    std::unordered_map<int, int> quest_states;   // quest_id → QuestState
    // ── G2.5: Save v3 新增 ──
    std::vector<int> unlocked_endings;           // 已解锁 EndingType 列表
    // ── G10.1: Element Core ──
    int element_level = 1;
    int element_exp = 0;
    int element_type = 0;  // 0=NONE, 1=FIRE, 2=ICE, 3=POISON
    bool element_initialized = false;
    // ── M4e: 跨对局镜像 AI 记忆 (72 alpha + 72 beta, 桶-major) ──
    std::vector<float> mirror_prior_alpha;
    std::vector<float> mirror_prior_beta;
};

class SaveManager {
public:
    static bool save_exists();
    static bool save_game(Player* player, int floor, int max_floor,
                          uint32_t dungeon_seed = 0,
                          const std::vector<bool>& special_triggered = {},
                          const std::vector<bool>& special_discovered = {},
                          const std::unordered_map<std::string, int>& rule_counters = {},
                          const std::unordered_map<int, int>& quest_states = {},
                          const std::vector<int>& unlocked_endings = {},
                          const std::vector<float>& mirror_prior_alpha = {},
                          const std::vector<float>& mirror_prior_beta = {});
    static SaveData* load_save();
    static void delete_save();

    // Q3.1: --sim 模式禁止覆盖玩家存档
    static bool g_sim_readonly;

private:
    static std::string _save_dir();
    static std::string _save_path();
    // B8: spr 序列化辅助
    static std::string _encode_spr(const std::vector<bool>& v);
    static std::vector<bool> _decode_spr(const std::string& s);
    // M4e: float 列表序列化辅助 (文件内静态函数, 不占类接口)
};
