#include "audio_server.h"
#include "wave_synth.h"
#include "core/logger.h"
#include <cmath>
#include <random>
#include <cstring>

// ---- 将样本数组打包为 Raylib Sound ----
static Sound _vec_to_sound(const std::vector<short>& data) {
    int n = (int)data.size();
    short* buf = (short*)malloc(n * sizeof(short));
    memcpy(buf, data.data(), n * sizeof(short));

    Wave wave;
    wave.frameCount = n;
    wave.sampleRate = SR;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = buf;

    Sound snd = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return snd;
}

// ---- 各音效合成器 (Python sfx_engine.py 直译) ----

static Sound _compile_melee() {
    float dur = 0.18f; int n = (int)(SR * dur);
    auto sw = square_wave(n, [=](float t){return 500 - 420 * std::pow(t/dur, 0.6f);}, spike(0.7f, 0.01f, dur));
    auto ns = noise_wave(n, spike(0.25f, 0.02f, dur*0.5f));
    auto mx = mix({&sw, &ns});
    return _vec_to_sound(mx);
}

static Sound _compile_hit() {
    float dur = 0.12f; int n = (int)(SR * dur);
    auto th = sine_wave(n, [=](float){return 80.0f;}, spike(0.9f, 0.005f, dur));
    auto ns = noise_wave(n, spike(0.3f, 0.01f, dur*0.6f));
    auto mx = mix({&th, &ns});
    return _vec_to_sound(mx);
}

static Sound _compile_slash() {
    float dur = 0.35f; int n = (int)(SR * dur);
    auto sw = square_wave(n, [=](float t){return 800 - 650 * std::sqrt(t/dur);}, spike(0.8f, 0.01f, dur));
    auto ring = sine_wave(n, [=](float t){return 2400.0f - 600 * (t/dur);}, decay(0.25f, dur));
    auto ns = noise_wave(n, spike(0.15f, 0.005f, 0.1f));
    auto mx = mix({&sw, &ring, &ns});
    return _vec_to_sound(mx);
}

static Sound _compile_bolt() {
    float dur = 0.45f; int n = (int)(SR * dur);
    std::vector<short> result(n, 0);
    std::mt19937 rng2(42);
    std::uniform_int_distribution<int> pos_dist(0, n-1);
    for (int k = 0; k < 32; k++) {
        int start = pos_dist(rng2);
        int length = 80 + rng2() % 321;
        float burst = (float)(rng2() % 900) / 1000.0f;
        for (int i = 0; i < length; i++) {
            int pos = start + i;
            if (pos < n) {
                float d = std::pow(std::max(0.0f, 1.0f - (float)i / length), 3.0f);
                result[pos] = (short)(MAX_AMP * burst * d * ((float)(rng2()%2000)/1000.0f - 1.0f));
            }
        }
    }
    auto thunder = sine_wave(n, [=](float t){return 45.0f + 20.0f * sinf(t*15);}, decay(0.4f, dur));
    auto mx = mix({&result, &thunder});
    return _vec_to_sound(mx);
}

static Sound _compile_heal() {
    float dur = 0.7f; int n = (int)(SR * dur);
    std::vector<short> result(n, 0);
    float notes[] = {1046.5f, 1318.5f, 1568.0f, 2093.0f};
    for (int j = 0; j < 4; j++) {
        int start = (int)(SR * j * 0.12f);
        auto chunk = sine_wave((int)(SR * 0.25f), [=](float){return notes[j];}, decay(0.45f, 0.25f));
        for (int i = 0; i < (int)chunk.size(); i++) {
            if (start + i < n) {
                int v = (int)result[start+i] + (int)chunk[i];
                result[start+i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, v));
            }
        }
    }
    auto wind = sine_wave(n, [=](float){return 800.0f;}, decay(0.15f, dur));
    auto pad = sine_wave(n, [=](float){return 523.25f;}, decay(0.12f, dur));
    auto mx = mix({&result, &wind, &pad});
    return _vec_to_sound(mx);
}

static Sound _compile_pickup() {
    float dur = 0.25f; int n = (int)(SR * dur);
    auto c5 = sine_wave(n, [=](float){return 523.25f;}, decay(0.35f, dur));
    auto e5 = sine_wave(n, [=](float){return 659.25f;}, decay(0.35f, dur));
    auto mx = mix({&c5, &e5});
    return _vec_to_sound(mx);
}

