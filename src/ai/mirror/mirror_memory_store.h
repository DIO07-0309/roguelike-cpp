#pragma once
// B3-M: Mirror 闭环 — BehaviorCloneTable 跨局持久化 (saves/mirror_memory.json)
// 单一职责: 文件读写 + 跨局遗忘曲线; 是否允许读写 (sim 确定性红线) 由调用方把关
#include <string>
#include "ai/mirror/behavior_clone_table.h"

namespace mirror {

class MirrorMemoryStore {
public:
    // 读入并逐 intent 计数衰减 kDecayPerLoad 后 merge 进 table; 无文件/损坏 → false
    static bool load_into(BehaviorCloneTable& table);
    // 快照落盘 (先写 .tmp 再 rename, 伪原子替换); 全零行剔除
    static bool save_from(const BehaviorCloneTable& table);

    static std::string path();
    static void set_path_for_test(const std::string& p);  // 测试注入, 生产不碰

    static constexpr float kDecayPerLoad = 0.99f;  // 每次读入 ×0.99: 旧证据让位新习惯
    static constexpr size_t kMaxEntries = 128;     // 熔丝: 桶组合上限 80, 超出截断

private:
    static std::string _path;
};

}  // namespace mirror
