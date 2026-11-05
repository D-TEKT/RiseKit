// RiseDSP.h — the RiseKit build-up engine.
// Plain C++17, no JUCE, so it can be tested on its own (see tests/dsp_test.cpp).
//
// CPU notes:
//  * When the smile is at 0 the engine does nothing at all (pure pass-through).
//  * Expensive maths (tan, pow) runs once per 32-sample sub-block, not per sample.
//  * No oversampling, no FFT, no heavy reverb: a 4-line feedback delay network
//    with two diffusers, a 4-stage phaser and a cheap rational tanh.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace rise
{

constexpr float kPi = 3.14159265358979f;

// Rational tanh approximation, exact at |x| = 3 (returns ±1), smooth everywhere.
inline float fastTanh (float x) noexcept
{
    x = std::clamp (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

//==============================================================================
// Everything the engine needs for one block. The plugin fills this from its
// parameters and the host play head.
struct Settings
{
    float smile      = 0.0f;  // final macro 0..1 (manual knob combined with the auto rise)
    float filter     = 0.8f;  // how far the high-pass climbs at full smile
    float resonance  = 0.3f;
    float phaser     = 0.5f;
    float reverb     = 0.5f;
    float drive      = 0.3f;
    float width      = 0.6f;
    float chop       = 0.0f;  // tempo-synced gate that speeds up near the end
    float rideDb     = 2.0f;  // level change at full smile
    float mix        = 1.0f;
    float outputDb   = 0.0f;
    bool  safetyClip = true;

    double bpm = 128.0;
    double ppq = -1.0;        // host position in quarter notes, < 0 if unknown
};

//==============================================================================
// Tempo-synced rise: while `on`, climbs from 0 to 1 over `lengthBeats`.
class AutoRamp
{
public:
    // curve: -1 (fast start) .. 0 (linear) .. +1 (slow start, fast end)
    float advance (bool on, double bpm, int numSamples, double sampleRate,
                   double lengthBeats, float curve, bool autoDrop) noexcept
    {
        if (! on)
        {
            beats = 0.0;
            progress = 0.0f;
            return 0.0f;
        }

        beats += numSamples * (std::max (bpm, 1.0) / 60.0) / sampleRate;
        const double t = std::clamp (beats / std::max (lengthBeats, 0.25), 0.0, 1.0);
        progress = (float) t;

        if (autoDrop && beats >= lengthBeats)
            return 0.0f;                                   // the "drop": snap back to dry

        const float exponent = std::pow (3.0f, std::clamp (curve, -1.0f, 1.0f));
        return std::pow ((float) t, exponent);
    }

    float getProgress() const noexcept { return progress; }
    void  reset() noexcept { beats = 0.0; progress = 0.0f; }

private:
    double beats = 0.0;
    float progress = 0.0f;
};

//==============================================================================
class Engine
{
public:
    void prepare (double newSampleRate)
    {
        sr = (float) newSampleRate;
        smoothCoef = std::exp (-1.0f / (0.004f * sr));        // ~4 ms declick on the macro
        gateCoef   = std::exp (-1.0f / (0.0015f * sr));       // ~1.5 ms gate edges

        // Reverb delay lines (lengths in ms, mutually prime-ish), scaled to the sample rate.
        const float fdnMs[4]  = { 37.1f, 43.9f, 53.3f, 61.7f };
        for (int i = 0; i < 4; ++i)
            fdn[i].setSize ((int) (fdnMs[i] * 0.001f * sr) + 1);

        diff[0].setSize ((int) (0.0047f * sr) + 1);
        diff[1].setSize ((int) (0.0113f * sr) + 1);

        // One-pole damping in the reverb loop (~6.5 kHz).
        dampCoef = std::exp (-2.0f * kPi * 6500.0f / sr);

        reset();
    }

    void reset() noexcept
    {
        smileSmoothed = 0.0f;
        idle = true;
        clearStates();
    }

    bool isIdle() const noexcept { return idle; }

    // In-place stereo processing.
    void process (float* left, float* right, int numSamples, const Settings& s) noexcept
    {
        const float target = std::clamp (s.smile, 0.0f, 1.0f);
        const float outGain = dbToGain (s.outputDb);

        // ---- Fully idle: pass-through, zero DSP cost ---------------------------------
        if (target <= 0.0f && smileSmoothed < 1.0e-4f)
        {
            if (! idle)
            {
                idle = true;
                smileSmoothed = 0.0f;
                clearStates();      // drop the tails so the next build starts clean
            }

            applyGain (left, right, numSamples, outGain);
            advanceBeat (s, numSamples);
            return;
        }

        idle = false;

        const double beatsPerSample = std::max (s.bpm, 1.0) / 60.0 / (double) sr;
        if (s.ppq >= 0.0)
            beatPos = s.ppq;

        for (int start = 0; start < numSamples; start += kSubBlock)
        {
            const int n = std::min (kSubBlock, numSamples - start);

            // Run the macro smoother across the sub-block, use the end value for coefficients.
            const float smileEnd = target + (smileSmoothed - target) * std::pow (smoothCoef, (float) n);
            updateCoefficients (s, smileEnd);

            for (int i = 0; i < n; ++i)
            {
                smileSmoothed = target + (smileSmoothed - target) * smoothCoef;
                const float m = smileSmoothed;

                float l = left [start + i];
                float r = right[start + i];
                const float dryL = l, dryR = r;

                // 1) Drive
                const float dAmt = s.drive * m;
                if (dAmt > 1.0e-4f)
                {
                    const float g = 1.0f + dAmt * dAmt * 15.0f;
                    const float comp = 1.0f / std::sqrt (g);
                    const float dm = std::min (1.0f, dAmt * 1.5f);
                    l += dm * (fastTanh (l * g) * comp - l);
                    r += dm * (fastTanh (r * g) * comp - r);
                }

                // 2) High-pass (TPT state-variable filter)
                l = hpL.process (l, hpA1, hpA2, hpA3, hpK);
                r = hpR.process (r, hpA1, hpA2, hpA3, hpK);

                // 3) Phaser
                const float pm = s.phaser * m;
                if (pm > 1.0e-4f)
                {
                    const float apL = phaserL.process (l, phaserCoefL);
                    const float apR = phaserR.process (r, phaserCoefR);
                    l += pm * 0.5f * (apL - l);
                    r += pm * 0.5f * (apR - r);
                }

                // 4) Stereo width (mid/side)
                const float w = s.width * m;
                if (w > 1.0e-4f)
                {
                    const float mid  = 0.5f * (l + r) * (1.0f - 0.25f * w);
                    const float side = 0.5f * (l - r) * (1.0f + 1.5f * w);
                    l = mid + side;
                    r = mid - side;
                }

                // 5) Reverb wash
                const float rv = s.reverb * m;
                if (rv > 1.0e-4f)
                {
                    float wetL, wetR;
                    reverbTick (0.5f * (l + r), wetL, wetR);
                    const float dryKeep = 1.0f - 0.3f * rv;
                    const float wet = 0.75f * rv;
                    l = l * dryKeep + wetL * wet;
                    r = r * dryKeep + wetR * wet;
                }

                // 6) Chop (tempo-synced gate, speeds up as the smile rises)
                const float c = s.chop * m * m * m;
                if (c > 1.0e-4f)
                {
                    const double div = m < 0.5f ? 1.0 : (m < 0.75f ? 2.0 : (m < 0.92f ? 4.0 : 8.0));
                    const double ph = beatPos * div;
                    const float open = (ph - std::floor (ph)) < 0.5 ? 1.0f : 0.0f;
                    gateState = open + (gateState - open) * gateCoef;
                    const float gg = 1.0f - c * (1.0f - gateState);
                    l *= gg;
                    r *= gg;
                }
                else
                {
                    gateState = 1.0f;
                }

                // 7) Level ride
                if (s.rideDb != 0.0f)
                {
                    // cheap approximation of dB->gain across the ride range
                    const float rg = fastDbToGain (s.rideDb * m);
                    l *= rg;
                    r *= rg;
                }

                // 8) Global mix
                if (s.mix < 1.0f)
                {
                    l = dryL + s.mix * (l - dryL);
                    r = dryR + s.mix * (r - dryR);
                }

                // 9) Safety clip (only touches peaks above -0.9 dBFS-ish)
                if (s.safetyClip)
                {
                    l = softClip (l);
                    r = softClip (r);
                }

                left [start + i] = l * outGain;
                right[start + i] = r * outGain;

                beatPos += beatsPerSample;
            }
        }
    }

    float getSmoothedSmile() const noexcept { return smileSmoothed; }

private:
    static constexpr int kSubBlock = 32;

    //------------------------------------------------------------------------------
    struct SVF
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        float process (float x, float a1, float a2, float a3, float k) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return x - k * v1 - v2;              // high-pass output
        }
        void clear() noexcept { ic1 = ic2 = 0.0f; }
    };

    struct Phaser4
    {
        std::array<float, 4> z {};
        float fb = 0.0f;
        float process (float x, float a) noexcept
        {
            float y = x + 0.3f * fb;
            for (auto& s : z)
            {
                const float out = a * y + s;
                s = y - a * out;
                y = out;
            }
            fb = y;
            return y;
        }
        void clear() noexcept { z.fill (0.0f); fb = 0.0f; }
    };

    struct Delay
    {
        std::vector<float> buf;
        int pos = 0;
        void setSize (int n) { buf.assign ((size_t) std::max (n, 2), 0.0f); pos = 0; }
        int size() const noexcept { return (int) buf.size(); }
        float read() const noexcept { return buf[(size_t) pos]; }
        void write (float v) noexcept { buf[(size_t) pos] = v; if (++pos >= (int) buf.size()) pos = 0; }
        void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
    };

    //------------------------------------------------------------------------------
    void updateCoefficients (const Settings& s, float m) noexcept
    {
        // High-pass: 20 Hz at rest, up to ~20 Hz * 150^filter (≈3 kHz) at full smile.
        const float maxRatio = std::pow (150.0f, std::clamp (s.filter, 0.0f, 1.0f));
        const float fc = std::min (20.0f * std::pow (maxRatio, std::pow (m, 1.2f)), 0.45f * sr);
        const float g = std::tan (kPi * fc / sr);
        hpK  = 2.0f - 1.8f * std::clamp (s.resonance, 0.0f, 1.0f) * m;
        hpA1 = 1.0f / (1.0f + g * (g + hpK));
        hpA2 = g * hpA1;
        hpA3 = g * hpA2;

        // Phaser LFO: speeds up with the smile. Right channel 90° behind for width.
        const float rate = 0.15f + 3.0f * m * m;
        lfoPhase += rate * (float) kSubBlock / sr;
        lfoPhase -= std::floor (lfoPhase);
        phaserCoefL = allpassCoef (sweepFreq (lfoPhase));
        phaserCoefR = allpassCoef (sweepFreq (lfoPhase + 0.25f));

        // Reverb decay grows from ~1 s to ~6 s.
        const float rt60 = 1.0f + 5.0f * m;
        for (int i = 0; i < 4; ++i)
            fdnGain[i] = std::pow (10.0f, -3.0f * (float) fdn[i].size() / (rt60 * sr));
    }

    float sweepFreq (float phase) const noexcept
    {
        const float tri = 1.0f - std::abs (2.0f * (phase - std::floor (phase)) - 1.0f); // 0..1
        return 250.0f * std::pow (12.0f, tri);   // 250 Hz .. 3 kHz
    }

    float allpassCoef (float f) const noexcept
    {
        const float t = std::tan (kPi * std::min (f, 0.45f * sr) / sr);
        return (t - 1.0f) / (t + 1.0f);
    }

    void reverbTick (float in, float& outL, float& outR) noexcept
    {
        // Two series allpass diffusers smear the input.
        float x = in;
        for (auto& d : diff)
        {
            const float z = d.read();
            const float y = -0.6f * x + z;
            d.write (x + 0.6f * y);
            x = y;
        }

        // 4-line FDN with Hadamard mixing and damping.
        float o[4];
        for (int i = 0; i < 4; ++i)
        {
            damp[i] = o[i] = fdn[i].read() * (1.0f - dampCoef) + damp[i] * dampCoef;
        }

        const float h0 = 0.5f * (o[0] + o[1] + o[2] + o[3]);
        const float h1 = 0.5f * (o[0] - o[1] + o[2] - o[3]);
        const float h2 = 0.5f * (o[0] + o[1] - o[2] - o[3]);
        const float h3 = 0.5f * (o[0] - o[1] - o[2] + o[3]);

        fdn[0].write (x + h0 * fdnGain[0]);
        fdn[1].write (x + h1 * fdnGain[1]);
        fdn[2].write (x + h2 * fdnGain[2]);
        fdn[3].write (x + h3 * fdnGain[3]);

        outL = 0.5f * (o[0] + o[2]);
        outR = 0.5f * (o[1] + o[3]);
    }

    static float softClip (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.9f) return x;
        const float y = 0.9f + 0.1f * fastTanh ((a - 0.9f) * 10.0f);
        return x < 0.0f ? -y : y;
    }

    // exp-based dB conversion is fine per sample on modern CPUs, but this keeps it lighter.
    static float fastDbToGain (float db) noexcept
    {
        // 10^(db/20) = e^(db * 0.115129). Second-order-accurate exp for |db| <= 12.
        const float x = db * 0.11512925f;
        const float h = x * 0.25f;                         // exp(x) = exp(x/4)^4
        float e = 1.0f + h * (1.0f + h * (0.5f + h * (1.0f / 6.0f)));
        e *= e;
        return e * e;
    }

    void applyGain (float* l, float* r, int n, float g) noexcept
    {
        if (g == 1.0f) return;
        for (int i = 0; i < n; ++i) { l[i] *= g; r[i] *= g; }
    }

    void advanceBeat (const Settings& s, int n) noexcept
    {
        beatPos = (s.ppq >= 0.0 ? s.ppq : beatPos) + n * std::max (s.bpm, 1.0) / 60.0 / (double) sr;
    }

    void clearStates() noexcept
    {
        hpL.clear(); hpR.clear();
        phaserL.clear(); phaserR.clear();
        for (auto& d : fdn) d.clear();
        for (auto& d : diff) d.clear();
        for (auto& v : damp) v = 0.0f;
        gateState = 1.0f;
    }

    //------------------------------------------------------------------------------
    float sr = 48000.0f;
    float smoothCoef = 0.99f, gateCoef = 0.99f;
    float smileSmoothed = 0.0f;
    bool idle = true;

    SVF hpL, hpR;
    float hpA1 = 1.0f, hpA2 = 0.0f, hpA3 = 0.0f, hpK = 2.0f;

    Phaser4 phaserL, phaserR;
    float phaserCoefL = 0.0f, phaserCoefR = 0.0f, lfoPhase = 0.0f;

    std::array<Delay, 4> fdn;
    std::array<Delay, 2> diff;
    std::array<float, 4> fdnGain { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 4> damp { 0.0f, 0.0f, 0.0f, 0.0f };
    float dampCoef = 0.5f;

    float gateState = 1.0f;
    double beatPos = 0.0;
};

} // namespace rise
