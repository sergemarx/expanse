#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Framework-free DSP building blocks (no JUCE dependency, so they can be unit-tested anywhere).
namespace gv
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

// Power-of-two circular delay line with linear interpolation.
// read(d): d = 1 returns the most recently pushed sample. d must be >= 1.
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int n = 1;
        while (n < maxDelaySamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        writePos = 0;
    }

    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; }

    inline void push (float x)
    {
        buf[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    inline float read (float d) const
    {
        const int di = (int) d;
        const float frac = d - (float) di;
        const int i0 = (writePos - di) & mask;
        const int i1 = (i0 - 1) & mask;
        return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

private:
    std::vector<float> buf;
    int mask = 0;
    int writePos = 0;
};

// Schroeder allpass, used for input diffusion.
class Allpass
{
public:
    void prepare (int len)
    {
        buf.assign ((size_t) std::max (len, 1), 0.0f);
        idx = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); idx = 0; }
    void setGain (float newGain) { g = newGain; }

    inline float process (float x)
    {
        const float d = buf[(size_t) idx];
        const float v = x + g * d;
        buf[(size_t) idx] = v;
        if (++idx >= (int) buf.size()) idx = 0;
        return d - g * v;
    }

private:
    std::vector<float> buf;
    int idx = 0;
    float g = 0.5f;
};

struct OnePoleLP
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float fc, double sr)
    {
        fc = std::min (fc, 0.45f * (float) sr);
        a = std::exp (-kTwoPi * fc / (float) sr);
    }
    void reset() { z = 0.0f; }
    inline float process (float x)
    {
        z = (1.0f - a) * x + a * z;
        return z;
    }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void setCutoff (float fc, double sr) { lp.setCutoff (fc, sr); }
    void reset() { lp.reset(); }
    inline float process (float x) { return x - lp.process (x); }
};

// Simple one-pole parameter smoother (per-sample).
struct Smoother
{
    float cur = 0.0f;
    float coef = 0.002f;
    void snap (float v) { cur = v; }
    inline float next (float target)
    {
        cur += (target - cur) * coef;
        return cur;
    }
};

} // namespace gv
