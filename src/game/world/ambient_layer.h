#pragma once
// G11.2: AmbientLayer — 群系氛围渲染组件 (组合, 不继承)
// 职责: biomes.json ambient 段实装 (环境粒子) + AI 情绪光照 vignette
// 数据流: biomes.json → BiomeDef.ambient → GameScene::enter_floor → set_biome()
#include "raylib.h"
#include <vector>

struct BiomeDef;

struct AmbientParticle {
    float x, y;           // 世界坐标 (像素)
    float vy;             // 垂直速度 (rise 为负)
    float vx;             // 水平漂移
    float size;           // 半径
    float life, max_life; // 生命周期
    unsigned char alpha;  // 基础透明度
};

struct AmbientCfg {
    int   count = 0;
    Color color = {140, 135, 150, 200};
    float size_min = 1.0f, size_max = 2.5f;
    float speed = 12.0f;        // 像素/秒
    bool  rise = true;          // 上飘 (火山余烬/深渊幽光) 或下落 (监狱尘埃)
    float life_min = 2.5f, life_max = 6.0f;
};

class AmbientLayer {
public:
    // 每层进入时调用: 载入群系 ambient 配置
    void set_biome(const BiomeDef* biome);
    // AI 情绪: 探索率 [0,1] + 击杀势头 [0,1] → vignette 参数
    void set_mood(float explore_ratio, float kill_momentum);
    // 逻辑更新 (dt 秒)
    void update(float dt, int map_w, int map_h, float cam_x, float cam_y,
                int sw, int sh);
    // 渲染: 粒子 (世界坐标, 已含相机偏移) + vignette (屏幕空间)
    void draw(float cam_x, float cam_y, int sw, int sh) const;
    // vignette 独立绘制 (地图之上, HUD 之下)
    void draw_vignette(int sw, int sh) const;

private:
    void _spawn_one(AmbientParticle& p, int map_w, int map_h) const;
    void _spawn_all(int map_w, int map_h);

    AmbientCfg          _cfg;
    std::vector<AmbientParticle> _particles;
    // 情绪参数
    float _explore_ratio = 0.0f;    // 探索进度
    float _kill_momentum = 0.0f;    // 击杀势头 (衰减)
    float _vignette_pulse = 0.0f;   // 击杀瞬间脉动
};
