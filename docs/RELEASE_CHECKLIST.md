# v1.5.0 Demo Release Checklist — 发布门禁

> 依据: V1_4_MILESTONE_PLANNING_REVIEW.md P4 章 + M1-M5 批次完成后增量项
> 用法: 每项标 `[ ]` 未验 / `[x]` 通过 / `[~]` 有条件通过(注明)。全部 P0 项过关才能打 tag。

## P0 · 阻塞项（必须全过）

### 1. 可玩性闭环

- [ ] 新档从标题 → 教程(11步) → F1 首战 → 死亡/重开，全程无卡死（实机 30 分钟路径）
- [ ] 三存档槽：新建×3 / 切换 / 满档删除流（二次确认）/ 删档不丢结局收集（Meta 独立性）
- [ ] 旧档迁移：save.json → slot_1.json 自动迁移 + .bak 保留（v1.4.0 前存档场景）
- [ ] 选关解锁：best_floor 推进，F5 击杀后 F6+ 可选
- [ ] Boss 全链：F5（三选一随机）/ F10（领域机制）/ F15（镜像）可正常触发、战斗、掉结算

### 2. 稳定性

- [ ] 30 分钟实机无崩溃（crash.log 不新增）
- [ ] 60/60 ctest 全绿
- [ ] world_validator.py 0 errors 0 warnings
- [ ] 冒烟 20 局：无 NaN/超长局/异常胜率跳变

### 3. 发布物完整

- [x] 发布包自包含：exe + `raylib.dll`（UCRT 为 Win10+ 系统自带）— 干净机器双击即玩；验证法 `objdump -p build\roguelike_cpp.exe | grep "DLL Name"` 应只有 raylib.dll + 系统库
- [ ] GitHub Release zip：exe + raylib.dll + assets/ + resources/ + README.md，命名 `roguelike-cpp-v1.5.0.zip`（v1.0.0 包结构参照 `dist/roguelike-cpp-v1.0.0/`）
- [x] README 陌生化：新玩家 30 秒内明白"这是什么游戏、怎么开始玩"（v1.4.14 重写，130 行玩家优先版）
- [ ] 至少 1 张游戏截图（标题/战斗/Boss 三选一）— 占位已设
- [x] 桌面打包版同步（exe 在根目录）+ raylib.dll 补齐（本次门禁发现并修复：桌面包此前缺 raylib.dll，靠系统 PATH 兜底才没炸）

### 4. 已知问题披露

- [ ] Current Limitations 节与实际一致（不夸大不隐瞒）
- [ ] P1-C8 间歇非确定性（12 连跑 3 分岔，F3 毒 tick）是否披露 → 已记入 LIMITATIONS

## P1 · 应过关（允许 [~] 注明）

- [ ] 前 30 分钟视觉一致性：三群系贴图差异可感知、无"全 orc"楼层
- [ ] 教程物品渲染与主游戏一致（G10.8 已修）
- [ ] 首遇提示全链（封门教学/首个圣物/满档删除确认）
- [ ] 音频无爆音/静音开关可发现
- [ ] 窗口缩放 + letterbox 正常（3:2）
- [ ] 键位表与实际一致（README 操作节 vs input_map.cpp）

## P2 · 可延后

- [ ] GIF/Trailer（录制清单见 §附录，v1.5.x 补）
- [ ] macOS/Linux 实机验证
- [ ] 手柄输入

## 验收记录

| 日期 | 验收人 | 项目 | 结果 |
|------|--------|------|------|
| | | | |

---

## 附录 A · GIF 录制清单（P2，v1.5.x 补）

工具：ScreenToGif（轻量）或 OBS（高清转 GIF）。录制区 960×640（游戏原生分辨率）。

| # | 场景 | 内容要点 | 建议时长 | 截取时机 |
|---|------|----------|----------|----------|
| 1 | 标题 → 开局 | 海报主菜单 + 新游戏 + 元素三选一 | 8s | 展示海报构图（A− 级屏幕） |
| 2 | F1 战斗 | 拾取武器 → 三连击杀史莱姆 → 打击感三件套（震屏/飘字/顿帧） | 10s | 有武器掉落的开局 |
| 3 | 门交互 | E 开门 → 进有怪房封门 → 清房开门 | 8s | Room Encounter 闭环 |
| 4 | F5 Boss 登场 | intro 立绘 + 弹幕战斗一回合 | 10s | 暗影骑士连招（视觉最丰富） |
| 5 | F15 镜像 | 镜像分析面板 + 它复制你的技能 | 10s | 招牌玩法，放 README 首图位 |

技巧：`--sim-seed` 固定种子可复现同一地图；Boss 层用选关直达 F5。

## 附录 B · 发布物构建步骤

```powershell
# 1. 干净构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cd build; ctest -C Release          # 期望 60/60

# 2. 数据校验
python tools/world_validator.py      # 期望 0 errors

# 3. 打包 (根目录执行, zip 内保持 exe 在顶层)
# 包含: roguelike_cpp.exe + assets/ + resources/ + mods/(可选) + README.md
```
