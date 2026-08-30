# G10.2-B3B — Font Selection Review (选型评审, 待 Review Gate)

> 状态: **只评估, 未下载/未提交字体** | 背景: D4 裁决 — 采用可再分发开源中文字体, 消除 `C:/Windows/Fonts` 依赖
> 候选数据来源: 官方仓库/维基 (2026-08 抓取)

## 0. 约束回顾 (裁决 D4 + B3B 要求)

- 开源、可再分发 (满足公开 GitHub 仓库发布)
- 支持简体中文 (游戏 UI 语言)
- 允许入库子集, 但 **Subset 不作当前目标** (等 UI 文本稳定, 防新字缺口)
- Regular + Bold (如 UI 需要) 优先
- 带 LICENSE.txt 一并入库
- Windows/Linux 发布兼容

## 1. 候选评审表

| 字体 | License | 中文覆盖 | 文件大小 | 像素风适配 | 推荐 |
|------|---------|---------|:--------:|:----------:|:----:|
| **A. Fusion Pixel Font** (缝合像素, TakWolf) | **OFL-1.1** (字体) + MIT(工具); 上游全 OFL 兼容 ✓ 商用/再分发自由 | **zh_hans 语言特定版** (简中); 含 GB2312 常用区 (8/10/12px 三档, 等宽/比例两模式) | **~1-5MB 估** (12px zh_hans 比例版 TTF, 需下载确认真值) | **★★★★★ 天生为像素风** (泛中日韩像素字体) | **★ 主选** |
| **B. Source Han Sans SC** (思源黑体/Noto Sans CJK SC) | **OFL-1.1** (v1.002 起; 早 Apache2) ✓ | 全量简中 65535 glyphs | 完整 OTF ~16MB/weight; Subset 后 ~1-2MB/weight | ★★☆ 矢量黑体, 非像素; 但 BILINEAR 下 18/32px 清晰 | 备选 (通用稳) |
| **C. Zpix (最像素)** | **❌ 商业付费 ￥7000/单产品; 禁止修改/反编译/转换/拆分** | 21998 字 (简繁日) | 单 TTF (12px) ~2-4MB | ★★★★★ 最知名像素中文 | **❌ 排除** (许可不满足开源仓库发布) |
| **D. 现状: 系统字体链** (simhei/msyh/simsun) | 微软授权, **不可再分发** | 依赖 Windows 系统安装 | 0 | ★★☆ | **❌ 消除目标** (B3B 目的) |

## 2. 候选 A 深度 (推荐)

- **身份**: "开源的泛中日韩像素字体, 黑体风格" — 由 方舟像素(10/12px) + MisekiBitmap(8px 简中) + Misaki + 精品點陣體 + Cubic11 + Galmuri 缝合
- **许可**: 字体 OFL-1.1 (商用/再分发/修改全自由, 只需保留许可声明) — **与本项目 CC0+OFL 混合现状兼容**
- **版本**: 8/10/12px × 等宽/比例 × 语言子集 (zh_hans 专用版)
  - 推荐: **12px zh_hans 比例模式** (可读性最好; 比例模式中西文混排观感佳)
- **渲染适配注意**: 像素字体必须以 **TEXTURE_FILTER_POINT** 绘制 (现在 ResourceManager 字体用 BILINEAR — 会柔化像素边缘, 破坏像素风)。→ B3B 集成时需将字体 filter 改 POINT (属于"字体接入"范围)
- **字符覆盖风险**: 点阵缝合字体生僻字可能缺失 → 集成验证必须跑"游戏实际文本提取 ↔ glyphCount 核对" (见 §4)
- **Bold**: 官方有"缝合粗像素字体"算法粗体版 (如需 Bold 可复用同源方案)

## 3. 候选 B 深度 (备选)

- 通用黑体, UI 可读性最佳, 覆盖绝对全; 体积大, 且视觉是矢量现代感 (与像素风角色/地形有一定张力, 但 D1-C"混合规则"下 UI 用清晰黑体可接受)
- 若 A 的 glyphCount 验证不达标 → 降级选 B

## 4. 游戏实际文本验证 (集成时执行, 非本阶段)

1. 提取运行需用字符集: 现有 `FONT_CP_DATA` (编译期码位清单, resource_manager.cpp) 全文列出 → 去重的简体中文集合 N
2. 候选字体 `LoadFontEx(path, size, cp[])` 后取 `glyphCount`, 比对 `>= N` (缺失计数 K)
3. K==0 → 选用; K>0 且缺失字在 UI 常用区 → **换候选** (A→B) 或补字 (像素字体补字需上游, 不自行改 → 落入"改动许可"范畴, 直接换)
4. 目视: 标题 32px + 正文 18px 各渲染一张 UI 样本 (含技能/物品/对话文本)

## 5. 集成计划 (B3B 实施阶段, 需你批准后执行)

| 步 | 内容 |
|----|------|
| 1 | 下载候选 A 12px zh_hans 比例版 TTF + License (OFL 全文) → `assets/fonts/game_regular.ttf` + `LICENSE.txt` |
| 2 | manifest `assets.font` 类别: `font.ui_cn` {file, fallback: "system:simhei,msyh,simsun"} |
| 3 | resource_manager 字体链: 候选表首位 = manifest `font.ui_cn`; **POINT filter** (像素字体红线) |
| 4 | 保留系统回退链 (fallback 声明化; Linux 无 simhei → 需在清单补充 Linux 常见路径) |
| 5 | 验证: §4 字符集核对 + 目视样本 + ctest (font 无障碍回归) |
| 6 | **Bold**: 若 UI 需要标题加粗 → 评估候选"缝合粗体"或 weight 双文件; 否则 B3B 只入 Regular |

## 6. Linux 发布兼容

- 候选 A/B 均 TTF/OTF, raylib LoadFontEx 跨平台读取, **无平台耦合** ✓ (对比现状依赖 `C:/Windows/Fonts/msyh.ttc` 的 Windows-only 链)
- Linux 下系统回退查 `fc-list :lang=zh` 常见路径 (清单补充: `/usr/share/fonts/...` Noto) — 主字体已入库后此为兜底

## 7. Review Gate 决策

- [ ] **选 A (Fusion Pixel 12px zh_hans) 入库?** — 像素风最契合, OFL 安全; 风险=生僻字覆盖 (验证 §4 兜底换 B)
- [ ] 若 A 验证失败 → 是否批准降级 **B (思源 SC Subset ~2MB)**?
- [ ] **POINT filter 切换** 确认 (像素字体硬要求, 会改变现有 UI 字体渲染观感)
- [ ] 只入 Regular; Bold 延后? (推荐是)
- [ ] 文件目标: `assets/fonts/game_regular.ttf` (+ LICENSE.txt)