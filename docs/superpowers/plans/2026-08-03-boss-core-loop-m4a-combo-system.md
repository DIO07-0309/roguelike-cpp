# BOSS 核心环 M4a：连招系统实施计划（暗影骑士样板）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 激活 BossSkillQueue 为真实连招执行器：暗影骑士按 JSON combo 模板出招（弹幕/扇形斩/瞬移），其余 Boss 保持旧循环不变。

**Architecture:** BossDef 新增 `combos` 数据 → BossAI 内部 `_combo_queue`（BossSkillQueue 复用）驱动 → 新 BossCommand（RANGED/CONE/BLINK/WHIRLWIND/NORMAL）→ 新 BossState + 新技能类（Barrage/Cone/Blink）。无 combos 配置的 Boss 走原 `_next_cycle_skill` 路径（零回归）。

**Tech Stack:** C++17、nlohmann/json、Raylib（CheckCollisionCircleRec）

## Global Constraints

- 任何函数 ≤ 40 行；一个类一件事；组合优于继承；变量语义化
- `enum class`；`#pragma once`；JSON 字段 snake_case；不引入新第三方库
- 修改 JSON 后必须跑 `conda run python tools/world_validator.py`
- 构建：`$env:PATH = "C:\mingw64\bin;$env:PATH"; cmake --build --preset release`
- 每批完成后按 CLAUDE.md 第 11 条同步桌面打包版 `C:\Users\HP\Desktop\Roguelike-CPP版`

---

### Task 1: 数据层 — combos/新技能配置 + 解析

**Files:**
- Modify: `resources/buffs.json`（加 poison2s）
- Modify: `resources/bosses.json`（shadow_knight 加 skills + combos）
- Modify: `src/data/boss_defs.h`（ComboDef 结构 + BossDef.combos）
- Modify: `src/data/boss_defs.cpp:30-74`（_parse_boss 解析 combos）

**Interfaces:**
- Consumes: 现有 `_parse_skill`（boss_defs.cpp:19）
- Produces: `struct ComboDef { std::string id; std::vector<std::string> commands; float interval; float end_delay; }`、`BossDef::combos: std::vector<ComboDef>`、buff id `"poison2s"`、boss 技能覆盖 id `"barrage"/"cone"/"blink"`

- [ ] **Step 1: buffs.json 加 poison2s（紧随 pool_poison 行后）**

```json
{"id":"poison2s","display_name":"中毒","short_name":"毒","duration":2.0,"max_stacks":5,"tick_interval":0.5,"tick_damage":3,"hud_color":[100,220,80]},
```

- [ ] **Step 2: bosses.json 暗影骑士（仅第 1 个对象）skills 追加 3 项 + 新增 combos 字段**

```json
    "skills": [
      {"id": "charge",   "cooldown": 6.0, "damage_mult": 2.5, "windup": 0.6, "range": 120},
      {"id": "shockwave","cooldown": 8.0, "damage_mult": 1.6, "windup": 0.7, "range": 100},
      {"id": "summon",   "cooldown": 12.0,"damage_mult": 1.0, "windup": 0.0, "range": 80},
      {"id": "barrage",  "cooldown": 7.0, "damage_mult": 0.8, "windup": 0.5, "range": 40},
      {"id": "cone",     "cooldown": 6.0, "damage_mult": 1.2, "windup": 0.4, "range": 96},
      {"id": "blink",    "cooldown": 12.0,"damage_mult": 1.0, "windup": 0.0, "range": 150}
    ],
    "combos": [
      {"id": "probe", "commands": ["barrage","cone","normal","normal"], "interval": 0.6, "end_delay": 0.8},
      {"id": "rage",  "commands": ["blink","cone","whirlwind","barrage","summon"], "interval": 0.5, "end_delay": 0.9}
    ],
```
说明：rage 含 `whirlwind` 以保留原 Phase2 旋风斩行为（设计文档 2.3 为骨架，旋风斩是既有能力，不改不丢）。`barrage.range=40` 语义为扇形总角度 40°，`blink.range=150` 为瞬移距离像素，`cone.range=96` 为扇形半径像素。

- [ ] **Step 3: boss_defs.h 加 ComboDef + BossDef::combos**

