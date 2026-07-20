#pragma once

// ============================================================
// G1 Step1: AttackEvolutionState — 普攻进化状态 (挂在 Player 上)
// 属于 Progression 层, 不属于 Combat
// ============================================================

struct AttackEvolutionState {
    int level = 1;   // 1=铁剑, 2=剑气, 3=旋风斩, (4-6 未来)

    const char* name() const {
        switch (level) {
            case 2: return "剑气";
            case 3: return "旋风斩";
            default: return "铁剑";
        }
    }
};
