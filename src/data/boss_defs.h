#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "core/registry_provider.h"  // G4.1: MergeMode

// ============================================================
// G1 Step6: BossDef — Boss 配置数据 (纯数据, 无 Gameplay/Render 依赖)
// 与 EnemyDef 独立 — Boss 有自己的生命周期/Narrative/Phase/Replay
// 属于 Data 层，放在 src/data/
// ============================================================

// ── Boss 技能参数 (cooldown / damage_mult / windup / range) ──
struct BossSkillDef {
    std::string id;             // "charge" | "shockwave" | "summon"
    float cooldown = 6.0f;
    float damage_mult = 1.0f;   // 伤害倍率 (charge:2.5, shockwave:1.6)
    float windup = 0.6f;        // 蓄力时间 (s)
    float range = 100.0f;       // AOE 范围 (shockwave)
};

// ── G2.3: BossArenaDef — Boss 战场配置 (Boss 知道自己战场用什么) ──
struct BossArenaDef {
    std::string danger_type = "shadow_wall";  // "lava"|"shadow_wall"|"void_crack"|"fire_column"|"none"
    float spawn_interval = 8.0f;              // 生成间隔(秒)
    int   max_zones = 6;                       // 最大同时zone数
    float zone_duration = 3.0f;                // 单个zone持续(秒)
    float spawn_radius = 120.0f;               // 生成范围(距Boss像素)
};

// ── Boss 模板 (一条 JSON 记录) ──
struct BossDef {
    std::string id;             // "shadow_knight" | "necromancer" | "fire_demon" | ...
    std::string name;           // 显示名 "暗影骑士"
    std::string title;          // "第一狱守·暗影骑士"
    std::string lore;           // 开场 lore 文本
    std::string visual_id;      // 表现层标识 (非 Color，与 EnemyDef 统一)

    int floor = 5;              // Boss 楼层 (5/10/15)
    bool is_summoner = false;   // Necromancer: 召唤特化
    bool is_defender = false;   // Golem: 防御特化

    // 基础数值
    int hp = 250, atk = 15, pdef = 10, mdef = 4;

    // Phase2 参数 (替代硬编码 0.5/1.5/1.25/0.7)
    float phase2_hp_threshold = 0.50f;  // HP 触发阈值
    float phase2_pause = 0.50f;         // 进入暂停时间
    float phase2_speed_mult = 1.50f;    // 移速倍率
    float phase2_atk_mult = 1.25f;      // 攻击倍率
    float phase2_cd_mult = 0.70f;       // 冷却倍率 (<1 = 更快)

    // 特殊参数
    float shield_pct = 0.0f;            // Golem: DEFEND 减伤比例
    float summon_speed = 1.0f;          // Necromancer: Phase2 召唤倍率
    int skill_cycle_bias = 6;           // 技能循环模式 (6=normal, 4=summon-heavy)

    // 技能冷却覆盖 (按 id 匹配，覆盖 BossSkill 默认 CD)
    std::vector<BossSkillDef> skill_overrides;

    // ── G2.3: Arena 战场配置 ──
    BossArenaDef arena;
};

// ── G2.3: ArenaEvent — BossAI 意图 ↔ ArenaManager 执行的中间层 ──
// 不携带 DangerType (由 BossArenaDef 决定)
// 不携带坐标 (由 ArenaManager 运行时计算)
enum class ArenaEventType { SPAWN_ZONE, CLEAR_ALL, INTENSIFY };

struct ArenaEvent {
    ArenaEventType type = ArenaEventType::SPAWN_ZONE;
    int   count = 1;
    float duration = 3.0f;
};

// ── 全局注册表 ──
bool load_boss_defs(const std::string& json_path);
int  load_boss_defs_from_json(const char* json_text, MergeMode mode,
                              const char* id_namespace = nullptr);  // G4.2: namespace
const BossDef* get_boss_def(const std::string& id);
const BossDef* get_boss_def_for_floor(int floor);  // 返回该层默认 Boss (供 UI)
const BossDef* get_boss_def_for_type(int type_int); // BossType int → BossDef
const std::unordered_map<std::string, BossDef>& get_all_boss_defs();  // G2.0
bool is_boss_defs_loaded();  // G2.0
