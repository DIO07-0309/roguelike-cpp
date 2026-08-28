#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

class Player;
class GameMap;
class Monster;

// ============================================================
// Batch 3F: Challenge Room — 波次战斗挑战
// ============================================================

enum class ChallengePhase : uint8_t {
    INACTIVE,       // 门 CLOSED, 未交互
    UNLOCKED,       // Key 消耗, 门 OPEN, 玩家可进入
    ARMED,          // 玩家进入房间, RoomManager 即将 LOCKED
    WAVE_SPAWNING,  // 当前波次正在生成
    COMBAT,         // 等待当前波次怪物全部死亡
    WAIT_NEXT_WAVE, // 波次间隔计时 (3s)
    REWARD,         // 发放奖励
    CLEARED         // 挑战完成, 门 OPEN
};

struct ChallengeModifier {
    float hp_multiplier = 1.5f;
    float attack_multiplier = 1.3f;
    int extra_buffs = 1;
};

struct ChallengeWave {
    int monster_count = 4;
    float spawn_delay = 3.0f;
};

class ChallengeRoomController {
public:
    void reset();

    // 激活: 检查 Key → 消耗 → 开门
    bool try_activate(Player& player);

    // 每帧 tick
    void tick(float dt, GameMap* map, Player* player,
              std::vector<std::unique_ptr<Monster>>& monsters,
              int floor, uint32_t dungeon_seed, int room_index);

    // 玩家进入房间时调用
    void on_player_entered();

    // RoomManager 通知: 门已 LOCKED
    void on_doors_locked();

    ChallengePhase phase() const { return _phase; }
    bool is_cleared() const { return _phase == ChallengePhase::CLEARED; }

    // 波次信息 (UI 显示用)
    int current_wave() const { return _current_wave; }
    int total_waves() const { return _total_waves; }

private:
    ChallengePhase _phase = ChallengePhase::INACTIVE;
    int _current_wave = 0;
    int _total_waves = 3;
    float _wave_timer = 0.0f;
    int _monsters_alive_this_wave = 0;
    int _room_rx = 0, _room_ry = 0, _room_rw = 0, _room_rh = 0;

    void _spawn_wave(int wave_index, GameMap* map,
                     std::vector<std::unique_ptr<Monster>>& monsters,
                     int floor, uint32_t seed, int room_idx);
    void _grant_rewards(Player& player, GameMap* map, int floor);
    bool _room_contains(int tx, int ty) const;
    uint32_t _deterministic_seed(uint32_t dungeon_seed, int room_index, int wave_index) const;
};
