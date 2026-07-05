#pragma once
#include <string>
#include <unordered_map>
#include "raylib.h"
#include "bgm_engine.h"

class AudioServer {
public:
    void init();
    void close();
    void play_bgm(const std::string& name, float vol = 0.4f);
    void stop_bgm(float = 0.4f);
    void play_sfx(const std::string& name, float vol = 0.6f);

private:
    BGMEngine _bgm;
    std::unordered_map<std::string, Sound> _sfx;
};
