#pragma once
#include <vector>
#include <functional>
#include <cstdint>
#include <cmath>

// ============================================================
// WaveSynth — Python sfx_engine.py 的 C++ 移植
// SR=22050, 16-bit signed mono
// ============================================================
#define _USE_MATH_DEFINES
#include <math.h>
inline constexpr int SR = 22050;
inline constexpr int MAX_AMP = 32767;
#ifndef PI
#define PI 3.14159265358979323846f
#endif

using AmpFn = std::function<float(float t)>;

// 基础波形生成
std::vector<short> square_wave(int n, std::function<float(float)> freq_fn, AmpFn amp);
std::vector<short> sine_wave(int n, std::function<float(float)> freq_fn, AmpFn amp);
std::vector<short> noise_wave(int n, AmpFn amp);

// 混音
std::vector<short> mix(std::initializer_list<const std::vector<short>*> arrays);

// 包络函数
AmpFn decay(float peak=0.8f, float dur=0.3f);
AmpFn spike(float peak=0.9f, float rise=0.01f, float dur=0.3f);
