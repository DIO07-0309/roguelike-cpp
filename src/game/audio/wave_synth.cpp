#include "wave_synth.h"
#include <algorithm>
#include <random>

static std::mt19937 _rng(42);

std::vector<short> square_wave(int n, std::function<float(float)> freq_fn, AmpFn amp) {
    std::vector<short> data(n, 0);
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float f = freq_fn(t);
        float a = amp(t);
        if (f <= 0) continue;
        int period = std::max(1, (int)(SR / f));
        data[i] = (short)(MAX_AMP * a * ((i % period) < period / 2 ? 1.0f : -1.0f));
    }
    return data;
}

std::vector<short> sine_wave(int n, std::function<float(float)> freq_fn, AmpFn amp) {
    std::vector<short> data(n, 0);
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float f = freq_fn(t);
        float a = amp(t);
        if (f <= 0) continue;
        data[i] = (short)(MAX_AMP * a * sinf(2.0f * PI * f * t));
    }
    return data;
}

std::vector<short> noise_wave(int n, AmpFn amp) {
    std::vector<short> data(n, 0);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (int i = 0; i < n; i++) {
        float a = amp((float)i / SR);
        data[i] = (short)(MAX_AMP * a * dist(_rng));
    }
    return data;
}

std::vector<short> mix(std::initializer_list<const std::vector<short>*> arrays) {
    int n = INT_MAX;
    for (auto* a : arrays) n = std::min(n, (int)a->size());
    if (n == INT_MAX) return {};
    std::vector<short> r(n, 0);
    for (auto* a : arrays) {
        for (int i = 0; i < n; i++) {
            int v = (int)r[i] + (int)(*a)[i];
            r[i] = (short)std::max(-MAX_AMP, std::min(MAX_AMP - 1, v));
        }
    }
    return r;
}

AmpFn decay(float peak, float dur) {
    return [=](float t) { return peak * std::max(0.0f, 1.0f - t / dur) * std::max(0.0f, 1.0f - t / dur); };
}

AmpFn spike(float peak, float rise, float dur) {
    return [=](float t) {
        if (t < rise) return peak * (t / rise);
        float d = dur - rise;
        return peak * std::pow(std::max(0.0f, 1.0f - (t - rise) / d), 1.5f);
    };
}
