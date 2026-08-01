---
name: roguelike-dev
description: "Roguelike C++ project development workflow — pre-commit validation, code review checklist, JSON consistency checks, build verification"
---

# Roguelike 开发工作流

## 适用场景

当在 `roguelike_cpp` 项目中执行以下任务时使用此 Skill：
- 修改 C++ 源代码后准备提交
- 修改 JSON 配置文件（`resources/*.json`）
- 新增游戏内容（物品/技能/boss/敌人/圣物/事件）
- Code Review 或重构
- 排查编译/运行时错误

## 修改代码后的强制检查清单

每次修改代码后，按以下顺序验证：

### 1. JSON 一致性检查

```bash
python tools/world_validator.py
```

期望输出：`0 errors, 0 warnings`

如果修改了 JSON，必须跑。不通过不能提交。

### 2. 编译验证

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

必须 0 errors。Warnings 需要逐个检查是否符合预期。

### 3. 编码规范自查

- [ ] 所有函数 ≤ 40 行（用 `wc -l` 或直接数）
- [ ] 所有 `.h` 有 `#pragma once`
- [ ] 没有裸 `new`/`delete`（用 `grep` 确认）
- [ ] 没有 `.h` 中的 `using namespace std`
- [ ] JSON 字段名全部 `snake_case`
- [ ] 类名 `PascalCase`，方法 `camelCase`，变量 `snake_case`

### 4. Code Review 要点

- 每个类只负责一件事（SRP）
- 组合优于继承
- 变量命名语义化（不是 `tmp`、`data`、`obj` 这种）
- 输出参数检查 nullptr（Registry 查找）
- `std::optional` 返回值检查 `std::nullopt`
- 跨模块改动需先读 `docs/ARCHITECTURE.md` 确认影响范围

### 5. 新增游戏内容的额外检查

- 新 ID 在 JSON 中唯一
- 所有引用 ID（biome_id、boss_id、skill_id 等）指向存在的目标
- 中文文本使用 `GuiFont::DrawTextCH()`，不是 `DrawText()`
- 新增中文后跑 `python tools/extract_chars.py` 更新字体码点

## 修改 JSON 配置的特殊规则

修改 `resources/` 下的任何 JSON 后：

1. 先读懂 World Validator 的 4 道检查流程（见 `tools/world_validator.py` 源码头部注释）
2. 修改后立即运行 Validator
3. 修复所有 ERROR（WARNING 需要确认是否可接受）
4. 如果新增了 ID，确保所有引用该 ID 的地方都已更新

## Git 提交规范

```
commit message 格式: <模块>: <简短描述>

例如:
  combat: 修复暴击伤害计算溢出
  world: 新增火山 biome 的遭遇事件
  data: 更新对话 JSON 的 next 链接
```

提交前让 AI 分析 diff 并生成 commit message（参考 `git diff --cached` 的输出）。

## 常见问题速查

| 症状 | 可能原因 | 检查方法 |
| :--- | :--- | :--- |
| 中文乱码/方块 | `DrawText` 不支持中文 | 搜索 `DrawText(` 是否用了中文 |
| 技能不触发 | JSON 引用ID 不存在 | World Validator |
| 怪物不生成 | biome enemy_pools 未配置 | `resources/biomes.json` |
| 编译 UTF-8 错误 | MinGW 缺标志 | `-finput-charset=UTF-8 -fexec-charset=UTF-8` |
| 存档损坏 | save.json 格式不符合 schema | 对比 `src/game/save/` 加载代码 |

## 限制

- 仅适用于 `roguelike_cpp` 项目
- 不替代人工测试（需要实际运行游戏验证手感）
- 遇到不确定的规则冲突时，优先遵守 CLAUDE.md 中列出的顺序（编号小的 > 编号大的）
