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
    // B13: Relic 不再跨层 (存档不保存圣物)
    // ── G1 Step7: Save v2 新增 ──
    int attack_evo_level = 1;                    // 普攻进化等级
    std::unordered_map<std::string, int> rule_counters; // rule_* counter
    // ── G2.4: Save v3 新增 ──
    std::unordered_map<int, int> quest_states;   // quest_id → QuestState
    // ── G2.5: Save v3 新增 ──
    std::vector<int> unlocked_endings;           // 已解锁 EndingType 列表
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
                          const std::vector<int>& unlocked_endings = {});
    static SaveData* load_save();
    static void delete_save();

private:
    static std::string _save_dir();
    static std::string _save_path();
    // B8: spr 序列化辅助
    static std::string _encode_spr(const std::vector<bool>& v);
    static std::vector<bool> _decode_spr(const std::string& s);
};
