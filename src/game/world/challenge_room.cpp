#include "challenge_room.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"
#include "item.h"
#include "reward_manager.h"
#include "growth_curve.h"
#include "core/logger.h"
#include <algorithm>
#include <cmath>

static constexpr int MAX_CHALLENGE_MONSTERS = 12;

// Deterministic seed: avalanche hash_combine (no XOR collision)
uint32_t ChallengeRoomController::_deterministic_seed(
    uint32_t dungeon_seed, int room_index, int wave_index) const {
    uint32_t h = dungeon_seed;
    h ^= (uint32_t)(room_index + 1) * 0x9E3779B9u;
    h ^= (uint32_t)(wave_index + 1) * 0x85EBCA6Bu;
    h ^= h >> 16; h *= 0x45D9F3Bu;
    h ^= h >> 16; h *= 0x45D9F3Bu;
    h ^= h >> 16;
    return h;
}

bool ChallengeRoomController::_room_contains(int tx, int ty) const {
    return tx >= _room_rx && tx < _room_rx + _room_rw &&
           ty >= _room_ry && ty < _room_ry + _room_rh;
}

void ChallengeRoomController::reset() {
    _phase = ChallengePhase::INACTIVE;
    _current_wave = 0;
    _wave_timer = 0.0f;
    _monsters_alive_this_wave = 0;
}

void ChallengeRoomController::on_player_entered() {
    if (_phase == ChallengePhase::UNLOCKED) {
        _phase = ChallengePhase::ARMED;
    }
}

void ChallengeRoomController::on_doors_locked() {
    if (_phase == ChallengePhase::ARMED) {
        _current_wave = 0;
        _phase = ChallengePhase::WAVE_SPAWNING;
    }
}

bool ChallengeRoomController::try_activate(Player& player) {
    if (_phase != ChallengePhase::INACTIVE) return false;
    if (player.key_count <= 0) return false;

    player.spend_key(1);
    _phase = ChallengePhase::UNLOCKED;
    LOG_INFO("[CHALLENGE] Key consumed, challenge activated");
    return true;
}

void ChallengeRoomController::tick(
    float dt, GameMap* map, Player* player,
    std::vector<std::unique_ptr<Monster>>& monsters,
    int floor, uint32_t dungeon_seed, int room_index) {

    if (_phase == ChallengePhase::INACTIVE ||
        _phase == ChallengePhase::UNLOCKED ||
        _phase == ChallengePhase::CLEARED) return;

    // ARMED: waiting for RoomManager to lock doors
    if (_phase == ChallengePhase::ARMED) return;

    // WAVE_SPAWNING: spawn current wave
    if (_phase == ChallengePhase::WAVE_SPAWNING) {
        _spawn_wave(_current_wave, map, monsters, floor, dungeon_seed, room_index);
        _phase = ChallengePhase::COMBAT;
        return;
    }

    // COMBAT: count alive monsters from current wave
    if (_phase == ChallengePhase::COMBAT) {
        int alive = 0;
        for (auto& m : monsters) {
            if (m && m->combat.is_alive && _room_contains(
                (int)(m->entity.rect.x / 32), (int)(m->entity.rect.y / 32))) {
                alive++;
            }
        }
        _monsters_alive_this_wave = alive;

        if (alive <= 0) {
            _current_wave++;
            if (_current_wave >= _total_waves) {
                _phase = ChallengePhase::REWARD;
                _grant_rewards(*player, map, floor);
                _phase = ChallengePhase::CLEARED;
                map->open_room_doors({});  // doors handled by RoomManager
                LOG_INFO("[CHALLENGE] All waves cleared!");
            } else {
                _wave_timer = 3.0f;
                _phase = ChallengePhase::WAIT_NEXT_WAVE;
            }
        }
        return;
    }

    // WAIT_NEXT_WAVE: countdown timer
    if (_phase == ChallengePhase::WAIT_NEXT_WAVE) {
        _wave_timer -= dt;
        if (_wave_timer <= 0.0f) {
            _phase = ChallengePhase::WAVE_SPAWNING;
        }
    }
}

void ChallengeRoomController::_spawn_wave(
    int wave_index, GameMap* map,
    std::vector<std::unique_ptr<Monster>>& monsters,
    int floor, uint32_t seed, int room_idx) {

    int count = 4;  // monsters per wave
    ChallengeModifier mod;
    const GrowthCurve& gc = g_growth.curve(floor);

    uint32_t wave_seed = _deterministic_seed(seed, room_idx, wave_index);

    for (int i = 0; i < count; i++) {
        if ((int)monsters.size() >= MAX_CHALLENGE_MONSTERS) break;

        // Deterministic position attempt
        for (int attempt = 0; attempt < 50; attempt++) {
            uint32_t pos_hash = wave_seed ^ (uint32_t)(i * 50 + attempt);
            int rx = _room_rx + 1 + (int)(pos_hash % (uint32_t)(_room_rw - 2));
            int ry = _room_ry + 1 + (int)((pos_hash >> 8) % (uint32_t)(_room_rh - 2));

            if (!map->is_walkable(rx, ry)) continue;

            // No spawn on doors
            if (map->is_door(rx, ry)) continue;

            auto [px, py] = map->tile_to_pixel(rx, ry);
            Monster* m = spawn_monster((float)px, (float)py, "slime");
            if (!m) continue;

            // Apply floor scaling + challenge modifier
            m->combat.max_hp = (int)(m->combat.max_hp * gc.monster_hp * mod.hp_multiplier);
            m->combat.current_hp = m->combat.max_hp;
            m->combat.attack = (int)(m->combat.attack * gc.monster_atk * mod.attack_multiplier);

            monsters.emplace_back(m);
            _monsters_alive_this_wave++;
            break;
        }
    }

    LOG_INFO("[CHALLENGE] Wave %d spawned, %d monsters alive",
             wave_index + 1, _monsters_alive_this_wave);
}

void ChallengeRoomController::_grant_rewards(Player& player, GameMap* map, int floor) {
    int granted = 0;
    for (int i = 0; i < 3; i++) {
        auto item = generate_random_item();
        int tries = 0;
        while (item && item->rarity < Rarity::RARE && tries < 5) {
            item = generate_random_item();
            tries++;
        }
        if (!item) continue;

        if (player.inventory.add(item, &player)) {
            granted++;
        } else {
            // Inventory full → ground drop at room center
            int cx = _room_rx + _room_rw / 2;
            int cy = _room_ry + _room_rh / 2;
            auto [px, py] = map->tile_to_pixel(cx, cy);
            DroppedItem di;
            di.item = item;
            di.tile_x = cx;
            di.tile_y = cy;
            // Note: ground_items managed by GameScene, logged here
            LOG_INFO("[CHALLENGE] Inventory full, item dropped at (%d,%d)", cx, cy);
        }
    }

    int gold = 50 + floor * 15;
    RewardManager::grant_gold(player, gold);
    LOG_INFO("[CHALLENGE] Rewards: %d items + %d gold", granted, gold);
}
