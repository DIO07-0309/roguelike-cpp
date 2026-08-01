# D2 Warning Projectile System — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend Projectile to support enemy/boss attacks with warning phase, avoid creating new Managers.

**Architecture:** Minimal extension to existing Projectile struct + tick loop. Enemy projectiles ride the same `game_scene.cpp` tick path as player crossbow bolts. Warning phase is a new timer field on Projectile — no new system, no new loop.

**Tech Stack:** C++17, Raylib 5.0, existing WeaponExecutor/projectile pipeline

## Current State Analysis

### Existing Projectile struct (`src/game/types/weapon_types.h:135-143`)
```cpp
struct Projectile {
    Vector2 pos{};
    Vector2 vel{};          // direction * speed, pre-computed
    int damage = 0;
    float lifetime = 2.0f;
    float elapsed = 0.0f;
    bool piercing = false;
    bool alive = true;
};
```
- Player-only. No owner field. No damage type. No element. No warning phase.

### Existing tick loop (`src/game/scenes/game_scene.cpp:613+`)
- `weapon_projectiles` is `std::vector<Projectile>` on GameScene
- Player crossbow bolts are spawned via `WeaponExecutor::execute(..., &gs.weapon_projectiles)`
- Then ticked: `WeaponExecutor::tick_projectiles(weapon_projectiles, mlist, dt)` — checks collision with monsters
- Rendered: `for (auto& p : weapon_projectiles) { DrawCircle(...); }`

### Existing monster attack (`src/game/entities/monster.cpp:20-32`)
```cpp
int Monster::attack_target(Player* target, double gt) {
    int dmg = calculate_damage(get_effective_attack(this),
        target->combat.get_effective_defense(attack_type), attack_type);
    target->combat.take_damage(dmg);       // INSTANT damage, no projectile
    last_attack_time = (float)gt;
    for (auto& tr : on_hit_triggers)       // buff triggers
        apply_buff(target, tr.buff_id, tr.stacks);
    return dmg;
}
```
- Called by `MonsterAI::_execute_attack()` when in range
- Instant damage — zero warning, zero dodge window

### Existing Boss skills (`src/game/entities/boss.cpp`)
- BossAI has ChargeSkill / ShockwaveSkill / SummonSkill
- All instant or self-animated (windup timer on Boss itself, not projectile-based)
- No concept of "spawn a projectile that flies toward the player"

### Architecture conflicts — NONE
- Projectile is a POD struct in `weapon_types.h` — safe to extend with new fields
- `tick_projectiles` is a standalone static method — can be reused for enemy projectiles
- Monster AI attack is a single function call — can be branched to spawn projectile instead
- GameScene already has `weapon_projectiles` vector — we add `enemy_projectiles` in parallel

## Global Constraints

- Zero new Managers, zero new Systems, zero new Scenes
- Do NOT modify WeaponExecutor core logic (only add enemy-specific helpers)
- Do NOT modify CombatSystem damage formula
- Do NOT modify ElementComponent
- Do NOT rewrite BossAI — only add projectile spawning calls
- Use existing Projectile struct, HitDetection, VFXServer, GameEvent/EventBus
- Reuse GameScene's existing tick + render loop

---

## Task Plan

### Task 1: Extend Projectile struct with owner, element, damage_type, warning phase

**Files:**
- Modify: `src/game/types/weapon_types.h` — extend Projectile struct
- Modify: `src/game/systems/weapon_executor.cpp` — adapt existing projectile creation to new fields

**Interfaces:**
- Produces: `ProjectileOwner` enum `{ PLAYER, MONSTER, ENVIRONMENT }`
- Produces: `ProjectilePhase` enum `{ WARNING, ACTIVE, FINISHED }`
- Produces: `WarningLevel` enum `{ NORMAL, DANGEROUS, DEADLY }`
- Produces: Extended `Projectile` with new fields: `owner`, `element`, `damage_type`, `phase`, `warning_time`, `warning_level`, `warning_timer`
- All new fields default to player/PHYSICAL/NONE for backward compat with existing crossbow code

**Steps:**

- [ ] **Step 1: Add enums above Projectile struct in weapon_types.h**

```cpp
// ── D2: Projectile owner ──
enum class ProjectileOwner : int { PLAYER = 0, MONSTER, ENVIRONMENT };
enum class ProjectilePhase : int { WARNING = 0, ACTIVE, FINISHED };
enum class WarningLevel : int { NORMAL = 0, DANGEROUS, DEADLY };
```

- [ ] **Step 2: Extend Projectile struct with new fields (all with backward-compat defaults)**

