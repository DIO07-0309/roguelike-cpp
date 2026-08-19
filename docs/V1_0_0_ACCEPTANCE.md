# v1.0.0 Release Standard — 五项 Stable 验收报告

> 日期: 2026-08-19 | 版本: v0.9.32 (验收时基线)
> 依据: docs/G4_PLATFORM_BIBLE.md §v1.0.0 Release Standard
> 原则: "不是功能足够多 → v1.0.0, 而是五项 Stable 全部达标"

## ✅ API Stable — 接口冻结 2+ 版本

对外契约 (Mod/存档/回放边界), 自 G4 阶段 (v0.9.0 前后) 冻结至今, 2+ 版本未破坏:

| 契约 | 内容 | 冻结版本 |
| :--- | :--- | :--- |
| 存档格式 | `v:3` 头 + `key:value` 行 (v2 增 atl/rul, v3 增 qst/end), 向后兼容 v1 | G1/G2 里程碑起 |
| Registry | `MergeMode {Skip,Replace,MergePatch}` + `ModuleData` + `BuildRecord` | G4.1/G4.3 |
| Mod | `mods/` scan → ModProvider → RegistryBuilder merge (priority + override) | G4.1 |
| Replay | hash 链 (compute_state_hash 链式), recorder/replay player | G4.5 |

内部接口 (BossAI/MirrorAgent 等) 持续演进属内容发展, 不构成契约破坏。

## ✅ Save Stable — 存档兼容 3+ 版本

- 格式演进: v1 (核心字段) → v2 (atl/rul) → v3 (qst/end) → 增量 (elem/mra/mrb), 全程向后兼容
- 新增验收测试 (tests/save/save_test.cpp, `SaveStable.*` 3 例):
  - `V1LegacyLoadsWithDefaults`: v1 旧档加载, v2/v3/增量字段取默认值
  - `CorruptLinesTolerated`: 未知技能/坏 buff/未知行跳过不崩溃
  - `FullRoundtrip`: save_game → load_save 全字段逐项一致 (技能/装备/物品/元素/任务/结局/镜像记忆)
- **验收发现并修复真实 bug**: `elem` 字段写元素名字 ("fire"), 读端 `atoi("fire")=0` → 元素类型读档永久丢失。已改为写 int (M4b-fix), 修复后新增测试验证 FIRE 完整往返。

## ✅ Mod Stable — Mod 接口不破坏

- `mod_mgr.scan("mods")` + `ModProvider` + `DependencyResolver` (mod_dependency.cpp) 全链路在
- Merge 语义稳定: Skip/Replace/MergePatch (`__patch` 字段级合并)
- registry_test 覆盖 6 类资源 (buff/relic/enemy/boss/skill/item) + 世界层 (biome/landmark/hazard/encounter) 加载 + 引用完整性 (15 层映射等)
- Mod 目录缺失/空 → 正常降级 (boot 全流程含 sim 模式实测)

## ✅ Regression Stable — 回放测试通过

- **Q3.14 确定性对拍**: 3 种子 × 20 局 × 2 批逐字节一致 (130 万行级), 修复后无分叉
- **replay 机制**: recorder 记录 hash 链 + replay player 比对 (`compute_state_hash` 链式), G4.5 契约在
- **平衡回归**: 500 局 (5 seeds × 100) 连续评估 v0.9.31: 7.0%, v0.9.32 验收时: 8.0% — 目标区间 6-10% 稳定
- 37 个 gtest 全绿 (34 ctest 条目)

## ✅ Performance Stable — 基准性能达标

| 基准 | 结果 | 判定 |
| :--- | :--- | :--- |
| sim 500 局 (5 seeds × 100, 5 进程并行) | 53 s, 0 超时 0 崩溃 | ✅ |
| 单核吞吐 | ~9.4 局/s | ✅ 可支撑 10000-run 大规模评估 (G5.1) |
| 全量测试套件 | 0.46 s | ✅ |

## 结论

**五项 Stable 全部达标**, v1.0.0 Release Standard 满足。

内容层 (Boss 数量/层数/技能/物品/对话/结局) 永可继续增加, 不影响版本冻结。