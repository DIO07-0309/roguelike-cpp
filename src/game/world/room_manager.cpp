#include "room_manager.h"
#include "game_map.h"
#include "config.h"
#include "monster.h"
#include "player.h"

// ============================================================
// Batch 2C: Room Encounter Manager implementation
// Constraints:
//   - build(): freeze room rects + door groups once (no runtime door search)
//   - tick():  only touches the active encounter room (IDLE rooms zero cost)
//   - decoupled from GameScene via callbacks (unit-testable)
// ============================================================

void RoomManager::build(GameMap* map,
                        const std::vector<std::tuple<int,int,int,int>>& rooms,
                        bool is_boss_floor) {
    _rooms.clear();
    _active = -1;
    _last_player_tile_x = -1;
    _last_player_tile_y = -1;
    if (!map) return;

    for (size_t i = 0; i < rooms.size(); i++) {
        RoomEntry e;
        e.rx = std::get<0>(rooms[i]);
        e.ry = std::get<1>(rooms[i]);
        e.rw = std::get<2>(rooms[i]);
        e.rh = std::get<3>(rooms[i]);
        e.is_boss_room = is_boss_floor && (int)i == (int)rooms.size() - 1;
        const int W = map->width, H = map->height;
        for (int x = e.rx; x < e.rx + e.rw; x++) {
            if (e.ry - 1 >= 0 && map->tile_at(x, e.ry - 1) == TileType::DOOR)
                e.door_tiles.push_back({x, e.ry - 1});
            if (e.ry + e.rh < H && map->tile_at(x, e.ry + e.rh) == TileType::DOOR)
                e.door_tiles.push_back({x, e.ry + e.rh});
        }
        for (int y = e.ry; y < e.ry + e.rh; y++) {
            if (e.rx - 1 >= 0 && map->tile_at(e.rx - 1, y) == TileType::DOOR)
                e.door_tiles.push_back({e.rx - 1, y});
            if (e.rx + e.rw < W && map->tile_at(e.rx + e.rw, y) == TileType::DOOR)
                e.door_tiles.push_back({e.rx + e.rw, y});
        }
        _rooms.push_back(std::move(e));
    }
}

int RoomManager::room_at(int tx, int ty) const {
    for (size_t i = 0; i < _rooms.size(); i++) {
        const RoomEntry& r = _rooms[i];
        if (tx >= r.rx && tx < r.rx + r.rw && ty >= r.ry && ty < r.ry + r.rh)
            return (int)i;
    }
    return -1;
}

int RoomManager::_monster_in_room(const RoomEntry& r, GameMap* map,
                                  const std::vector<std::unique_ptr<Monster>>& monsters) const {
    int n = 0;
    for (auto& m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        auto [tx, ty] = map->pixel_to_tile(m->entity.position.x, m->entity.position.y);
        if (tx >= r.rx && tx < r.rx + r.rw && ty >= r.ry && ty < r.ry + r.rh) n++;
    }
    return n;
}

// E1: player or monster rect overlapping a door tile
bool RoomManager::_entity_on_doors(const RoomEntry& r, GameMap* map, Player* player,
                                   const std::vector<std::unique_ptr<Monster>>& monsters) const {
    for (auto& [dx, dy] : r.door_tiles) {
        float door_px = (float)dx * map->tile_size + map->tile_size / 2.0f;
        float door_py = (float)dy * map->tile_size + map->tile_size / 2.0f;
        if (player) {
            const auto& pr = player->entity.rect;
            if (door_px >= pr.x && door_px <= pr.x + pr.width &&
                door_py >= pr.y && door_py <= pr.y + pr.height) return true;
        }
        for (auto& m : monsters) {
            if (!m || !m->combat.is_alive) continue;
            const auto& mr = m->entity.rect;
            if (door_px >= mr.x && door_px <= mr.x + mr.width &&
                door_py >= mr.y && door_py <= mr.y + mr.height) return true;
        }
    }
    return false;
}

// ARMED -> LOCKED: E1 (no entity on doors) + E2 (monsters in room) + E3 (atomic close)
void RoomManager::_try_lock(RoomEntry& r, GameMap* map, Player* player,
                            const std::vector<std::unique_ptr<Monster>>& monsters) {
    if (_entity_on_doors(r, map, player, monsters)) return;      // E1
    if (_monster_in_room(r, map, monsters) <= 0) return;         // no monsters
    if (!map->close_room_doors(r.door_tiles)) return;            // E3 atomic failure
    r.state = RoomEncounterState::LOCKED;
    if (_cb.on_locked) _cb.on_locked((int)(&r - &_rooms[0]));
}

// LOCKED -> CLEARED: room monsters cleared -> open doors + event
void RoomManager::_try_unlock(RoomEntry& r, GameMap* map,
                              const std::vector<std::unique_ptr<Monster>>& monsters) {
    int alive = _monster_in_room(r, map, monsters);
    if (alive > 0) return;
    map->open_room_doors(r.door_tiles);
    r.state = RoomEncounterState::CLEARED;
    if (_cb.on_cleared) _cb.on_cleared((int)(&r - &_rooms[0]));
    _active = -1;
}

void RoomManager::tick(GameMap* map, Player* player,
                       const std::vector<std::unique_ptr<Monster>>& monsters) {
    if (!map || !player) return;

    auto [ptx, pty] = map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width / 2,
        player->entity.rect.y + player->entity.rect.height / 2);
    if (ptx != _last_player_tile_x || pty != _last_player_tile_y) {
        _last_player_tile_x = ptx;
        _last_player_tile_y = pty;
        if (_active < 0) {
            int idx = room_at(ptx, pty);
            if (idx >= 0 && _rooms[idx].state == RoomEncounterState::IDLE) {
                RoomEntry& r = _rooms[idx];
                if (!r.is_boss_room && _monster_in_room(r, map, monsters) > 0) {
                    r.state = RoomEncounterState::ARMED;
                    _active = idx;
                }
            }
        }
    }

    if (_active < 0) return;
    RoomEntry& r = _rooms[_active];
    if (r.state == RoomEncounterState::ARMED) {
        _try_lock(r, map, player, monsters);
    } else if (r.state == RoomEncounterState::LOCKED) {
        _try_unlock(r, map, monsters);
    }
}

bool RoomManager::is_room_locked(int room_idx) const {
    return room_idx >= 0 && room_idx < (int)_rooms.size() &&
           _rooms[room_idx].state == RoomEncounterState::LOCKED;
}
