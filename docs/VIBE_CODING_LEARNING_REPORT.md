# Vibe Coding 从入门到精通 — 学习报告

> 学习时间：2026-07-24  
> 学习材料：《Vibe Coding从入门到精通》（5章）  
> 实践项目：roguelike_cpp（C++17 Raylib 地牢肉鸽）

---

## 一、课程理论体系

### 核心认知

Vibe Coding 的本质不是"对 AI 说话"，而是**把意图表述清晰、无歧义、边界清楚**。

| 传统编程 | Vibe Coding |
| :--- | :--- |
| 自己想方案，自己写代码 | 描述需求，AI 出方案和代码 |
| 一行一行调试 | 把错误信息给 AI，它自己调 |
| 查文档、查接口 | 直接问 AI「怎么实现 XXX」 |
| 代码 Review 靠人工 | 让 AI 解释它写的代码 |
| 重构要先理解全部代码 | 告诉 AI「把这块重构成 XXX 模式」 |

### 五步开发法

```
描述需求 → 审查方案 → 确认执行 → 验证结果 → 迭代改进
```

每一步你只做两件事：**说清楚要什么**，**验证结果对不对**。

### Plan → Craft 黄金工作流

1. **Plan 模式**：先讨论方案，反复调整，不写代码
2. **编辑器写执行指令**：方案确认后写进 Markdown
3. **Craft 模式**：放手让 AI 一次性执行完成

### 项目宪章（CLAUDE.md）

- **最重要的文件**：AI 每次启动第一个就读它
- **从护栏开始**：AI 犯一次错就加一条规则，不写"百科全书"
- **判断标准**：AI 能从代码里读出来的不写；AI 猜不到的必须写

### Skills 渐进式披露

```
Level 1: SKILL.md 元数据（name + description）→ 始终在 Context
    ↓ 匹配触发
Level 2: SKILL.md 完整内容（正式指令）→ 触发时加载
    ↓ 按需
Level 3: Scripts/Reference/Assets → 按需动态加载，不进 Context
```

---

## 二、实践成果

### 2.1 第一个项目：AI 新闻聚合 CLI

| 项目 | 详情 |
| :--- | :--- |
| 位置 | `C:\Demo\ai-news-digest` |
| 技术栈 | TypeScript + Node.js + tsx |
| 功能 | 3个 RSS 源并行抓取 → URL 去重 → AI 摘要 → Markdown 日报 |
| 输出 | `output/YYYY-MM-DD.md`（首次运行 44 篇，13.5K 字符） |
| 运行 | `npx tsx src/index.ts`（立即）/ `--cron`（每天8:00） |

**验证**：一次运行成功，0 errors。日报格式完整（统计 + 时间倒序 + AI 摘要 + 链接）。

### 2.2 项目宪章：CLAUDE.md

| 项目 | 详情 |
| :--- | :--- |
| 位置 | `C:\Demo\roguelike_cpp\CLAUDE.md` |
| 长度 | ~80 行（精简，符合"不写百科全书"原则） |
| 内容 | 架构/开发命令/10条规范/代码风格/5条常见陷阱/目录约定/5条不要做/开发工具/设计文档 |

**关键改进**：
- 修复 `assets/` vs `resources/` 混淆（JSON 配置在 `resources/`，不在 `assets/`）
- 添加 3 个 tools 文档 + 6 个 docs 文档索引（含"何时查阅"列）
- 修正 World Validator 运行路径

### 2.3 自定义 Skill：roguelike-dev

| 项目 | 详情 |
| :--- | :--- |
| 位置 | `.claude/skills/roguelike-dev/SKILL.md` |
| 功能 | 6 步开发工作流 + 5 条 JSON 规则 + 问题速查表 |

**验证**：项目 0 errors 0 warnings，131/131 headers 有 `#pragma once`

### 2.4 验证结果

| 检查项 | 结果 |
| :--- | :--- |
| World Validator | ✅ 0 errors, 0 warnings |
| `#pragma once` 覆盖率 | ✅ 131/131 (100%) |
| `using namespace std` in headers | ✅ 0 |
| `new`/`delete` 裸指针 | ⚠️ 26处（需逐步迁移到 `unique_ptr`，已列入 G7.4） |

---

## 三、关键心得

### 3.1 最有效的技巧

1. **"先别急着写代码，给我一个方案"** — 让 AI 先想清楚再动手
2. **具体化 + 指向已有模式** — 比模糊描述有效 100 倍
3. **描述症状，不猜原因** — 让 AI 自己定位 bug
4. **两次纠正不行，果断 `/clear` 重来** — 在跑偏的对话上修补是浪费

### 3.2 踩坑记录

1. **一个会话什么都塞** → 不同任务用 `/clear` 或新对话
2. **看着像对的就接受了** → 每轮都实际跑一次验证
3. **过度微操** → 关注最终输出，中间过程不用逐行看

### 3.3 迁移到 roguelike 项目

- `resources/`（20+ JSON）是数据骨干，修改后必须跑 World Validator
- CLAUDE.md 已有 10 条规范 + 5 条陷阱，持续从犯错中补充
- `roguelike-dev` Skill 封装了完整开发工作流
- 26 处裸 `new` 是已知技术债（G7.4 路线图中）

---

## 四、下一步行动

1. **飞轮启动**：每次 AI 犯错就加一条到 CLAUDE.md（课程第3章迭代飞轮）
2. **G7.4 推进**：将 26 处裸 `new` 逐步迁移为 `std::make_unique`
3. **Skill 迭代**：`roguelike-dev` 随项目演进持续更新
4. **CI/CD**：将 World Validator + 编译检查接入 GitHub Actions

---

*报告自动生成 · 2026-07-24*