```cpp
struct ComboDef {
    std::string id;
    std::vector<std::string> commands;  // normal|charge|shockwave|summon|defend|barrage|cone|blink|whirlwind
    float interval = 0.6f;
    float end_delay = 0.8f;
};
```
（在 BossDef 定义前加；`BossDef` 成员区加 `std::vector<ComboDef> combos;`）

- [ ] **Step 4: boss_defs.cpp `_parse_boss` 加 combos 解析（紧跟 skills 解析块后）**

```cpp
    // ── M4a: 连招模板 (可选) ──
    if (j.contains("combos") && j["combos"].is_array()) {
        for (auto& c : j["combos"]) {
            ComboDef cd;
            cd.id       = c.value("id", "");
            cd.interval = c.value("interval", 0.6f);
            cd.end_delay = c.value("end_delay", 0.8f);
            if (c.contains("commands") && c["commands"].is_array())
                for (auto& cmd : c["commands"])
                    cd.commands.push_back(cmd.get<std::string>());
            if (!cd.id.empty()) def.combos.push_back(cd);
        }
    }
```

- [ ] **Step 5: 验证 + 提交**

Run: `conda run python tools/world_validator.py` → 无新增 ERROR。
Run: `cmake --build --preset release` → 0 error（解析器未接线，仅编译通过即可）。
```bash
git add resources/buffs.json resources/bosses.json src/data/boss_defs.h src/data/boss_defs.cpp
git commit -m "feat: M4a 数据层 — combo 模板与弹幕/扇形/瞬移技能配置 + 解析"
```

### Task 2: 类型层 — BossCommand 扩展 + BossAI 声明

**Files:**
- Modify: `src/game/types/boss_types.h:56`（BossCommand 枚举）
- Modify: `src/game/entities/boss.h`（BossSkill 子类声明 + BossAI 成员/方法声明）

**Interfaces:**
- Consumes: Task 1 的 `ComboDef`、现有 `BossSkillQueue`（boss_types.h:63）
- Produces: `enum class BossCommand { NONE, NORMAL, MOVE, CHARGE, SHOCKWAVE, SUMMON, DEFEND, RETREAT, CAST, PHASE, LAST_STAND, RANGED, CONE, BLINK, WHIRLWIND }`；类 `BarrageSkill/ConeAttackSkill/BlinkSkill`；BossAI 新成员 `_combo_queue/_combo_timer/_combo_end_delay/_combo_current_end_delay/_combo_id/_combos/_barrage/_cone/_blink`；新私有方法 `_tick_combo_attack/_run_combo_command/_select_combo/_combo_advance/_combo_on_skill_end/_command_from_str`

- [ ] **Step 1: boss_types.h 扩展枚举（追加 5 项）**

```cpp
enum class BossCommand { NONE, NORMAL, MOVE, CHARGE, SHOCKWAVE, SUMMON, DEFEND, RETREAT, CAST, PHASE, LAST_STAND, RANGED, CONE, BLINK, WHIRLWIND };
```
用 Grep 检查 `switch (cmd)`/`case BossCommand` 是否全枚举覆盖（boss_decision_to_command 等），无 default 的 switch 若存在需补 default 分支。

- [ ] **Step 2: boss.h 加 3 个技能类声明（WhirlwindSkill 类后）**

```cpp
// M4a: 弹幕 — 蓄力后向玩家扇形发射多颗弹, 命中减速
class BarrageSkill : public BossSkill {
public:
    BarrageSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    float windup_left = 0.0f;
    float windup_time = 0.5f;
    int   shot_count = 4;
    float spread_deg = 40.0f;   // 扇形总角度 (度)
    float speed = 220.0f;
    struct Shot { float x = 0, y = 0, vx = 0, vy = 0, life = 0; };
    std::vector<Shot> shots;
    bool fired = false;
    bool finished = false;
};

// M4a: 扇形斩 — 蓄力后向玩家方向扇形挥击, 命中中毒 2s
class ConeAttackSkill : public BossSkill {
public:
    ConeAttackSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    float windup_left = 0.0f;
    float windup_time = 0.4f;
    float half_angle = 45.0f;  // 半角 (度)
    float reach = 96.0f;       // 半径 (像素)
};

// M4a: 瞬移 — 闪烁至玩家侧翼, CD 独立计时
class BlinkSkill : public BossSkill {
public:
    BlinkSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    bool blinked = false;
    float blink_dist = 150.0f;
};
```

