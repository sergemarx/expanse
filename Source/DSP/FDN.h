#pragma once
#include "Util.h"

namespace gv
{
// 8-line feedback delay network with a Hadamard feedback matrix.
// Per-line damping, slow delay-length modulation and a freeze mode that
// closes the input and pushes the loop gain to (almost) unity.
class FDN
{
public:
    static constexpr int N = 8;
    static constexpr float kMaxSize = 2.5f;

    void prepare (double sr)
    {
        sampleRate = sr;
        scale = (float) (sr / 48000.0);
        static const float kBase[N] = { 1409, 1627, 1873, 2099, 2371, 2663, 2957, 3257 };
        for (int i = 0; i < N; ++i)
        {
            baseLen[i] = kBase[i] * scale;
            lines[i].prepare ((int) (baseLen[i] * kMaxSize) + (int) (16.0f * scale) + 64);
            curLen[i] = tgtLen[i] = baseLen[i] * 1.5f;
            phase[i] = 0.7854f * (float) i;
            lfoInc[i] = kTwoPi * (0.11f + 0.043f * (float) i) / (float) sr;
            gain[i] = 0.9f;
        }
        reset();
    }

    void reset()
    {
        for (int i = 0; i < N; ++i) { lines[i].reset(); lp[i] = 0.0f; }
        fz = 0.0f;
    }

    void setParams (float decaySec, float size, float damping, float modulation, bool freeze)
    {
        decaySec = std::max (decaySec, 0.1f);
        for (int i = 0; i < N; ++i)
        {
            tgtLen[i] = baseLen[i] * std::clamp (size, 0.25f, kMaxSize);
            gain[i] = std::pow (10.0f, -3.0f * tgtLen[i] / ((float) sampleRate * decaySec));
        }
        const float fc = 20000.0f * std::pow (0.08f, damping);
        lpA = std::exp (-kTwoPi * std::min (fc, 0.45f * (float) sampleRate) / (float) sampleRate);
        modDepth = modulation * 14.0f * scale;
        fzTarget = freeze ? 1.0f : 0.0f;
    }

    inline void process (float inL, float inR, float& outL, float& outR)
    {
        static const float kInSign[N]  = { 1, -1, 1, 1, -1, 1, -1, -1 };
        static const float kOutSign[N] = { 1, 1, -1, 1, -1, -1, 1, -1 };

        fz += (fzTarget - fz) * 0.0008f;
        const float inGain = (1.0f - fz) * 0.4f;

        float v[N], tap[N];
        for (int i = 0; i < N; ++i)
        {
            curLen[i] += (tgtLen[i] - curLen[i]) * 0.0004f;
            const float d = curLen[i] + modDepth * std::sin (phase[i]);
            phase[i] += lfoInc[i];
            if (phase[i] > kTwoPi) phase[i] -= kTwoPi;

            const float x = lines[i].read (std::max (d, 2.0f));
            lp[i] = x + lpA * (lp[i] - x);
            tap[i] = lp[i];
            const float g = gain[i] + (0.9999f - gain[i]) * fz;
            v[i] = lp[i] * g;
        }

        // Fast Walsh-Hadamard transform (orthonormal after scaling).
        for (int len = 1; len < N; len <<= 1)
            for (int i = 0; i < N; i += len * 2)
                for (int j = i; j < i + len; ++j)
                {
                    const float a = v[j], b = v[j + len];
                    v[j] = a + b;
                    v[j + len] = a - b;
                }
        constexpr float norm = 0.35355339f; // 1 / sqrt(8)

        outL = outR = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float in = (i & 1) ? inR : inL;
            lines[i].push (v[i] * norm + kInSign[i] * in * inGain);
            const float o = tap[i] * kOutSign[i];
            if (i & 1) outR += o; else outL += o;
        }
        outL *= 0.5f;
        outR *= 0.5f;
    }

private:
    DelayLine lines[N];
    float baseLen[N] {}, curLen[N] {}, tgtLen[N] {}, gain[N] {}, lp[N] {}, phase[N] {}, lfoInc[N] {};
    double sampleRate = 48000.0;
    float scale = 1.0f, lpA = 0.5f, modDepth = 0.0f, fz = 0.0f, fzTarget = 0.0f;
};

} // namespace gv
