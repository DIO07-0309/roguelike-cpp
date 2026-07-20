#include "mod_dependency.h"
#include "core/logger.h"
#include <algorithm>
#include <queue>

// ============================================================
// G4.2: DependencyResolver — Kahn 拓扑排序 + 循环检测
// ============================================================

DepResult DependencyResolver::resolve(const std::vector<ModDepInfo>& mods) {
    DepResult result;
    if (mods.empty()) return result;

    // ── 构建 ID → index 映射 ──
    std::unordered_map<std::string, int> id_to_idx;
    for (size_t i = 0; i < mods.size(); i++)
        id_to_idx[mods[i].id] = (int)i;

    // ── 检查依赖缺失 ──
    std::vector<bool> skipped(mods.size(), false);
    for (size_t i = 0; i < mods.size(); i++) {
        for (auto& req : mods[i].requires_ids) {
            if (!id_to_idx.count(req)) {
                if (!skipped[i]) {
                    LOG_INFO("[Dependency] Skipping '%s': requires '%s' not found",
                             mods[i].id.c_str(), req.c_str());
                    result.skipped.push_back(mods[i].id);
                    skipped[i] = true;
                }
            }
        }
    }

    // ── 构建邻接表: load_after 和 requires 边 ──
    // 边方向: A load_after B → B 必须在 A 之前 → B → A
    //      A requires B    → B 必须在 A 之前 → B → A
    int n = (int)mods.size();
    std::vector<std::vector<int>> adj(n);
    std::vector<int> indegree(n, 0);

    for (int i = 0; i < n; i++) {
        if (skipped[i]) continue;
        // requires: B → A
        for (auto& req : mods[i].requires_ids) {
            auto it = id_to_idx.find(req);
            if (it == id_to_idx.end() || skipped[it->second]) continue;
            adj[it->second].push_back(i);  // B → A
            indegree[i]++;
        }
        // load_after: B → A
        for (auto& la : mods[i].load_after) {
            auto it = id_to_idx.find(la);
            if (it == id_to_idx.end() || skipped[it->second]) continue;
            adj[it->second].push_back(i);
            indegree[i]++;
        }
    }

    // ── Kahn 拓扑排序 ──
    std::queue<int> q;
    for (int i = 0; i < n; i++)
        if (!skipped[i] && indegree[i] == 0)
            q.push(i);

    int processed = 0;
    while (!q.empty()) {
        int u = q.front(); q.pop();
        result.ordered_ids.push_back(mods[u].id);
        processed++;

        for (int v : adj[u]) {
            indegree[v]--;
            if (indegree[v] == 0)
                q.push(v);
        }
    }

    // ── 剩余节点 = 循环依赖 ──
    for (int i = 0; i < n; i++) {
        if (!skipped[i] && indegree[i] > 0) {
            LOG_INFO("[Dependency] Skipping '%s': cyclic dependency detected",
                     mods[i].id.c_str());
            result.cycle_info.push_back(mods[i].id);
        }
    }

    if (!result.ordered_ids.empty())
        LOG_INFO("[Dependency] Load order: %zu mods", result.ordered_ids.size());
    return result;
}