- [ ] **Step 3: BossAI 加成员（`_laser` 成员后）与方法声明（private 区）**

```cpp
    // M4a: 连招系统
    std::unique_ptr<BarrageSkill> _barrage;
    std::unique_ptr<ConeAttackSkill> _cone;
    std::unique_ptr<BlinkSkill> _blink;
    BossSkillQueue _combo_queue;
    float _combo_timer = 0.0f;
    float _combo_end_delay = 0.0f;
    float _combo_current_end_delay = 0.8f;
    std::string _combo_id;
    const std::vector<ComboDef>* _combos = nullptr;   // 来自 BossDef (factory 设置)
```
private 区（`_next_cycle_skill` 声明后）：
```cpp
    bool _tick_combo_attack(Monster* self, Player* player, GameMap* map,
                            double dt, double gt, std::vector<Effect>* effects);
    void _run_combo_command(BossCommand cmd, Monster* self, Player* player,
                            double gt, std::vector<Effect>* effects);
    void _select_combo();
    void _combo_advance();
    void _combo_on_skill_end();
    static BossCommand _command_from_str(const std::string& s);
```
boss.h 顶部加两个 include（`#include "ai.h"` 之后）：`#include "types/boss_types.h"`（BossSkillQueue/BossCommand）与 `#include "data/boss_defs.h"`（ComboDef；boss_defs.h 不依赖 boss.h，无循环依赖）。

- [ ] **Step 4: 验证 + 提交**

Run: `cmake --build --preset release` → 0 error（未实现的 execute 先声明不定义可编译？——类内只声明，link 期才需要。Task 2 结束时 link 需要定义！因此 Step 4 构建若做全量 link 会失败。改为：本 Task 验证 = 编译单元通过（构建到 .o 阶段仍会 link……）。方案：Task 2 结束时临时给 3 个 execute 提供空定义（在 Task 3 填充），保持可 link。
```bash
git add src/game/types/boss_types.h src/game/entities/boss.h
git commit -m "feat: M4a 类型层 — BossCommand 扩展 + 连招技能类声明"
```

### Task 3: 技能实现 — Barrage/Cone/Blink + 工厂接线

**Files:**
- Modify: `src/game/entities/boss.cpp`（3 个 execute 实现 + BossAI 构造初始化 + factory skill_overrides 消费）

**Interfaces:**
- Consumes: `ProjectileFactory` 不需要（自驱动弹幕）；`apply_buff(player,"slow"/"poison2s",1)`（combat_system.h:56）、`map->pixel_to_tile/is_walkable`（SummonMinions 同款用法 boss.cpp:110-116）
- Produces: 3 个可执行技能；`_barrage->finished` 语义（true=弹幕全部结算完）

- [ ] **Step 1: BarrageSkill 实现（WhirlwindSkill 实现后追加）**

```cpp
BarrageSkill::BarrageSkill() : BossSkill("弹幕", 7.0f) {
    fx_kind = "cone"; fx_radius = 200; fx_color = {150, 80, 255, 255};
}
std::string BarrageSkill::execute(Monster* boss, Player* player,
    std::vector<Monster*>&, GameMap*, double) {
    if (windup_left > 0) {
        boss->color = Color{180, 60, 60, 255};
        return "";
    }
    if (!fired) {
        float bx = boss->entity.rect.x + boss->entity.rect.width/2;
        float by = boss->entity.rect.y + boss->entity.rect.height/2;
        float tx = player->entity.rect.x + player->entity.rect.width/2;
        float ty = player->entity.rect.y + player->entity.rect.height/2;
        float base = atan2f(ty - by, tx - bx);
        float total = spread_deg * 3.14159f / 180.0f;
        float half = total / 2.0f;
        float step = (shot_count > 1) ? total / (float)(shot_count - 1) : 0.0f;
        for (int i = 0; i < shot_count; i++) {
            float ang = base - half + step * (float)i;
            shots.push_back({bx, by, cosf(ang) * speed, sinf(ang) * speed, 3.5f});
        }
        fired = true;
        return "暗影骑士释放弹幕！";
    }
    for (auto& s : shots) { s.x += s.vx * 0.016f; s.y += s.vy * 0.016f; s.life -= 0.016f; }
    for (auto it = shots.begin(); it != shots.end();) {
        bool dead = it->life <= 0.0f;
        if (!dead && CheckCollisionCircleRec({it->x, it->y}, 8.0f, player->entity.rect)) {
            int dmg = calculate_damage(
                (int)(boss->combat.get_effective_attack() * damage_mult),
                player->combat.get_effective_defense(AttackType::PHYSICAL));
            player->combat.take_damage(dmg);
            apply_buff(player, "slow", 1);
            dead = true;
        }
        if (dead) it = shots.erase(it); else ++it;
    }
    if (fired && shots.empty()) { finished = true; mark_used(GetTime()); }
    return "";
}
```
注意：函数约 40 行（规则 1 边缘），若超限将发射段拆为私有 helper（实施时按行数决定）。

