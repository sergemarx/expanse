# Expanse

A big, soaring ambient reverb for guitar. JUCE plugin (VST3, AU on macOS, Standalone).

## Build

Requires CMake 3.22+ and a C++17 compiler (Visual Studio 2022, Xcode, or GCC/Clang). JUCE 8.0.4 is fetched automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Built plugins are copied to your system plugin folders (`COPY_PLUGIN_AFTER_BUILD`). Before shipping, change `COMPANY_NAME`, `PLUGIN_MANUFACTURER_CODE` and `PLUGIN_CODE` in `CMakeLists.txt`.

Offline DSP test (no JUCE needed): `g++ -std=c++17 -O2 Tests/engine_test.cpp -o engine_test && ./engine_test`

## Signal flow

```
in -> ping-pong delay -> low cut -> pre-delay -> 4x allpass diffusion -+-> 8-line FDN reverb
                                                                       |        |
                          shimmer feedback (pitch shift, HP, tanh) <---+--------+
FDN out -> reverse blend -> lo-fi -> width -> mix -> output
```

## Controls

| Section | Control | What it does |
|---|---|---|
| Space | Mix, Decay (0.3-60 s), Size, Pre-Delay, Damping, Modulation, Diffusion, Low Cut | The core reverb. Size scales the delay lines, Modulation detunes them for a chorused wash. |
| Tails | Shimmer + Shimmer Pitch | Pitch-shifted tail is fed back into the reverb (down an octave, 4th, 5th, octave, octave+5th). |
| Tails | Reverse + Rev Time | Blends in a reversed copy of the tail (200-2000 ms windows). |
| Tails | Freeze | Closes the input and holds the current tail indefinitely. |
| Tails | Lo-Fi | Tape wobble, sample-rate and bit reduction, saturation and darkening on the wet signal. |
| Delay | Delay Mix, Time, Feedback | Stereo ping-pong echoes that also feed the reverb. |
| Output | Width, Output | Stereo width of the wet signal and output gain. |

## Ideas for next steps

- Tempo-synced delay time and shimmer/reverse time.
- Presets (APVTS state is already saved with the host session).
- Higher-quality pitch shifter (phase vocoder) for cleaner shimmer.
- Ducking of the wet signal while you play.
- Custom knob graphics and a level meter.

## Status

The DSP engine (`Source/DSP`) was compiled and stability-tested offline across all modes (including freeze and maximum shimmer/delay feedback) at 48 kHz and 96 kHz. The JUCE wrapper and editor have not been built yet, so expect to fix a small compile error or two on the first build.