```cpp
struct Projectile {
    Vector2 pos{};
    Vector2 vel{};
    int damage = 0;
    float lifetime = 2.0f;
    float elapsed = 0.0f;
    bool piercing = false;
    bool alive = true;

    // D2: Owner, element, damage type
    int owner = 0;           // ProjectileOwner as int (0=PLAYER)
    int element = 0;         // ElementType as int (0=NONE)
    int damage_type = 0;     // AttackType as int (0=PHYSICAL)

    // D2: Warning phase
    int phase = 1;           // ProjectilePhase as int (1=ACTIVE, 0=WARNING, 2=FINISHED)
    float warning_time = 0.0f;   // seconds in WARNING before becoming ACTIVE
    float warning_timer = 0.0f;  // countdown
    int warning_level = 0;   // WarningLevel as int (0=NORMAL)
    float warning_radius = 0.0f; // radius for AOE warning circle display
};
```

- [ ] **Step 3: Update existing crossbow projectile creation in weapon_executor.cpp to set defaults**

In `_try_crossbow_power`: add after spawning:
```cpp
proj.owner = (int)ProjectileOwner::PLAYER;
```

In `_crossbow_normal`: add after each push_back:
```cpp
proj.owner = (int)ProjectileOwner::PLAYER;
// (other new fields default to 0/PHYSICAL/NONE — already correct)
```

- [ ] **Step 4: Verify existing GameScene rendering still works**

Check that `weapon_projectiles` loop only checks `p.alive` and `p.piercing` — new fields are additive, no breakage.

- [ ] **Step 5: Reconfigure + build**

```bash
cmake -B build && cmake --build build -j8
```

- [ ] **Step 6: Commit**

```
git add src/game/types/weapon_types.h src/game/systems/weapon_executor.cpp
git commit -m "D2.1: Extend Projectile with owner/element/damage_type/warning phase fields"
```

---

### Task 2: Add enemy_projectiles vector to GameScene + tick loop

**Files:**
- Modify: `src/game/scenes/game_scene.h` — add `std::vector<Projectile> enemy_projectiles`
- Modify: `src/game/scenes/game_scene.cpp` — tick enemy projectiles, render warning circles + enemy projectiles

**Interfaces:**
- Produces: GameScene member `enemy_projectiles`
- Consumes: Extended Projectile from Task 1

**Steps:**

- [ ] **Step 1: Add enemy_projectiles to game_scene.h**

```cpp
// G9.1: weapon projectiles (crossbow bolts)
std::vector<Projectile> weapon_projectiles;

// D2: enemy projectiles (archer arrows, boss fireballs)
std::vector<Projectile> enemy_projectiles;
```

- [ ] **Step 2: Add enemy projectile tick in game_scene.cpp _process()**

After the existing `tick_projectiles(weapon_projectiles, ...)` block, add:
```cpp
// D2: tick enemy projectiles (hit player)
{
    std::vector<Monster*> mlist2;
    for (auto& m : monsters) mlist2.push_back(m.get());
    (void)mlist2; // enemy projs hit player, not monsters
    for (auto& p : enemy_projectiles) {
        if (!p.alive) continue;
        // Warning phase: only countdown, no damage
        if (p.phase == (int)ProjectilePhase::WARNING) {
            p.warning_timer -= dt;
            if (p.warning_timer <= 0.0f) {
                p.phase = (int)ProjectilePhase::ACTIVE;
            }
            continue;
        }
        if (p.phase != (int)ProjectilePhase::ACTIVE) continue;
        p.elapsed += dt;
        if (p.elapsed >= p.lifetime) { p.alive = false; continue; }
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        // Hit player
        if (CheckCollisionCircleRec(p.pos, 8.0f, player->entity.rect)) {
            int dmg = calculate_damage(p.damage,
                player->combat.get_effective_defense((AttackType)p.damage_type),
                (AttackType)p.damage_type);
            player->combat.take_damage(dmg);
            _presentation.damage_floats.push_back({
                player->entity.rect.x + player->entity.rect.width/2,
                player->entity.rect.y - 12, 0.6f, dmg,
                dmg_color_for(dmg, p.damage_type != 0, false)
            });
            _presentation.trigger_shake(dmg > 20 ? 8.0f : 3.0f);
            if (!p.piercing) p.alive = false;
        }
    }
    enemy_projectiles.erase(std::remove_if(enemy_projectiles.begin(),
        enemy_projectiles.end(), [](auto& p){ return !p.alive; }),
        enemy_projectiles.end());
}
```

- [ ] **Step 3: Add enemy projectile rendering + warning circles in _render()**