- [ ] **Step 2: ConeAttackSkill 实现（BarrageSkill 后）**

```cpp
ConeAttackSkill::ConeAttackSkill() : BossSkill("扇形斩", 6.0f) {
    fx_kind = "cone"; fx_radius = 96; fx_color = {140, 240, 80, 255};
}
std::string ConeAttackSkill::execute(Monster* boss, Player* player,
    std::vector<Monster*>&, GameMap*, double) {
    if (windup_left > 0) {
        boss->color = Color{160, 220, 90, 255};
        return "";
    }
    float bx = boss->entity.rect.x + boss->entity.rect.width/2;
    float by = boss->entity.rect.y + boss->entity.rect.height/2;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    float dx = px - bx, dy = py - by;
    float dist = sqrtf(dx * dx + dy * dy);
    mark_used(GetTime());
    boss->color = Color{200, 40, 40, 255};
    if (dist > reach) return "扇形斩落空";
    int dmg = calculate_damage(
        (int)(boss->combat.get_effective_attack() * damage_mult),
        player->combat.get_effective_defense(AttackType::PHYSICAL));
    player->combat.take_damage(dmg);
    apply_buff(player, "poison2s", 1);
    return "扇形斩命中！中毒 2 秒";
}
```

- [ ] **Step 3: BlinkSkill 实现（ConeAttackSkill 后）**

```cpp
BlinkSkill::BlinkSkill() : BossSkill("瞬移", 12.0f) {
    fx_kind = "circle"; fx_radius = 70; fx_color = {120, 80, 255, 255};
}
std::string BlinkSkill::execute(Monster* boss, Player* player,
    std::vector<Monster*>&, GameMap* map, double) {
    if (blinked) return "";
    float bx = boss->entity.rect.x + boss->entity.rect.width/2;
    float by = boss->entity.rect.y + boss->entity.rect.height/2;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    float dx = px - bx, dy = py - by;
    float len = sqrtf(dx * dx + dy * dy);
    if (len > 1.0f) {
        float nx = -dy / len, ny = dx / len;
        for (int side = 0; side < 2 && !blinked; side++) {
            float s = (side == 0) ? 1.0f : -1.0f;
            float tx = px + nx * s * blink_dist;
            float ty = py + ny * s * blink_dist;
            auto [tile_x, tile_y] = map->pixel_to_tile(tx, ty);
            if (map->is_walkable(tile_x, tile_y)) {
                boss->entity.position = {tx, ty};
                boss->entity.sync_rect();
                blinked = true;
            }
        }
    }
    if (!blinked) blinked = true;   // 无可行点则原地 (仍结算)
    mark_used(GetTime());
    return "暗影骑士瞬移了！";
}
```

- [ ] **Step 4: BossAI 构造初始化 + factory skill_overrides 消费**

