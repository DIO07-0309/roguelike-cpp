#pragma once
// ============================================================
// G10.8-B3/B4: First Encounter Hint — 单点工具
// 用法: first_hint(gs, "encounter_lock", "房间已封锁", "击败所有敌人后开启");
// 首次(跨 run 持久化)返回 true 并自动标记; 之后永不再弹。
// 显示走 GameScene::room_msg 现有提示管道。
// ============================================================
#include <string>

class GameScene;

// 返回 true = 本次是首次(已标记,调用方无需再做事)
bool first_hint(GameScene& gs, const std::string& hint_id,
                const std::string& line1, const std::string& line2 = "");
