#pragma once
#include <vector>
#include <tuple>
#include <memory>
#include <utility>
#include <functional>
#include <cstdint>

class GameMap;
class GameScene;
class Player;
class Monster;

// ============================================================
// Batch 2C: Room Encounter Manager
// 以撒式房间战斗: 进有怪房 → 封门 → 清房 → 开门
// 约束 (用户审核):
//   1. 只维护/检查当前激活的 Encounter (IDLE 房间不扫描) — 避免每帧全图扫描
//   2. 房间↔门组映射在 build() 时一次性建立并固化 — 运行时不重新搜索门
//   3. 与 GameScene 解耦 (回调注入) — 可单元测试, 职责边界清晰
// ============================================================

enum class RoomEncounterState {
    IDLE,    // 无怪 / 已清房 / Boss 房不启用
    ARMED,   // 玩家已进入且房内存在活怪 (待锁)
    LOCKED,  // 门组已 CLOSED (战斗进行中)
    CLEARED  // 房内怪全灭, 门已 OPEN (本层内永久)
};

struct RoomEntry {
    RoomEncounterState state = RoomEncounterState::IDLE;
    int rx = 0, ry = 0, rw = 0, rh = 0;                 // 房间矩形 (build 时固化)
    std::vector<std::pair<int,int>> door_tiles;          // 门组 (build 时固化, 运行时不搜索)
    bool is_boss_room = false;
};

// Room Encounter 事件回调 (由 GameScene 注入; 测试可注入记录器)
struct RoomEncounterCallbacks {
    std::function<void(int room_idx)> on_locked;    // 封门时
    std::function<void(int room_idx)> on_cleared;   // 清房开门时
};

class RoomManager {
public:
    void set_callbacks(const RoomEncounterCallbacks& cb) { _cb = cb; }

    // 进层时构建 (一次): 房间矩形 + 门组映射全部固化
    void build(GameMap* map, const std::vector<std::tuple<int,int,int,int>>& rooms,
               bool is_boss_floor);

    // 每帧 tick: 仅处理 当前激活房间 (IDLE 房间零开销)
    void tick(GameMap* map, Player* player,
              const std::vector<std::unique_ptr<Monster>>& monsters);

    bool is_room_locked(int room_idx) const;
    int room_at(int tx, int ty) const;   // 玩家所在房间索引 (-1=走廊)

private:
    std::vector<RoomEntry> _rooms;
    int _active = -1;
    int _last_player_tile_x = -1;
    int _last_player_tile_y = -1;
    RoomEncounterCallbacks _cb;

    int _monster_in_room(const RoomEntry& r, GameMap* map,
                         const std::vector<std::unique_ptr<Monster>>& monsters) const;
    bool _entity_on_doors(const RoomEntry& r, GameMap* map, Player* player,
                          const std::vector<std::unique_ptr<Monster>>& monsters) const;
    void _try_lock(RoomEntry& r, GameMap* map, Player* player,
                   const std::vector<std::unique_ptr<Monster>>& monsters);
    void _try_unlock(RoomEntry& r, GameMap* map,
                     const std::vector<std::unique_ptr<Monster>>& monsters);
};
