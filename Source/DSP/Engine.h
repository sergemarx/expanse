#pragma once
#include "Util.h"
#include "FDN.h"
#include "Effects.h"

namespace gv
{
struct Params
{
    float mix = 0.35f;          // 0..1 dry/wet
    float decay = 8.0f;         // seconds (RT60)
    float size = 1.5f;          // 0.5..2.5 delay-line scale
    float preDelayMs = 25.0f;
    float damping = 0.35f;      // 0..1 high-frequency damping in the tail
    float lowCutHz = 120.0f;    // low cut on reverb send
    float modulation = 0.4f;    // 0..1
    float diffusion = 0.7f;     // 0..1
    float shimmer = 0.3f;       // 0..1 amount of pitched feedback
    float shimmerSemis = 12.0f;
    float reverse = 0.0f;       // 0..1 reversed-tail blend
    float reverseTimeMs = 800.0f;
    bool  freeze = false;
    float lofi = 0.0f;          // 0..1
    float delayMix = 0.0f;      // 0..1 audible echo level (also feeds the reverb)
    float delayTimeMs = 375.0f;
    float delayFeedback = 0.4f;
    float width = 1.0f;         // 0..2
    float outputDb = 0.0f;
};

// Whole signal chain:
//   in -> ping-pong delay -> low cut -> pre-delay -> diffusers (+ shimmer feedback)
//      -> 8-line FDN -> [reverse blend] -> lo-fi -> width -> mix
class Engine
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        delay.prepare (sr);
        fdn.prepare (sr);
        psL.prepare (sr); psR.prepare (sr);
        revL.prepare (sr, 2.1f); revR.prepare (sr, 2.1f);
        lofiL.prepare (sr, 0.0f); lofiR.prepare (sr, 1.3f);
        preL.prepare ((int) (0.31 * sr)); preR.prepare ((int) (0.31 * sr));

        const float s = (float) (sr / 48000.0);
        const int lenL[4] = { 142, 107, 379, 277 };
        const int lenR[4] = { 151, 113, 401, 293 };
        for (int k = 0; k < 4; ++k)
        {
            apL[k].prepare ((int) (lenL[k] * s));
            apR[k].prepare ((int) (lenR[k] * s));
        }
        for (auto* h : { &hpL1, &hpL2, &hpR1, &hpR2 }) h->setCutoff (120.0f, sr);
        shHpL.setCutoff (250.0f, sr);
        shHpR.setCutoff (250.0f, sr);

        for (auto* sm : { &mixS, &delayMixS, &shimS, &revS, &outS }) sm->coef = 0.002f;
        reset();
        setParams (Params {});
        snapSmoothers();
    }

    void reset()
    {
        delay.reset(); fdn.reset();
        psL.reset(); psR.reset(); revL.reset(); revR.reset();
        lofiL.reset(); lofiR.reset(); preL.reset(); preR.reset();
        for (int k = 0; k < 4; ++k) { apL[k].reset(); apR[k].reset(); }
        for (auto* h : { &hpL1, &hpL2, &hpR1, &hpR2, &shHpL, &shHpR }) h->reset();
        shimFbL = shimFbR = 0.0f;
    }

    void setParams (const Params& newParams)
    {
        p = newParams;
        fdn.setParams (p.decay, p.size, p.damping, p.modulation, p.freeze);
        delay.setParams (p.delayTimeMs * 0.001f, std::min (p.delayFeedback, 0.95f));
        psL.setSemitones (p.shimmerSemis);
        psR.setSemitones (p.shimmerSemis);
        revL.setTime (p.reverseTimeMs * 0.001f);
        revR.setTime (p.reverseTimeMs * 0.001f);
        lofiL.setAmount (p.lofi);
        lofiR.setAmount (p.lofi);
        preTarget = std::max (1.0f, p.preDelayMs * 0.001f * (float) sampleRate);
        const float g = 0.25f + 0.45f * p.diffusion;
        for (int k = 0; k < 4; ++k) { apL[k].setGain (g); apR[k].setGain (g); }
        for (auto* h : { &hpL1, &hpL2, &hpR1, &hpR2 }) h->setCutoff (p.lowCutHz, sampleRate);
        outGain = std::pow (10.0f, p.outputDb / 20.0f);
    }

    void process (float* L, float* R, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const float mix = mixS.next (p.mix);
            const float dMix = delayMixS.next (p.delayMix);
            const float shim = shimS.next (p.shimmer);
            const float rev = revS.next (p.reverse);
            const float og = outS.next (outGain);
            const float dryG = std::cos (mix * 0.5f * kPi);
            const float wetG = std::sin (mix * 0.5f * kPi) * 1.4f;

            const float inL = L[n], inR = R[n];

            float dlyL, dlyR;
            delay.process (0.5f * (inL + inR), dlyL, dlyR);

            // Reverb send: dry + echoes, low-cut, pre-delayed, diffused.
            float sL = hpL2.process (hpL1.process (inL + dlyL * dMix));
            float sR = hpR2.process (hpR1.process (inR + dlyR * dMix));

            preL.push (sL); preR.push (sR);
            preCur += (preTarget - preCur) * 0.001f;
            sL = preL.read (std::max (preCur, 1.0f));
            sR = preR.read (std::max (preCur, 1.0f));

            for (int k = 0; k < 4; ++k) { sL = apL[k].process (sL); sR = apR[k].process (sR); }

            sL += shimFbL;
            sR += shimFbR;

            float wL, wR;
            fdn.process (sL, sR, wL, wR);

            // Shimmer: pitch-shift the tail and feed it back into the reverb.
            // Loop gain stays below unity at max so shimmer builds and blooms but
            // still decays; the high-pass stops low-frequency energy accumulating
            // (especially when pitching down).
            shimFbL = std::tanh (shHpL.process (psL.process (wL)) * shim * kShimmerGain);
            shimFbR = std::tanh (shHpR.process (psR.process (wR)) * shim * kShimmerGain);

            // Reverse blend.
            const float rL = revL.process (wL), rR = revR.process (wR);
            wL = wL * (1.0f - 0.7f * rev) + rL * rev;
            wR = wR * (1.0f - 0.7f * rev) + rR * rev;

            wL = lofiL.process (wL);
            wR = lofiR.process (wR);

            const float mid = 0.5f * (wL + wR);
            const float side = 0.5f * (wL - wR) * p.width;
            wL = mid + side;
            wR = mid - side;

            L[n] = (inL * dryG + dlyL * dMix + wL * wetG) * og;
            R[n] = (inR * dryG + dlyR * dMix + wR * wetG) * og;
        }
    }

private:
    void snapSmoothers()
    {
        mixS.snap (p.mix); delayMixS.snap (p.delayMix); shimS.snap (p.shimmer);
        revS.snap (p.reverse); outS.snap (outGain);
        preCur = preTarget;
    }

    Params p;
    double sampleRate = 48000.0;

    PingPongDelay delay;
    FDN fdn;
    PitchShifter psL, psR;
    ReverseEngine revL, revR;
    LoFi lofiL, lofiR;
    DelayLine preL, preR;
    Allpass apL[4], apR[4];
    OnePoleHP hpL1, hpL2, hpR1, hpR2, shHpL, shHpR;
    Smoother mixS, delayMixS, shimS, revS, outS;
    static constexpr float kShimmerGain = 0.4f;

    float shimFbL = 0.0f, shimFbR = 0.0f;
    float preCur = 1200.0f, preTarget = 1200.0f, outGain = 1.0f;
};

} // namespace gv
