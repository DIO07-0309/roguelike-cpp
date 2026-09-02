#pragma once
#include "core/node.h"
#include "raylib.h"               // Rectangle
#include "save/save_manager.h"
#include <memory>
#include <vector>

class GameScene;

// ══ G10.9-C1: 统一三档选择场景 ══
// 复用于: 新游戏(空档直进/满档管理) · 继续(已有档) · 选关(已有档, 带该档maxf)
// 底层只调 Slot API: get_all_slots / set_active_slot / slot_exists / get_slot_summary
class SlotSelectScene : public Node {
public:
    enum class Mode { NEW_GAME, CONTINUE_GAME, SELECT_FLOOR };

    Mode mode = Mode::CONTINUE_GAME;

    void _ready() override;
    void _process(double delta) override;
    void _render() override;
    void _input(const class InputMap& input) override;

private:
    // 布局: 三张竖排大卡 (960x640 逻辑)
    static constexpr float CARD_W = 340.0f;
    static constexpr float CARD_H = 118.0f;
    static constexpr float CARD_GAP = 26.0f;
    static constexpr float CARD_X = 175.0f;   // 左列, 避右侧怪物
    static constexpr float CARD_Y0 = 205.0f;  // 标题带之下

    void _refresh_slots();
    Rectangle _card_rect(int i) const;
    bool _slot_clickable(int i) const;
    void _activate_slot(int i);
    void _confirm_delete(int i);
    void _enter_game(int i);
    void _draw_delete_confirm();

    std::vector<SlotSummary> _slots;
    int _cursor = 0;
    int _hover = -1;
    float _anim = 0.0f;

    // 满档删除流 (NEW_GAME 且三档全满)
    bool _delete_confirm_open = false;
    int _delete_target = -1;
};
