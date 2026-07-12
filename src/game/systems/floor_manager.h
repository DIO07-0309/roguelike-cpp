#pragma once
#include <vector>
#include <memory>
#include <string>
#include <utility>
#include <cstdint>

class Player;
class Monster;
class GameMap;
class InputMap;

// ============================================================
// FloorManager — 楼层生成、怪物生成、楼层切换
// ============================================================
class FloorManager {
public:
    // 在房间中生成楼层怪物 (含难度倍率)
    static void spawn_floor_monsters(int floor_number, GameMap* map,
                                      std::vector<std::unique_ptr<Monster>>& out_monsters,
                                      const std::vector<std::pair<int,int>>& rooms);

    // 检查楼层是否清空
    static bool is_floor_cleared(const std::vector<std::unique_ptr<Monster>>& monsters);

    // 检查是否应下楼 (玩家在楼梯上按了 >键)
    // 返回: -1=不下楼, MAX_FLOORS=通关, 其他=下楼
    static int check_floor_transition(const InputMap& input, int current_floor,
                                       GameMap* map, const Player* player,
                                       std::pair<int,int> stairs_pos);
};