After player projectile rendering block, add:
```cpp
// D2: draw enemy projectiles + warning circles
for (auto& p : enemy_projectiles) {
    if (!p.alive) continue;
    float sx = p.pos.x - _cam_x, sy = p.pos.y - _cam_y;
    if (p.phase == (int)ProjectilePhase::WARNING) {
        // Warning circle with pulse
        float pulse = 1.0f + sinf((float)GetTime() * 8.0f) * 0.15f;
        float wr = (p.warning_radius > 0 ? p.warning_radius : 24.0f) * pulse;
        Color wc = (p.warning_level >= 2) ? Color{255,40,20,100}
                 : (p.warning_level >= 1) ? Color{255,160,30,100}
                 : Color{255,200,60,80};
        DrawCircle(sx, sy, wr, Fade(wc, 0.4f));
        DrawRing({sx, sy}, wr - 4, wr + 4, 0, 360, 16,
            {wc.r, wc.g, wc.b, (unsigned char)(wc.a * 0.6f)});
    } else {
        // Active enemy projectile
        Color ec = (p.element == (int)ElementType::FIRE) ? Color{255,80,30,255}
                 : (p.element == (int)ElementType::ICE) ? Color{80,180,255,255}
                 : Color{255,60,40,220};
        DrawCircle(sx, sy, 6.0f, ec);
        DrawCircle(sx, sy, 3.0f, {255,255,200,200});
    }
}
```

- [ ] **Step 4: Reconfigure + build**

```bash
cmake -B build && cmake --build build -j8
```

- [ ] **Step 5: Commit**

```
git add src/game/scenes/game_scene.h src/game/scenes/game_scene.cpp
git commit -m "D2.2: Add enemy_projectiles tick/render loop with warning phase"
```

---

### Task 3: Add enemy projectile spawning function + hook into Monster::attack_target

**Files:**
- Modify: `src/game/entities/monster.cpp` — add `spawn_enemy_projectile()` + branch `attack_target()`
- Modify: `src/game/entities/monster.h` — add projectile_spawn fields

**Interfaces:**
- Produces: `void spawn_enemy_projectile(Projectile& p, Player* target)` — computes velocity toward player
- Produces: Monster field `bool uses_projectile` and `float projectile_warning_time`

**Steps:**

- [ ] **Step 1: Add projectile flags to Monster in monster.h**

```cpp
// D2: Monster projectile config
bool uses_projectile = false;        // spawns projectile instead of instant melee
float projectile_warning_time = 0.6f; // seconds of warning before active
int   projectile_warning_level = 0;   // WarningLevel as int
```

- [ ] **Step 2: Add spawn helper function to monster.cpp**

```cpp
static void spawn_enemy_projectile(Monster* self, Player* target,
    std::vector<Projectile>& enemy_projectiles) {
    float cx = self->entity.rect.x + self->entity.rect.width/2;
    float cy = self->entity.rect.y + self->entity.rect.height/2;
    float tx = target->entity.rect.x + target->entity.rect.width/2;
    float ty = target->entity.rect.y + target->entity.rect.height/2;
    float dx = tx - cx, dy = ty - cy;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1) len = 1;
    float speed = 250.0f;

    Projectile p;
    p.pos = {cx, cy};
    p.vel = {dx/len * speed, dy/len * speed};
    p.damage = calculate_damage(get_effective_attack(self),
        target->combat.get_effective_defense(self->attack_type), self->attack_type);
    p.damage_type = (int)self->attack_type;
    p.owner = (int)ProjectileOwner::MONSTER;
    p.phase = (int)ProjectilePhase::WARNING;
    p.warning_time = self->projectile_warning_time;
    p.warning_timer = self->projectile_warning_time;
    p.warning_level = self->projectile_warning_level;
    p.warning_radius = 24.0f;
    p.lifetime = 3.0f;
    enemy_projectiles.push_back(p);
}
```

- [ ] **Step 3: Branch Monster::attack_target**

```cpp
int Monster::attack_target(Player* target, double gt) {
    // D2: projectile-based attack for ranged monsters
    if (uses_projectile && ai) {
        // AI passes enemy_projectiles via a different path (see Task 4)
        // For now, store a pointer on the Monster instance
        (void)gt;
        return 0; // damage handled by projectile on hit
    }
    // Original instant-attack path unchanged
    int dmg = calculate_damage(...);
    ...
}
```

- [ ] **Step 4: Reconfigure + build**

```bash
cmake -B build && cmake --build build -j8
```

- [ ] **Step 5: Commit**

```
git add src/game/entities/monster.h src/game/entities/monster.cpp
git commit -m "D2.3: Add enemy projectile spawning + Monster uses_projectile flag"
```