构造（`_laser` 初始化后）：
```cpp
    _barrage = std::make_unique<BarrageSkill>();
    _cone    = std::make_unique<ConeAttackSkill>();
    _blink   = std::make_unique<BlinkSkill>();
```
factory（boss_factory_create 的 skill_overrides 循环 `_summon` 分支后加 3 分支）：
```cpp
        } else if (sk.id == "barrage") {
            ai->_barrage->cooldown    = sk.cooldown;
            ai->_barrage->damage_mult = sk.damage_mult;
            ai->_barrage->windup_time = sk.windup;
            ai->_barrage->spread_deg  = sk.range;
        } else if (sk.id == "cone") {
            ai->_cone->cooldown    = sk.cooldown;
            ai->_cone->damage_mult = sk.damage_mult;
            ai->_cone->windup_time = sk.windup;
            ai->_cone->reach       = sk.range;
        } else if (sk.id == "blink") {
            ai->_blink->cooldown  = sk.cooldown;
            ai->_blink->blink_dist = sk.range;
        }
```
factory 里 `ai->_combos = &def->combos;`（`ai->_arena_cfg` 赋值后）。

- [ ] **Step 5: 验证 + 提交**

Run: `cmake --build --preset release` → 0 error（新技能未接入状态机但已实例化，可编译）。
```bash
git add src/game/entities/boss.cpp
git commit -m "feat: M4a 技能实现 — 弹幕/扇形斩/瞬移 + 工厂接线"
```

### Task 4: 状态机集成 — combo 驱动 ATTACK + 3 新状态

**Files:**
- Modify: `src/game/entities/boss.cpp:252-527`（_tick_boss_state 重构）

**Interfaces:**
- Consumes: Task 2/3 全部产物
- Produces: 连招驱动行为；其余 Boss（无 combos）行为零变化

- [ ] **Step 1: 实现 6 个 combo helper（BossAI::_next_cycle_skill 后）**

```cpp
BossCommand BossAI::_command_from_str(const std::string& s) {
    if (s == "normal")    return BossCommand::NORMAL;
    if (s == "charge")    return BossCommand::CHARGE;
    if (s == "shockwave") return BossCommand::SHOCKWAVE;
    if (s == "summon")    return BossCommand::SUMMON;
    if (s == "defend")    return BossCommand::DEFEND;
    if (s == "barrage")   return BossCommand::RANGED;
    if (s == "cone")      return BossCommand::CONE;
    if (s == "blink")     return BossCommand::BLINK;
    if (s == "whirlwind") return BossCommand::WHIRLWIND;
    return BossCommand::NONE;
}

void BossAI::_select_combo() {
    if (!_combos || _combos->empty()) return;
    const ComboDef* target = nullptr;
    for (auto& c : *_combos) {
        if ((phase2 && c.id == "rage") || (!phase2 && c.id == "probe")) { target = &c; break; }
    }
    if (!target) target = &(*_combos)[0];
    _combo_id = target->id;
    _combo_current_end_delay = target->end_delay;
    _combo_queue.clear();
    for (auto& cmd : target->commands)
        _combo_queue.enqueue(_command_from_str(cmd));
    _combo_queue.start();
    _combo_timer = 0.0f;
    normal_attack_count = 0;
}

void BossAI::_combo_advance() {
    _combo_queue.advance();
    if (_combo_queue.active) {
        _combo_timer = 0.6f;
        if (_combos) for (auto& c : *_combos)
            if (c.id == _combo_id) { _combo_timer = c.interval; break; }
    } else {
        _combo_end_delay = _combo_current_end_delay;
    }
    normal_attack_count = 0;
}

void BossAI::_combo_on_skill_end() {
    if (_combo_queue.active) _combo_advance();
    else normal_attack_count = 0;
}

bool BossAI::_tick_combo_attack(Monster* self, Player* player, GameMap* map,
                                double dt, double gt, std::vector<Effect>* effects) {
    (void)map;
    if (_combo_queue.active) {
        if (_combo_timer > 0) { _combo_timer -= (float)dt; return true; }
        _run_combo_command(_combo_queue.current_cmd(), self, player, gt, effects);
        return true;
    }
    if (_combo_end_delay > 0) { _combo_end_delay -= (float)dt; return true; }
    if (normal_attack_count >= 2) { _select_combo(); return true; }
    return false;
}

void BossAI::_run_combo_command(BossCommand cmd, Monster* self, Player* player,
                                double gt, std::vector<Effect>* effects) {
    switch (cmd) {
    case BossCommand::NORMAL:
        if (self->can_attack(gt)) {
            self->attack_target(player, gt);
            _spawn_boss_vfx(self, "charge", effects);
        }
        _combo_advance();
        break;
    case BossCommand::CHARGE:
        boss_state = BossState::CHARGE;
        _charge->windup_left = _charge->windup_time;
        _charge->dash_duration = 0.0f;
        _spawn_boss_vfx(self, "charge", effects);
        break;
    case BossCommand::SHOCKWAVE:
        boss_state = BossState::SHOCKWAVE;
        _shockwave->windup_left = _shockwave->windup_time;
        _spawn_boss_vfx(self, "shockwave", effects);
        break;
    case BossCommand::SUMMON:
        boss_state = BossState::SUMMON;
        _spawn_boss_vfx(self, "summon", effects);
        break;
    case BossCommand::DEFEND:
        boss_state = BossState::DEFEND;
        _spawn_boss_vfx(self, "shockwave", effects);
        break;
    case BossCommand::RANGED:
        boss_state = BossState::RANGED_BARRAGE;
        _barrage->windup_left = _barrage->windup_time;
        _barrage->shots.clear();
        _barrage->fired = false;
        _barrage->finished = false;
        _spawn_boss_vfx(self, "charge", effects);
        break;
    case BossCommand::CONE:
        boss_state = BossState::CONE_ATTACK;
        _cone->windup_left = _cone->windup_time;
        _spawn_boss_vfx(self, "shockwave", effects);
        break;
    case BossCommand::BLINK:
        boss_state = BossState::BLINK;
        _blink->blinked = false;
        _spawn_boss_vfx(self, "summon", effects);
        break;
    case BossCommand::WHIRLWIND:
        boss_state = BossState::WHIRLWIND;
        _whirlwind->spin_duration = 0.0f;
        _spawn_boss_vfx(self, "charge", effects);
        break;
    default:
        _combo_advance();
        break;
    }
}
```

