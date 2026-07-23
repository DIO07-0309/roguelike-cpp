#include "state_hash.h"
#include "entities/player.h"
#include "entities/monster.h"
#include <cstdint>
#include <algorithm>

// ── simple hash mixer ──
static uint64_t _mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

uint64_t compute_state_hash(uint64_t prev,
    const Player& player, int floor,
    const std::vector<std::unique_ptr<Monster>>& monsters) {
    uint64_t h = prev;

    // player state
    h = _mix(h, (uint64_t)player.combat.current_hp);
    h = _mix(h, (uint64_t)player.combat.max_hp);
    h = _mix(h, (uint64_t)player.level);
    h = _mix(h, (uint64_t)(int)player.entity.position.x);
    h = _mix(h, (uint64_t)(int)player.entity.position.y);

    // floor
    h = _mix(h, (uint64_t)floor);

    // monsters: per-monster hash (id, hp) sorted for determinism
    struct MRec {
        int type_int;
        int hp;
        float x, y;
    };
    std::vector<MRec> recs;
    for (auto& m : monsters) {
        if (!m->combat.is_alive) continue;
        recs.push_back({
            (int)m->monster_type,
            m->combat.current_hp,
            m->entity.position.x,
            m->entity.position.y
        });
    }
    std::sort(recs.begin(), recs.end(),
        [](auto& a, auto& b) {
            if (a.type_int != b.type_int) return a.type_int < b.type_int;
            if (a.hp != b.hp) return a.hp < b.hp;
            if (a.x != b.x) return a.x < b.x;
            return a.y < b.y;
        });
    for (auto& r : recs) {
        h = _mix(h, (uint64_t)r.type_int);
        h = _mix(h, (uint64_t)r.hp);
        h = _mix(h, (uint64_t)(int)r.x);
        h = _mix(h, (uint64_t)(int)r.y);
    }
    h = _mix(h, (uint64_t)recs.size());
    return h;
}

bool verify_hash_chain(const std::vector<uint64_t>& expected,
                       const std::vector<uint64_t>& actual) {
    size_t n = std::min(expected.size(), actual.size());
    for (size_t i = 0; i < n; i++)
        if (expected[i] != actual[i])
            return false;
    return expected.size() == actual.size();
}
