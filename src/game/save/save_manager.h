#pragma once
#include <string>
#include <memory>
#include <vector>
#include <cstdint>

class Player;

struct SaveData {
    std::unique_ptr<Player> player;
    int current_floor = 1;
    int max_unlocked_floor = 1;
    uint32_t dungeon_seed = 0;                  // B8: 当前楼层地牢种子
    std::vector<bool> special_triggered;         // B8: 特殊房间触发状态
    std::vector<bool> special_discovered;        // B10: 特殊房间发现状态
};

class SaveManager {
public:
    static bool save_exists();
    static bool save_game(Player* player, int floor, int max_floor,
                          uint32_t dungeon_seed = 0,
                          const std::vector<bool>& special_triggered = {},
                          const std::vector<bool>& special_discovered = {});
    static SaveData* load_save();
    static void delete_save();

private:
    static std::string _save_dir();
    static std::string _save_path();
    // B8: spr 序列化辅助
    static std::string _encode_spr(const std::vector<bool>& v);
    static std::vector<bool> _decode_spr(const std::string& s);
};