- [ ] **Step 2: BossState 枚举加 3 个状态（boss.h:16-26）**

```cpp
    RANGED_BARRAGE,// M4a: 弹幕 (蓄力→扇形弹)
    CONE_ATTACK,   // M4a: 扇形斩 (蓄力→扇形挥击)
    BLINK,         // M4a: 瞬移 (闪烁至侧翼)
```

- [ ] **Step 3: ATTACK 分支接入 combo（`normal_attack_count >= 2` 判断改为双路径）**

在 `case BossState::ATTACK:` 内，距离检查（IDLE 切换）之后、普攻块之前插入 combo 分支；普攻块与旧循环整体作为"非 combo 驱动"的兜底路径：
```cpp
        // M4a: 连招驱动 (优先于普攻/旧循环)
        if (_combos && !_combos->empty()) {
            if (_tick_combo_attack(self, player, map, dt, gt, effects)) break;
        }
```
`_tick_combo_attack` 返回 true = 连招占用本帧（跳过顶部普攻块与旧 `_next_cycle_skill` 路径，避免与 NORMAL 命令重复普攻）；返回 false = 无 combo 配置或空闲期，走原有普攻 + 旧循环代码（原封不动，含 G5.4 phase2 注入）。

- [ ] **Step 4: 新 3 状态实现（`case BossState::LASER_BARRAGE:` 分支后追加）**

```cpp
    case BossState::RANGED_BARRAGE: {
        if (_barrage->windup_left > 0) {
            _barrage->windup_left -= (float)dt;
            if (effects) {
                Effect warn;
                warn.kind = "pulse";
                warn.world_x = self->entity.rect.x + self->entity.rect.width/2;
                warn.world_y = self->entity.rect.y + self->entity.rect.height/2;
                warn.radius = 40; warn.duration = 0.12f; warn.elapsed = 0;
                warn.color = {150, 80, 255, 160};
                effects->push_back(warn);
            }
            break;
        }
        _barrage->execute(self, player, _empty_monsters, map, gt);
        if (_barrage->finished) {
            boss_state = BossState::ATTACK;
            _combo_on_skill_end();
        }
        break;
    }
    case BossState::CONE_ATTACK: {
        if (_cone->windup_left > 0) {
            _cone->windup_left -= (float)dt;
            if (effects) {
                Effect warn;
                warn.kind = "pulse";
                warn.world_x = self->entity.rect.x + self->entity.rect.width/2;
                warn.world_y = self->entity.rect.y + self->entity.rect.height/2;
                warn.radius = _cone->reach; warn.duration = 0.14f; warn.elapsed = 0;
                warn.color = {140, 240, 80, 140};
                effects->push_back(warn);
            }
            break;
        }
        _cone->execute(self, player, _empty_monsters, map, gt);
        boss_state = BossState::ATTACK;
        _combo_on_skill_end();
        break;
    }
    case BossState::BLINK: {
        _blink->execute(self, player, _empty_monsters, map, gt);
        if (_blink->blinked) {
            boss_state = BossState::ATTACK;
            _combo_on_skill_end();
        }
        break;
    }
```

