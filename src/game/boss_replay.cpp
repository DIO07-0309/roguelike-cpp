#include "boss_replay.h"
#include <algorithm>

int BossReplayMemory::analyze_strategy() const {
    // 0=冲脸, 1=远程压制, 2=反治疗, 3=延迟攻击, 4=AOE, 5=均衡
    if (ranged_hits > melee_hits * 2) return 1;    // 远程多→冲脸
    if (potion_used >= 3)              return 2;    // 嗑药多→反治疗
    if (dodge_count > 10)              return 3;    // 翻滚多→延迟攻击
    if (melee_hits > 15 && skills_used < 5) return 4; // 近战多技能少→AOE
    if (combo_max >= 15)               return 0;    // combo高→近身压制
    return 5;
}

const char* BossReplayMemory::strategy_name() const {
    int s = analyze_strategy();
    const char* names[] = {"近身压制","远程冲脸","反治疗","延迟攻击","AOE控制","均衡"};
    return names[s];
}

BossRank calc_boss_rank(const BossReplayMemory& mem, int dmg_taken, int dmg_done, float time) {
    int score = 0;
    if (dmg_done > dmg_taken * 2) score += 20;
    else if (dmg_done > dmg_taken) score += 10;
    if (mem.combo_max >= 20) score += 15;
    else if (mem.combo_max >= 10) score += 8;
    if (time < 45) score += 10;
    if (mem.potion_used <= 1) score += 8;
    if (mem.dodge_count < 5) score += 5;

    if (score >= 50) return BossRank::SS;
    if (score >= 40) return BossRank::S;
    if (score >= 30) return BossRank::A;
    if (score >= 20) return BossRank::B;
    if (score >= 10) return BossRank::C;
    return BossRank::D;
}

BossBattleReport generate_battle_report(const BossReplayMemory& mem, int dmg_d, int dmg_t, float time, int az) {
    BossBattleReport r;
    r.replay = mem; r.total_damage = dmg_d; r.damage_taken = dmg_t;
    r.battle_time = time; r.arena_zones_spawned = az;
    r.rank = calc_boss_rank(mem, dmg_t, dmg_d, time);
    return r;
}

const char* BossBattleReport::rank_name() const {
    switch (rank) {
        case BossRank::SS: return "SS";
        case BossRank::S:  return "S";
        case BossRank::A:  return "A";
        case BossRank::B:  return "B";
        case BossRank::C:  return "C";
        default: return "D";
    }
}
