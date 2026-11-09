// Offline stability / behaviour test for the Expanse DSP engine (no JUCE needed).
#include "../Source/DSP/Engine.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace gv;

struct Result { float peak; float rmsFirst; float rmsLast; bool finite; };

static Result run (const char* name, Params p, double sr = 48000.0, float seconds = 30.0f,
                   int freezeAtSec = -1)
{
    Engine e;
    e.prepare (sr);
    e.setParams (p);

    const int block = 512;
    const int total = (int) (seconds * sr);
    std::vector<float> L (block), R (block);
    float peak = 0.0f; bool finite = true;
    double sumFirst = 0.0, sumLast = 0.0; long cntFirst = 0, cntLast = 0;
    unsigned rng = 12345;
    int pos = 0;

    while (pos < total)
    {
        if (freezeAtSec >= 0 && pos >= (int) (freezeAtSec * sr) && ! p.freeze)
        {
            p.freeze = true;
            e.setParams (p);
        }
        for (int i = 0; i < block; ++i)
        {
            const int t = pos + i;
            // 0.4 s decaying noisy burst around 220 Hz = a plucked-ish chord stab.
            float x = 0.0f;
            if (t < (int) (0.4 * sr))
            {
                rng = rng * 1664525u + 1013904223u;
                const float noise = ((rng >> 9) * (1.0f / 8388608.0f)) - 1.0f;
                const float env = std::exp (-6.0f * (float) t / (float) sr);
                x = env * (0.6f * std::sin (kTwoPi * 220.0f * (float) t / (float) sr)
                         + 0.3f * std::sin (kTwoPi * 330.0f * (float) t / (float) sr)
                         + 0.1f * noise);
            }
            L[(size_t) i] = x;
            R[(size_t) i] = x;
        }
        e.setParams (p);
        e.process (L.data(), R.data(), block);
        for (int i = 0; i < block; ++i)
        {
            const float a = L[(size_t) i], b = R[(size_t) i];
            if (! std::isfinite (a) || ! std::isfinite (b)) finite = false;
            peak = std::max ({ peak, std::fabs (a), std::fabs (b) });
            const double t = (double) (pos + i) / sr;
            if (t >= 1.0 && t < 3.0) { sumFirst += a * a; ++cntFirst; }
            if (t >= seconds - 4.0) { sumLast += a * a; ++cntLast; }
        }
        pos += block;
    }
    Result r { peak, (float) std::sqrt (sumFirst / std::max (1L, cntFirst)),
               (float) std::sqrt (sumLast / std::max (1L, cntLast)), finite };
    std::printf ("%-34s peak=%.3f rms[1-3s]=%.5f rms[last 4s]=%.6f finite=%d\n",
                 name, r.peak, r.rmsFirst, r.rmsLast, (int) r.finite);
    return r;
}

int main()
{
    bool ok = true;
    auto check = [&] (bool c, const char* msg) { if (! c) { std::printf ("  FAIL: %s\n", msg); ok = false; } };

    Params base; base.mix = 0.5f;

    { auto r = run ("default (8s decay)", base); check (r.finite && r.peak < 2.0f, "bounded"); check (r.rmsLast < r.rmsFirst * 0.1f, "decays"); }

    { Params p = base; p.decay = 60.0f; p.size = 2.5f; p.damping = 0.1f;
      auto r = run ("huge (60s, size 2.5)", p); check (r.finite && r.peak < 2.0f, "bounded"); }

    { Params p = base; p.decay = 20.0f; p.shimmer = 1.0f; p.shimmerSemis = 12.0f;
      auto r = run ("shimmer max +12", p); check (r.finite && r.peak < 3.0f, "shimmer bounded");
      check (r.rmsLast < r.rmsFirst, "shimmer tail eventually decays"); }

    { Params p = base; p.shimmer = 1.0f; p.shimmerSemis = -12.0f; p.decay = 20.0f;
      auto r = run ("shimmer max -12", p); check (r.finite && r.peak < 3.0f, "shimmer down bounded");
      check (r.rmsLast < r.rmsFirst, "shimmer-down tail eventually decays"); }

    { Params p = base; p.reverse = 1.0f; p.reverseTimeMs = 1200.0f;
      auto r = run ("reverse full", p); check (r.finite && r.peak < 2.0f, "reverse bounded"); }

    { Params p = base; p.lofi = 1.0f;
      auto r = run ("lofi full", p); check (r.finite && r.peak < 2.0f, "lofi bounded"); }

    { Params p = base; p.delayMix = 0.8f; p.delayFeedback = 0.95f; p.delayTimeMs = 500.0f;
      auto r = run ("delay fb 0.95", p); check (r.finite && r.peak < 3.0f, "delay bounded"); }

    { Params p = base; p.decay = 6.0f;
      auto r = run ("freeze engaged at 2s", p, 48000.0, 30.0f, 2);
      check (r.finite && r.peak < 2.0f, "freeze bounded");
      check (r.rmsLast > r.rmsFirst * 0.2f, "freeze sustains"); }

    { Params p = base; p.shimmer = 0.8f; p.reverse = 0.5f; p.lofi = 0.6f; p.delayMix = 0.5f; p.decay = 30.0f;
      auto r = run ("everything at once", p, 96000.0, 20.0f, 3); check (r.finite && r.peak < 3.0f, "bounded @96k"); }

    std::printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
