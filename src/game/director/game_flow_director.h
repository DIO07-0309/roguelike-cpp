#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_map>

class Player;
class GameMap;
class Monster;
struct DroppedItem;
class GameScene;

// ============================================================
// D6 Step6: GameFlowDirector — 游戏生命周期状态机
// 组合 Boss/Gameplay/Presentation 三个Director
// GameScene 仅通过此 Director 调度流程
// ============================================================

enum class GameFlowState {
    TITLE,
    NEW_GAME,
    ENTER_FLOOR,
    PLAYING,
    BOSS_INTRO,
    BOSS_FIGHT,
    FLOOR_CLEAR,
    BOSS_DEAD,
    PLAYER_DEAD,
    GAME_CLEAR,
    ENDING,
    CREDITS,
    RETURN_TITLE
};

class GameFlowDirector {
public:
    GameFlowState current_state = GameFlowState::TITLE;

    // ── 注入GameScene引用(用于访问其player/map/monsters/directors) ──
    void bind(GameScene* gs) { _scene = gs; }

    // ── 生命周期 ──
    void new_game();    // NEW_GAME → reset → enter_floor(1)
    void load_saved_game(int floor, int max_f, std::unique_ptr<Player> p,
                         uint32_t seed,
                         const std::vector<bool>& special_triggered,
                         const std::vector<bool>& special_discovered,
                         const std::unordered_map<std::string, int>& rule_counters = {},
                         const std::unordered_map<int, int>& quest_states = {},
                         const std::vector<int>& unlocked_endings = {});

    // ── 状态转换 ──
    void on_boss_intro_confirm();   // BOSS_INTRO → spawn boss → BOSS_FIGHT
    void on_player_dead();          // → ENDING → DeathScene
    void on_game_clear();           // → ENDING → VictoryScene → Credits

    // ── 调试名称 ──
    static const char* state_name(GameFlowState s);

private:
    GameScene* _scene = nullptr;
};