- [ ] **Step 5: 全部技能状态结束点替换为 `_combo_on_skill_end()`**

将 `_tick_boss_state` 中所有 `boss_state = BossState::ATTACK;\n        normal_attack_count = 0;`（CHARGE×4、SHOCKWAVE×2、SUMMON、DEFEND、WHIRLWIND、LASER、GRAVITY 共 ~11 处）第二行替换为 `_combo_on_skill_end();`（保持第一行 `boss_state = BossState::ATTACK;`）。GRAVITY_PULL 内 `_shockwave->execute(...)` 后已是 mark_used，保持。

- [ ] **Step 6: 验证 + 提交**

Run: `cmake --build --preset release` → 0 error。
Run: `build/roguelike_cpp.exe`（冒烟 10s，主菜单正常）。
```bash
git add src/game/entities/boss.cpp src/game/entities/boss.h
git commit -m "feat: M4a 状态机 — 连招驱动暗影骑士技能循环"
```

### Task 5: 验证 + 桌面同步 + 收尾

**Files:**
- 无新代码；仅验证与同步

- [ ] **Step 1: 全量验证**

Run: `conda run python tools/world_validator.py` → 0 ERROR。
Run: `cmake --build --preset release` → 0 error。
Run: `build/roguelike_cpp.exe` 冒烟 10 秒 → 正常启动（F5 暗影骑士实机出招验证由用户完成）。
Run: `git status` → 工作区干净。

- [ ] **Step 2: 同步桌面打包版（CLAUDE.md 第 11 条）**

按既有流程：robocopy 镜像 `src/ resources/ tools/ tests/ docs/ assets/ .github/ vendor/` + 根文件（CMakeLists.txt README.md CLAUDE.md CMakePresets.json .gitignore），重编译 exe 复制到 `C:\Users\HP\Desktop\Roguelike-CPP版`（保留 python_edition/saves，补 3 个 MinGW DLL）。

- [ ] **Step 3: 确认 git log**

```bash
git log --oneline -6
```
期望 4 个 M4a 提交（数据层/类型层/技能/状态机）。

## Self-Review 记录

- **Spec 覆盖**：设计文档 2.1（弹幕+减速✓、扇形+毒2s✓、瞬移✓）2.3（probe/rage 连招✓、interval/end_delay✓、收招硬直=end_delay✓）3.1 全部✓；Phase2 旋风斩保留（rage 含 whirlwind）✓；非暗影骑士 Boss 零回归（combo 路径守卫）✓
- **占位符扫描**：无 TBD/“待定”内容
- **类型一致性**：`ComboDef`（Task1）→ `_combos` 指针（Task2 声明、Task4 用）；`BossCommand::RANGED/CONE/BLINK/WHIRLWIND/NORMAL` 在 Task2 枚举、Task4 `_command_from_str`/`_run_combo_command` 一致；`_combo_on_skill_end` 在 Task2 声明、Task4 实现与埋点一致；`BarrageSkill::finished` 语义 Task3 设置/Task4 读取一致
- **已知取舍**：扇形斩以 boss→玩家连线为轴（boss 无朝向概念），可读招点=蓄力脉冲+距离控制；弹幕无预警圈（飞行物可见可躲，复用 Whirlwind 每帧 tick 模式）；`barrage.range` 字段复用为扇形角度（40°），JSON 注释说明
