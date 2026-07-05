#pragma once
#include <string>
#include <unordered_map>
#include "raylib.h"

class BGMEngine {
public:
    void init();
    void close();
    void play(const std::string& name, float vol = 0.4f);
    void stop();

private:
    Sound _compile_bgm(const std::string& name);
    std::unordered_map<std::string, Sound> _cache;
    bool _playing = false;
};
