#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

// ============================================================
// G4.2: DependencyResolver — 拓扑排序 + 循环检测
// 输入: Mod manifest 列表 (id, requires, load_after)
// 输出: 排序后的 Mod ID 列表 (依赖先于被依赖者)
// ============================================================

struct ModDepInfo {
    std::string id;
    std::vector<std::string> requires_ids;   // 硬依赖 — 缺失则跳过
    std::vector<std::string> load_after;     // 软依赖 — 仅排序
};

struct DepResult {
    std::vector<std::string> ordered_ids;    // 拓扑排序结果
    std::vector<std::string> skipped;        // 被跳过的 Mod (缺失依赖)
    std::vector<std::string> cycle_info;     // 循环中跳过的 Mod
};

class DependencyResolver {
public:
    // 主入口: 输入所有 Mod 信息, 返回排序结果
    static DepResult resolve(const std::vector<ModDepInfo>& mods);
};