static Sound _compile_levelup() {
    float dur = 0.5f; int n = (int)(SR * dur);
    std::vector<short> result(n, 0);
    float notes[] = {392.0f, 523.25f, 659.25f, 784.0f, 1046.5f};
    for (int j = 0; j < 5; j++) {
        int start = (int)(SR * j * 0.08f);
        auto chunk = square_wave((int)(SR * 0.15f), [=](float){return notes[j];}, decay(0.4f, 0.15f));
        for (int i = 0; i < (int)chunk.size(); i++) {
            if (start + i < n) {
                int v = (int)result[start+i] + (int)chunk[i];
                result[start+i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, v));
            }
        }
    }
    return _vec_to_sound(result);
}

static Sound _compile_victory() {
    float dur = 2.5f; int n = (int)(SR * dur);
    std::vector<short> result(n, 0);

    // 琶音上行
    struct Note { float freq, start, len; };
    Note notes[] = {
        {261.63f,0.0f,0.35f},{329.63f,0.2f,0.3f},{392.0f,0.4f,0.3f},
        {523.25f,0.6f,0.4f},{659.25f,0.9f,0.25f},{784.0f,1.05f,0.3f},{1046.5f,1.25f,0.5f}
    };
    for (auto& nt : notes) {
        int start = (int)(SR * nt.start), chunk_n = (int)(SR * nt.len);
        auto chunk = square_wave(chunk_n, [=](float){return nt.freq;}, decay(0.5f, nt.len));
        for (int i = 0; i < chunk_n; i++)
            if (start + i < n) { int v = (int)result[start+i] + (int)chunk[i]; result[start+i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, v)); }
    }
    // 和弦铺垫
    int pad_start = (int)(SR * 1.2f), pad_n = (int)(SR * 1.1f);
    float chords[] = {261.63f, 329.63f, 392.0f};
    for (auto f : chords) {
        auto pad = sine_wave(pad_n, [=](float){return f;}, decay(0.35f, 1.1f));
        for (int i = 0; i < pad_n; i++)
            if (pad_start + i < n) { int v = (int)result[pad_start+i] + (int)pad[i]; result[pad_start+i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, v)); }
    }
    // 终响
    int fin_start = (int)(SR * 1.9f), fin_n = (int)(SR * 0.55f);
    float fin_notes[] = {523.25f, 659.25f, 784.0f, 1046.5f};
    for (auto f : fin_notes) {
        auto fn = square_wave(fin_n, [=](float){return f;}, decay(0.6f, 0.55f));
        for (int i = 0; i < fin_n; i++)
            if (fin_start + i < n) { int v = (int)result[fin_start+i] + (int)fn[i]; result[fin_start+i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP-1, v)); }
    }
    return _vec_to_sound(result);
}

// ============================================================
// AudioServer
// ============================================================
void AudioServer::init() {
    LOG_INFO("音频: 初始化SFX...");
    _sfx["melee"]   = _compile_melee();
    _sfx["hit"]     = _compile_hit();
    _sfx["slash"]   = _compile_slash();
    _sfx["bolt"]    = _compile_bolt();
    _sfx["heal"]    = _compile_heal();
    _sfx["pickup"]  = _compile_pickup();
    _sfx["levelup"] = _compile_levelup();
    _sfx["victory"] = _compile_victory();

    // timestop: 优先外部 MP3
    const char* mp3 = "assets/jojo_timestop.mp3";
    if (FileExists(mp3)) _sfx["timestop"] = LoadSound(mp3);
    else _sfx["timestop"] = _compile_bolt();

    // BGM
    LOG_INFO("音频: 合成BGM(4支)...");
    _bgm.init();
    LOG_INFO("音频: 就绪 (8SFX + 4BGM)");
}

void AudioServer::close() {
    for (auto& [_, snd] : _sfx) UnloadSound(snd);
    _sfx.clear();
    _bgm.close();
}

void AudioServer::play_bgm(const std::string& name, float vol) {
    LOG_DEBUG("BGM -> %s (vol:%.2f)", name.c_str(), vol);
    _bgm.play(name, vol);
}
void AudioServer::stop_bgm(float) { _bgm.stop(); }

void AudioServer::play_sfx(const std::string& name, float vol) {
    auto it = _sfx.find(name);
    if (it != _sfx.end()) { SetSoundVolume(it->second, vol); PlaySound(it->second); }
}
