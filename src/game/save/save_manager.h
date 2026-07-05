#pragma once
#include <string>
#include <memory>

class Player;

struct SaveData {
    std::unique_ptr<Player> player;
    int current_floor = 1;
    int max_unlocked_floor = 1;
};

class SaveManager {
public:
    static bool save_exists();
    static bool save_game(Player* player, int floor, int max_floor);
    static SaveData* load_save();
    static void delete_save();

private:
    static std::string _save_dir();
    static std::string _save_path();
};
