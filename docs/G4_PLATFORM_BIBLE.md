# G4 Platform Bible v1.0

> 版本: v0.8.0 → v1.0.0 之间的平台化阶段
> 日期: 2026-07-17
> 隶属于: ARCHITECTURE.md §Development Bible

---

## Rule 1: Engine 不知道 Mod

```
Engine 只知道 Registry
Registry 不知道来源
来源可以是: resources/ / mods/ / dlc/ / plugin/
任何来源都一样
```

### 禁止

```cpp
// ❌ 永远不要这样做
if (is_mod) { load_mod_items(); }
else { load_builtin_items(); }
```

### 正确

```cpp
// ✅ RegistryBuilder 合并多个 Provider
RegistryBuilder builder;
builder.add_provider(std::make_unique<DefaultProvider>());
builder.add_provider(std::make_unique<ModsProvider>());
builder.add_provider(std::make_unique<DLCProvider>());
builder.build();  // → 统一 Registry
```

---

## Rule 2: Provider Pattern

```
DefaultProvider      BuiltinProvider      ModsProvider
       │                    │                  │
       └────────────────────┼──────────────────┘
                            │
                     RegistryBuilder
                            │
                    Priority Merge
                            │
                      Registry
```

### Provider 接口 (G4 暂不实现 — 先定义接口)

```cpp
class IRegistryProvider {
public:
    virtual const char* name() const = 0;
    virtual int priority() const = 0;   // 数字越低优先级越高 (0=内置)
    virtual bool provides(const std::string& module) const = 0;
    virtual std::string load_json(const std::string& module) const = 0;
};
```

### Priority Merge 规则

| 场景 | 行为 |
|------|------|
| 低优先级 key 已存在 | 高优先级覆盖 |
| 高优先级 key 不存在 | 保留低优先级 |
| 高优先级新增 key | 追加到 registry |
| 高优先级设置为 `null` | 删除该 key (Mod 移除某个敌人) |

---

## Rule 3: Mod Structure

```
mods/
  vanilla/              ← 内置数据 (优先级 0)
    mod.json
    enemies.json
    bosses.json
    ...
  better_items/         ← 物品增强 Mod (优先级 10)
    mod.json
    items.json
  hard_mode/            ← 难度提升 Mod (优先级 15)
    mod.json
    enemies.json
    bosses.json
    floor_config.json
  my_boss/              ← 自定义 Boss Mod (优先级 20)
    mod.json
    bosses.json
    dialogues.json
    endings.json
```

### mod.json format

```json
{
  "name": "better_items",
  "version": "1.0.0",
  "priority": 10,
  "description": "Enhanced item stats and new rarities",
  "author": "...",
  "requires": ["vanilla"],
  "conflicts": ["hard_mode"],
  "replaces": ["items.json"]
}
```

---

## Rule 4: Resource Pack 独立

```
boss.json
  ↓
visual_id: "shadow_knight"

ResourceManager
  ↓
Default Pack:  assets/default/shadow_knight.png
HD Pack:       assets/hd/shadow_knight.png
Pixel Pack:    assets/pixel/shadow_knight.png
Halloween Pack: assets/halloween/shadow_knight.png
```

### BossDef 不变

```json
{
  "visual_id": "shadow_knight"
  // 不需要 "texture": "assets/abc.png"
  // ResourceManager 负责 visual_id → file 映射
}
```

---

## Rule 5: Plugin — 先接口, 后实现

```cpp
// G4 定义接口 (稳定), G4.3+ 实现动态加载
class IModPlugin {
public:
    virtual ~IModPlugin() = default;
    virtual const char* name() const = 0;
    virtual void on_load() = 0;
    virtual void on_unload() = 0;
    virtual void on_event(const GameEvent& ev) = 0;
    virtual void on_registry_ready() = 0;
};
```

G4 阶段只需要定义 `IModPlugin` 接口。
DLL/SO/Lua/Python 实现在 G5+ 做。

---

## G4 Milestones (Revised)

| Step | 内容 | 优先级 |
|------|------|--------|
| G4.1 | Mod Loader: mod.json + priority merge + override | P0 |
| G4.2 | Resource Pack: visual_id → file mapping | P1 |
| G4.3 | IModPlugin interface definition | P2 |
| G4.4 | Registry Validator: automated checks | P2 |
| G4.5 | Replay Regression: input log → hash compare | P3 |

---

## G5 Milestones

| Step | 内容 | 优先级 |
|------|------|--------|
| G5.1 | 10000-run automated simulation | P0 |
| G5.2 | Balance data collection (boss win rate, skill usage) | P0 |
| G5.3 | Performance profiling & memory audit | P1 |
| G5.4 | Plugin implementation (DLL/Lua) | P2 |
| G5.5 | Package & deploy | P2 |

---

## v1.0.0 Release Standard

```
不是功能足够多 → v1.0.0
而是:

✅ API Stable       (接口冻结 2+ versions)
✅ Save Stable      (存档兼容 3+ 版本)
✅ Mod Stable       (Mod 接口不破坏)
✅ Regression Stable(回放测试通过)
✅ Performance Stable(基准性能达标)
```

### 永远可以继续增加的内容

```
Boss 数量
地图层数
技能种类
物品 list
对话数量
结局分支
```

这些属于 **Content**，不是 **Version**。

---

> 本 Bible 是 G4 平台化的最高规范。
> G4 所有开发必须遵守 Rule 1-5。
> 任何违反 Provider Pattern 或直接引用 Mod 路径的代码不被接受。
