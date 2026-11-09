#pragma once
#include "Util.h"

namespace gv
{
// Dual-tap granular pitch shifter (two crossfaded, sweeping delay taps).
// Good enough for the washy, smeared character that shimmer reverbs want.
class PitchShifter
{
public:
    void prepare (double sr)
    {
        win = (float) (0.12 * sr);
        dl.prepare ((int) win + 8);
        reset();
    }
    void reset() { dl.reset(); phase = 0.0f; }
    void setSemitones (float st) { ratio = std::pow (2.0f, st / 12.0f); }

    inline float process (float x)
    {
        dl.push (x);
        phase += (1.0f - ratio) / win;
        phase -= std::floor (phase);

        float out = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            float p = phase + 0.5f * (float) k;
            p -= std::floor (p);
            const float w = 0.5f - 0.5f * std::cos (kTwoPi * p);
            out += w * dl.read (1.0f + p * win);
        }
        return out;
    }

private:
    DelayLine dl;
    float win = 4800.0f;
    float phase = 0.0f;
    float ratio = 2.0f;
};

// Reverses the incoming audio in overlapping windowed chunks (50% overlap, Hann).
// Fed with the reverb output this gives swelling, backwards-sounding tails.
class ReverseEngine
{
public:
    void prepare (double sr, float maxSeconds)
    {
        sampleRate = sr;
        maxT = (int) (maxSeconds * sr);
        dl.prepare (2 * maxT + 16);
        T = std::min (maxT, (int) (0.8 * sr));
        reset();
    }
    void reset() { dl.reset(); n = 0; }
    void setTime (float seconds)
    {
        T = std::clamp ((int) (seconds * sampleRate), 64, maxT);
        if (n >= T) n = 0;
    }

    inline float process (float x)
    {
        dl.push (x);
        float out = 0.0f;
        const int n2 = (n + T / 2) % T;
        const int idx[2] = { n, n2 };
        for (int g = 0; g < 2; ++g)
        {
            const float w = 0.5f - 0.5f * std::cos (kTwoPi * (float) idx[g] / (float) T);
            out += w * dl.read (1.0f + 2.0f * (float) idx[g]);
        }
        if (++n >= T) n = 0;
        return out;
    }

private:
    DelayLine dl;
    double sampleRate = 48000.0;
    int maxT = 1, T = 1, n = 0;
};

// Tape/cassette-ish degradation: wow & flutter, sample-rate reduction, bit reduction,
// soft saturation and a darkening low-pass. amount = 0 is near-transparent.
class LoFi
{
public:
    void prepare (double sr, float phaseOffset)
    {
        sampleRate = sr;
        wob.prepare ((int) (0.04 * sr) + 8);
        phase1 = phaseOffset;
        phase2 = phaseOffset * 1.7f;
        reset();
    }
    void reset() { wob.reset(); lp.reset(); held = 0.0f; counter = 0; }

    void setAmount (float a)
    {
        amount = a;
        levels = std::pow (2.0f, 15.0f - 8.0f * a); // 16-bit down to ~8-bit
        hold = 1 + (int) (a * 5.0f);
        lp.setCutoff (18000.0f * std::pow (0.2f, a), sampleRate);
        drive = 1.0f + 1.5f * a;
    }

    inline float process (float x)
    {
        wob.push (x);
        phase1 += kTwoPi * 0.63f / (float) sampleRate;
        phase2 += kTwoPi * 0.17f / (float) sampleRate;
        if (phase1 > kTwoPi) phase1 -= kTwoPi;
        if (phase2 > kTwoPi) phase2 -= kTwoPi;
        const float depth = (float) sampleRate * 0.0035f * amount;
        const float d = 2.0f + depth * (1.0f + 0.7f * std::sin (phase1) + 0.5f * std::sin (phase2));
        float y = wob.read (d);

        if (++counter >= hold) { counter = 0; held = std::round (y * levels) / levels; }
        y = held;
        y = std::tanh (y * drive) / drive;
        return lp.process (y);
    }

private:
    DelayLine wob;
    OnePoleLP lp;
    double sampleRate = 48000.0;
    float amount = 0.0f, levels = 32768.0f, drive = 1.0f, held = 0.0f;
    float phase1 = 0.0f, phase2 = 0.0f;
    int hold = 1, counter = 0;
};

// Stereo ping-pong delay with a darkened feedback path.
class PingPongDelay
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        dlL.prepare ((int) (2.2 * sr));
        dlR.prepare ((int) (2.2 * sr));
        curT = tgtT = (float) (0.375 * sr);
        lpL.setCutoff (5500.0f, sr);
        lpR.setCutoff (5500.0f, sr);
        reset();
    }
    void reset() { dlL.reset(); dlR.reset(); lpL.reset(); lpR.reset(); }
    void setParams (float timeSeconds, float feedback)
    {
        tgtT = std::clamp (timeSeconds, 0.01f, 2.0f) * (float) sampleRate;
        fb = feedback;
    }

    inline void process (float monoIn, float& outL, float& outR)
    {
        curT += (tgtT - curT) * 0.0006f;
        const float tL = dlL.read (std::max (curT, 2.0f));
        const float tR = dlR.read (std::max (curT, 2.0f));
        dlL.push (monoIn + lpR.process (tR) * fb);
        dlR.push (lpL.process (tL) * fb);
        outL = tL;
        outR = tR;
    }

private:
    DelayLine dlL, dlR;
    OnePoleLP lpL, lpR;
    double sampleRate = 48000.0;
    float curT = 0.0f, tgtT = 0.0f, fb = 0.4f;
};

} // namespace gv
