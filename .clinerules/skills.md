# 项目 Skill 路由（roguelike_cpp 专属）

除全局共享 skill（见 `C:\Users\HP\.cline\rules\skills-router.md`）外，本项目额外生效：

## roguelike-dev（项目工作流）

- 读取: `C:\Demo\roguelike_cpp\.claude\skills\roguelike-dev\SKILL.md`
- 触发: 修改 C++ 源码后准备提交 / 修改 `resources/*.json` / 构建验证 / 提交前审查
- 要求: 提交前必须先读该 SKILL.md 并执行其中校验清单（ctest 全绿 + 构建 0 警告）

## anysearch（项目副本）

- 本项目 `.claude\skills\anysearch\` 含独立副本，优先使用全局版 `C:\Users\HP\.claude\skills\anysearch\`

## 本项目既定工作约定

- 构建: `cmake --build build --config Release`；测试: `ctest --test-dir build -C Release`（当前 53 用例）
- 提交习惯: Conventional Commits（feat/fix/docs），CHANGELOG 按版本记录
- 同步: 提交后 push `origin/master`，并同步桌面版 `C:\Users\HP\Desktop\Roguelike-CPP版`
