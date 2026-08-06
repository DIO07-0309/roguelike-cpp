#pragma once
#include <deque>

// M2: 滚动窗口在线准确率 — 最近 MAX_WINDOW 次预测命中的滑动平均值。
// 只关心近期表现（动态 Phase 触发用），不累计全局统计。
class RollingAccuracy {
public:
    void reset();
    void add(bool hit);
    float accuracy() const;      // 窗口内命中率 [0,1]
    int   hit_count() const;     // 窗口内命中次数
    int   total() const;         // 累计预测次数
private:
    std::deque<bool> _window;
    int _hits = 0;
    int _total = 0;
};