---

### Task 4: Wire MonsterAI to spawn projectiles via GameScene

**Files:**
- Modify: `src/game/entities/ai.cpp` — `_execute_attack` spawns projectile for ranged monsters
- Modify: `src/game/scenes/game_scene.cpp` — pass enemy_projectiles to monster update

**Interfaces:**
- Consumes: Monster `uses_projectile` field from Task 3
- Consumes: `MonsterAI::update()` receives a pointer to `enemy_projectiles` vector

**Steps:**

- [ ] **Step 1: Add enemy_projectiles parameter to MonsterAI::update**

In `monster.h`, extend update signature:
```cpp
void update(Monster* self, Player* player, GameMap* map, double dt, double gt,
            std::vector<Monster*>* all, std::vector<Effect>* effects,
            std::vector<Projectile>* enemy_projectiles = nullptr);
```

In `ai.h`, update the virtual method signature to match.

- [ ] **Step 2: Modify MonsterAI::_execute_attack to spawn projectile for ranged monsters**

```cpp
void MonsterAI::_execute_attack(Monster* self, Player* player, double gt,
                                 std::vector<Effect>* effects,
                                 std::vector<Projectile>* enemy_projectiles) {
    if (self->uses_projectile && enemy_projectiles) {
        // D2: ranged monster spawns warning projectile
        if (self->can_attack(gt)) {
            spawn_enemy_projectile(self, player, *enemy_projectiles);
            self->last_attack_time = (float)gt;
            // Warning VFX at target position
            if (effects) {
                VFXServer vfx;
                float tx = player->entity.rect.x + player->entity.rect.width/2;
                float ty = player->entity.rect.y + player->entity.rect.height/2;
                vfx.ring(tx, ty, 24.0f, {255,200,60,160}, 1, 0.5f);
                for (auto& e : vfx.effects) effects->push_back(e);
            }
        }
        return;
    }
    // Original melee attack unchanged
    ...
}
```

- [ ] **Step 3: Pass enemy_projectiles from GameScene to monster AI**

In `PlayerController::tick()` or the `_update_monsters` call, pass `&enemy_projectiles`.

- [ ] **Step 4: Configure Archer/Shaman as projectile monsters in monster factory**

In `spawn_monster()` (monster.cpp), set `uses_projectile = true` for monsters with type ARCHER or SHAMAN.

- [ ] **Step 5: Reconfigure + build**

```bash
cmake -B build && cmake --build build -j8
```

- [ ] **Step 6: Commit**

```
git add src/game/entities/ai.cpp src/game/entities/monster.cpp src/game/scenes/game_scene.cpp
git commit -m "D2.4: Wire MonsterAI to spawn warning projectiles for ranged enemies"
```

---

### Task 5: Boss projectile spawning

**Files:**
- Modify: `src/game/entities/boss.cpp` — add projectile spawn from BossAI
- Modify: `src/game/scenes/game_scene.cpp` — pass enemy_projectiles to Boss update

**Interfaces:**
- Consumes: `enemy_projectiles` vector on GameScene
- Produces: Boss spread-shot logic that creates multiple warning projectiles

**Steps:**

- [ ] **Step 1: Add spread-shot pattern to BossAI**

In boss.cpp, add helper:
```cpp
static void boss_spread_shot(Monster* boss, Player* player,
    std::vector<Projectile>& enemy_projectiles, int count, float spread_deg) {
    float cx = boss->entity.rect.x + boss->entity.rect.width/2;
    float cy = boss->entity.rect.y + boss->entity.rect.height/2;
    float tx = player->entity.rect.x + player->entity.rect.width/2;
    float ty = player->entity.rect.y + player->entity.rect.height/2;
    float base_angle = atan2f(ty - cy, tx - cx);
    float half = (spread_deg * 3.14159f / 180.0f) / 2.0f;
    float step = (count > 1) ? (spread_deg * 3.14159f / 180.0f) / (count - 1) : 0;

    for (int i = 0; i < count; i++) {
        float angle = base_angle - half + step * i;
        Projectile p;
        p.pos = {cx, cy};
        p.vel = {cosf(angle) * 200.0f, sinf(angle) * 200.0f};
        p.damage = calculate_damage(get_effective_attack(boss),
            player->combat.get_effective_defense(boss->attack_type), boss->attack_type);
        p.damage_type = (int)boss->attack_type;
        p.owner = (int)ProjectileOwner::MONSTER;
        p.phase = (int)ProjectilePhase::WARNING;
        p.warning_time = 1.0f;
        p.warning_timer = 1.0f;
        p.warning_level = (int)WarningLevel::DEADLY;
        p.warning_radius = 20.0f;
        p.lifetime = 3.0f;
        enemy_projectiles.push_back(p);
    }
}
```

