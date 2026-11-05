// Stand-alone checks for the RiseKit engine. No JUCE needed.
// Build:  c++ -std=c++17 -O2 -I Source tests/dsp_test.cpp -o dsp_test
#include "RiseDSP.h"

#include <chrono>
#include <cstdio>
#include <random>

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("FAIL: %s\n", msg); ++failures; } else std::printf ("ok:   %s\n", msg); } while (0)

int main()
{
    const double sr = 48000.0;
    const int block = 512;
    const int seconds = 20;

    std::mt19937 rng (1);
    std::uniform_real_distribution<float> dist (-0.5f, 0.5f);

    std::vector<float> L (block), R (block), inL (block), inR (block);

    // 1) Smile at 0 is a perfect pass-through.
    {
        rise::Engine e; e.prepare (sr);
        rise::Settings s; s.smile = 0.0f;
        bool identical = true;
        for (int b = 0; b < 50; ++b)
        {
            for (int i = 0; i < block; ++i) { inL[i] = L[i] = dist (rng); inR[i] = R[i] = dist (rng); }
            e.process (L.data(), R.data(), block, s);
            for (int i = 0; i < block; ++i) if (L[i] != inL[i] || R[i] != inR[i]) identical = false;
        }
        CHECK (identical && e.isIdle(), "smile 0 = untouched audio, engine idle");
    }

    // 2) Full sweep with every effect maxed: finite and below 1.0 with safety clip.
    {
        rise::Engine e; e.prepare (sr);
        rise::Settings s;
        s.filter = s.resonance = s.phaser = s.reverb = s.drive = s.width = s.chop = 1.0f;
        s.rideDb = 12.0f;
        bool finite = true; float peak = 0.0f;
        const int blocks = (int) (sr * seconds / block);
        for (int b = 0; b < blocks; ++b)
        {
            s.smile = (float) b / (float) blocks;
            s.ppq = b * block * (s.bpm / 60.0) / sr;
            for (int i = 0; i < block; ++i) { L[i] = dist (rng) * 1.6f; R[i] = dist (rng) * 1.6f; }
            e.process (L.data(), R.data(), block, s);
            for (int i = 0; i < block; ++i)
            {
                if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) finite = false;
                peak = std::max ({ peak, std::abs (L[i]), std::abs (R[i]) });
            }
        }
        CHECK (finite, "all effects maxed: no NaN/inf");
        std::printf ("      peak = %.4f\n", peak);
        CHECK (peak <= 1.0f, "safety clip keeps peaks <= 1.0");
    }

    // 3) Reverb stays stable when left at full smile for a long time with silence after.
    {
        rise::Engine e; e.prepare (sr);
        rise::Settings s; s.smile = 1.0f; s.reverb = 1.0f; s.safetyClip = false;
        float lastPeak = 0.0f;
        for (int b = 0; b < (int) (sr * 30 / block); ++b)
        {
            const bool impulse = b < 20;
            for (int i = 0; i < block; ++i) { L[i] = impulse ? dist (rng) : 0.0f; R[i] = L[i]; }
            e.process (L.data(), R.data(), block, s);
            lastPeak = 0.0f;
            for (int i = 0; i < block; ++i) lastPeak = std::max (lastPeak, std::abs (L[i]));
        }
        std::printf ("      tail after 30 s = %.8f\n", lastPeak);
        CHECK (lastPeak < 1.0e-3f, "reverb tail decays (stable feedback)");
    }

    // 4) Dropping the smile to 0 returns the engine to idle quickly.
    {
        rise::Engine e; e.prepare (sr);
        rise::Settings s; s.smile = 1.0f;
        for (int b = 0; b < 100; ++b) { for (int i = 0; i < block; ++i) L[i] = R[i] = dist (rng); e.process (L.data(), R.data(), block, s); }
        s.smile = 0.0f;
        int blocksToIdle = 0;
        while (! e.isIdle() && blocksToIdle < 100)
        {
            for (int i = 0; i < block; ++i) L[i] = R[i] = dist (rng);
            e.process (L.data(), R.data(), block, s);
            ++blocksToIdle;
        }
        std::printf ("      idle after %d blocks (%.1f ms)\n", blocksToIdle, blocksToIdle * block * 1000.0 / sr);
        CHECK (blocksToIdle <= 6, "drop returns to dry within ~60 ms");
    }

    // 5) Auto rise reaches 1.0 after the chosen length, auto drop snaps back.
    {
        rise::AutoRamp ramp;
        const double bpm = 150.0, lengthBeats = 32.0;          // 8 bars of 4/4
        const int blocksFor8Bars = (int) std::ceil (lengthBeats * 60.0 / bpm * sr / block);
        float v = 0.0f;
        for (int b = 0; b < blocksFor8Bars - 1; ++b) v = ramp.advance (true, bpm, block, sr, lengthBeats, 0.0f, false);
        CHECK (v > 0.99f && v < 1.0f, "auto rise almost at the top just before the end");
        v = ramp.advance (true, bpm, block, sr, lengthBeats, 0.0f, false);
        CHECK (v == 1.0f, "auto rise at 1.0 after 8 bars");
        v = ramp.advance (true, bpm, block, sr, lengthBeats, 0.0f, true);
        CHECK (v == 0.0f, "auto drop snaps to 0 at the end");
        v = ramp.advance (false, bpm, block, sr, lengthBeats, 0.0f, false);
        CHECK (v == 0.0f && ramp.getProgress() == 0.0f, "rise off resets");
    }

    // 6) CPU: worst case (everything on) vs real time.
    {
        rise::Engine e; e.prepare (sr);
        rise::Settings s; s.smile = 0.8f;
        s.filter = s.resonance = s.phaser = s.reverb = s.drive = s.width = s.chop = 1.0f;
        for (int i = 0; i < block; ++i) { L[i] = dist (rng); R[i] = dist (rng); }
        const int blocks = (int) (sr * 60 / block);              // one minute of audio
        auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) e.process (L.data(), R.data(), block, s);
        auto t1 = std::chrono::steady_clock::now();
        const double secs = std::chrono::duration<double> (t1 - t0).count();
        std::printf ("      60 s of stereo audio processed in %.1f ms  (%.3f%% of one core)\n",
                     secs * 1000.0, secs / 60.0 * 100.0);
        CHECK (secs < 6.0, "worst case uses well under 10% of one core");
    }

    std::printf (failures == 0 ? "\nAll checks passed.\n" : "\n%d check(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