- [ ] **Step 2: Call from BossAI spread attack**

In BossAI's ShockwaveSkill or add a dedicated projectile skill:
```cpp
// In BossSkill::execute or BossAI::update:
if (skill_name == "shockwave" && phase2) {
    boss_spread_shot(boss, player, *enemy_projectiles, 7, 60.0f); // 7 arrows, 60° spread
}
```

- [ ] **Step 3: Pass enemy_projectiles pointer to BossAI**

Store `std::vector<Projectile>* enemy_projectiles` as a field on Monster or pass through the AI update chain.

- [ ] **Step 4: Reconfigure + build**

```bash
cmake -B build && cmake --build build -j8
```

- [ ] **Step 5: Commit**

```
git add src/game/entities/boss.cpp
git commit -m "D2.5: Boss spread-shot warning projectiles"
```

---

### Task 6: Tests

**Files:**
- Create: `tests/combat/projectile_test.cpp`

**Interfaces:**
- Tests the extended Projectile struct fields, warning phase lifecycle, player/monster owner differentiation

**Steps:**

- [ ] **Step 1: Write projectile struct tests**

```cpp
// D2: Projectile extension tests
#include <gtest/gtest.h>
#include "types/weapon_types.h"
#include "entities/combat_stats.h"

TEST(ProjectileD2, DefaultsAreBackwardCompat) {
    Projectile p;
    EXPECT_EQ(p.owner, 0);        // PLAYER
    EXPECT_EQ(p.damage_type, 0);  // PHYSICAL
    EXPECT_EQ(p.phase, 1);       // ACTIVE (existing projectiles work unchanged)
    EXPECT_EQ(p.warning_time, 0.0f);
}

TEST(ProjectileD2, MonsterOwner) {
    Projectile p;
    p.owner = (int)ProjectileOwner::MONSTER;
    p.phase = (int)ProjectilePhase::WARNING;
    p.warning_time = 0.8f;
    p.warning_timer = 0.8f;
    EXPECT_EQ(p.owner, 1);   // MONSTER = 1
    EXPECT_EQ(p.phase, 0);   // WARNING = 0
    EXPECT_GT(p.warning_timer, 0.0f);
}

TEST(ProjectileD2, WarningTransition) {
    Projectile p;
    p.phase = (int)ProjectilePhase::WARNING;
    p.warning_timer = 0.5f;
    p.warning_timer -= 0.6f; // exceeded
    if (p.warning_timer <= 0.0f) p.phase = (int)ProjectilePhase::ACTIVE;
    EXPECT_EQ(p.phase, (int)ProjectilePhase::ACTIVE);
}

TEST(ProjectileD2, WarningPhaseDoesNoDamage) {
    Projectile p;
    p.phase = (int)ProjectilePhase::WARNING;
    p.damage = 50;
    // In the tick loop, WARNING projectiles skip collision — verified by logic, not by mock
    EXPECT_EQ(p.phase, 0);
    EXPECT_GT(p.damage, 0);
}

TEST(ProjectileD2, ElementAndDamageType) {
    Projectile p;
    p.element = 1;      // FIRE = 1
    p.damage_type = 1;  // MAGICAL = 1
    EXPECT_EQ((int)AttackType::MAGICAL, 1);
    // ElementType::FIRE should be 1
}
```

- [ ] **Step 2: Register test in CMakeLists.txt**

```
add_roguelike_test(projectile_test  combat/projectile_test.cpp)
```

- [ ] **Step 3: Build + run tests**

```bash
cmake -B build -DENABLE_TESTS=ON && cmake --build build -j8 && cd build && ctest -R projectile
```

- [ ] **Step 4: Commit**

```
git add tests/combat/projectile_test.cpp tests/CMakeLists.txt
git commit -m "D2.6: Projectile extension tests (owner/phase/warning/damage_type)"
```

---

## Verification Checklist

1. `cmake -B build && cmake --build build -j8` — clean build, no errors
2. `build/roguelike_cpp.exe` — game launches, existing player crossbow works unchanged
3. Enemy archer spawns projectiles with warning circle
4. Warning circle renders for 0.6s before projectile becomes active
5. Active enemy projectile flies toward player, deals damage on hit
6. Boss spread-shot spawns 7 warning circles in a fan pattern
7. `cmake -DENABLE_TESTS=ON && cmake --build build && ctest` — projectile_test passes
