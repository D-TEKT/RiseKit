// RiseDSP.h — the RiseKit build-up engine.
// Plain C++17, no JUCE, so it can be tested on its own (see tests/dsp_test.cpp).
//
// Chain:  drive → high-pass → phaser → width → delay → reverb → wash → chop → ride → mix → clip
//
// CPU notes:
//  * When the smile is at 0 the engine does nothing at all (pure pass-through).
//  * Expensive maths (tan, pow, sin) runs once per 32-sample sub-block, not per sample.
//  * Effects that are turned down are skipped completely.
//  * Reverb: 8-line feedback delay network (no convolution), delay: two interpolated lines.
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
enum ReverbType { Hall = 0, Plate, Room, Shimmer, Infinite, kNumReverbTypes };

// Delay note lengths in quarter-note beats: 1/2, 1/4., 1/4, 1/4T, 1/8., 1/8, 1/8T, 1/16
inline constexpr std::array<double, 8> kDelayBeats { 2.0, 1.5, 1.0, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0, 0.25 };

//==============================================================================
// Everything the engine needs for one block. The plugin fills this from its
// parameters and the host play head.
struct Settings
{
    float smile      = 0.0f;  // final macro 0..1 (manual knob combined with the auto rise)

    float filter     = 0.8f;  // how far the high-pass climbs at full smile
    float resonance  = 0.3f;
    float phaser     = 0.5f;
    float drive      = 0.3f;
    float width      = 0.6f;
    float chop       = 0.0f;  // tempo-synced gate that speeds up near the end

    float reverb     = 0.6f;  // reverb level at full smile
    float decay      = 0.6f;  // how long the reverb gets at full smile
    int   reverbType = Hall;
    float delay      = 0.4f;  // echo level at full smile
    float feedback   = 0.6f;  // echo feedback at full smile (it rises with the smile)
    int   delayTime  = 4;     // index into kDelayBeats
    bool  pingPong   = true;
    float wash       = 0.8f;  // how much of the dry signal is replaced by the wet at full smile

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
// Power-of-two circular buffer. Always read() before push() within one sample:
// read(1) is the previous sample, read(L) is L samples ago.
class DelayLine
{
public:
    void allocate (int maxDelaySamples)
    {
        size_t n = 1;
        while (n < (size_t) maxDelaySamples + 4) n <<= 1;
        buf.assign (n, 0.0f);
        mask = (uint32_t) n - 1;
        w = 0;
    }

    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

    void push (float x) noexcept { buf[w] = x; w = (w + 1) & mask; }

    float read (int d) const noexcept { return buf[(w - (uint32_t) d) & mask]; }

    float readFrac (float d) const noexcept
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = buf[(w - (uint32_t) i) & mask];
        const float b = buf[(w - (uint32_t) i - 1u) & mask];
        return a + f * (b - a);
    }

    int capacity() const noexcept { return (int) buf.size() - 4; }

private:
    std::vector<float> buf { 0.0f, 0.0f, 0.0f, 0.0f };
    uint32_t mask = 3, w = 0;
};

//==============================================================================
// Octave-up pitch shifter (two crossfaded taps). Used for the Shimmer reverb.
class OctaveUp
{
public:
    void prepare (float sr)
    {
        window = 0.05f * sr;
        line.allocate ((int) window + 8);
        clear();
    }

    void clear() noexcept { line.clear(); phase = 0.0f; }

    float process (float x) noexcept
    {
        phase += 1.0f / window;
        if (phase >= 1.0f) phase -= 1.0f;
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;

        // Delay shrinks one sample per sample -> playback at 2x speed = one octave up.
        const float y = (1.0f - std::abs (2.0f * phase - 1.0f)) * line.readFrac (1.0f + window * (1.0f - phase))
                      + (1.0f - std::abs (2.0f * p2    - 1.0f)) * line.readFrac (1.0f + window * (1.0f - p2));
        line.push (x);
        return y;
    }

private:
    DelayLine line;
    float window = 2400.0f, phase = 0.0f;
};

//==============================================================================
// 8-line feedback delay network with input diffusion, damping and slow modulation.
class Reverb
{
public:
    // hiGain: how much of the highs survive each pass (a shelf, so the lows decay only by rt60)
    struct TypeDef { float size, rtBase, rtSpan, dampHz, hiGain, modDepth, diffusion, shimmer, level; };

    static const TypeDef& def (int type) noexcept
    {
        static const TypeDef defs[kNumReverbTypes] =
        {
            //  size   rtBase rtSpan dampHz   hiGain mod    diff   shim  level
            { 1.00f,  1.2f, 14.0f, 4000.0f, 0.65f, 6.0f,  0.70f, 0.0f, 2.20f },   // Hall
            { 0.50f,  0.8f,  7.0f, 7000.0f, 0.80f, 3.0f,  0.75f, 0.0f, 1.70f },   // Plate
            { 0.35f,  0.4f,  2.6f, 3000.0f, 0.40f, 2.0f,  0.65f, 0.0f, 3.30f },   // Room
            { 1.00f,  1.5f, 12.0f, 6000.0f, 0.75f, 6.0f,  0.70f, 1.0f, 1.80f },   // Shimmer
            { 1.25f,  2.0f, 60.0f, 3500.0f, 0.85f, 10.0f, 0.72f, 0.0f, 1.80f },   // Infinite
        };
        return defs[std::clamp (type, 0, (int) kNumReverbTypes - 1)];
    }

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        const float maxSize = 1.3f;
        for (int i = 0; i < 8; ++i)
            lines[i].allocate ((int) (kLineMs[i] * 0.001f * sr * maxSize) + 64);
        for (int i = 0; i < 4; ++i)
            diffusers[i].allocate ((int) (kDiffMs[i] * 0.001f * sr * maxSize) + 4);
        shifter.prepare (sr);
        setType (type, true);
    }

    int getType() const noexcept { return type; }

    void setType (int newType, bool force = false)
    {
        if (newType == type && ! force) return;
        type = std::clamp (newType, 0, (int) kNumReverbTypes - 1);

        const auto& d = def (type);
        for (int i = 0; i < 8; ++i)
            lineLen[i] = kLineMs[i] * 0.001f * sr * d.size;
        for (int i = 0; i < 4; ++i)
            diffLen[i] = std::max (1, (int) (kDiffMs[i] * 0.001f * sr * d.size));
        clear();
    }

    void clear() noexcept
    {
        for (auto& l : lines) l.clear();
        for (auto& l : diffusers) l.clear();
        lp.fill (0.0f);
        shifter.clear();
        shimmerHp = 0.0f;
    }

    // Once per sub-block.
    void update (float smile, float decay, int subBlock) noexcept
    {
        const auto& d = def (type);
        const float rt60 = d.rtBase + d.rtSpan * (0.25f + 0.75f * std::clamp (decay, 0.0f, 1.0f)) * smile;
        for (int i = 0; i < 8; ++i)
            gain[i] = std::pow (10.0f, -3.0f * lineLen[i] / (rt60 * sr));

        dampCoef = std::exp (-2.0f * kPi * d.dampHz / sr);
        hiGain = d.hiGain;
        diffG = d.diffusion;
        shimmerGain = d.shimmer * 0.3f * smile;

        const float depth = d.modDepth * sr / 48000.0f;
        for (int i = 0; i < 8; ++i)
        {
            modPhase[i] += kModHz[i] * (float) subBlock / sr;
            modPhase[i] -= std::floor (modPhase[i]);
            readLen[i] = lineLen[i] + 1.0f + depth * 0.5f * (1.0f + std::sin (2.0f * kPi * modPhase[i]));
        }
    }

    void tick (float in, float& outL, float& outR) noexcept
    {
        // Input diffusion (series allpasses)
        float x = in;
        for (int k = 0; k < 4; ++k)
        {
            const float z = diffusers[k].read (diffLen[k]);
            const float y = -diffG * x + z;
            diffusers[k].push (x + diffG * y);
            x = y;
        }

        // Read + damp
        float o[8];
        float sum = 0.0f;
        for (int i = 0; i < 8; ++i)
        {
            const float v = lines[i].readFrac (readLen[i]);
            lp[i] = v + (lp[i] - v) * dampCoef;          // low-passed copy
            o[i] = lp[i] + hiGain * (v - lp[i]);         // high shelf: lows untouched
            sum += o[i];
        }

        // Shimmer: octave-up of the tail fed back in (high-passed so it stays airy)
        float sh = 0.0f;
        if (shimmerGain > 0.0f)
        {
            float s = shifter.process (sum * 0.25f);
            shimmerHp += 0.05f * (s - shimmerHp);
            sh = (s - shimmerHp) * shimmerGain;
        }

        // Householder mixing: h = o - (2/N) * sum
        const float hs = sum * 0.25f;
        const float inject = x * 0.35f;
        for (int i = 0; i < 8; ++i)
        {
            const float sign = (i & 1) ? -1.0f : 1.0f;
            lines[i].push ((inject + sh) * sign + (o[i] - hs) * gain[i]);
        }

        outL = 0.5f * (o[0] - o[2] + o[4] - o[6]);
        outR = 0.5f * (o[1] - o[3] + o[5] - o[7]);
    }

    float level() const noexcept { return def (type).level; }

private:
    static constexpr float kLineMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 61.7f, 71.9f, 79.3f };
    static constexpr float kDiffMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
    static constexpr float kModHz[8]  = { 0.11f, 0.17f, 0.23f, 0.31f, 0.37f, 0.43f, 0.53f, 0.61f };

    float sr = 48000.0f;
    int type = Hall;

    std::array<DelayLine, 8> lines;
    std::array<DelayLine, 4> diffusers;
    std::array<float, 8> lineLen {}, readLen {}, gain {}, lp {}, modPhase { 0.0f, 0.13f, 0.29f, 0.41f, 0.57f, 0.66f, 0.78f, 0.91f };
    std::array<int, 4> diffLen { 1, 1, 1, 1 };
    float dampCoef = 0.5f, hiGain = 0.6f, diffG = 0.7f, shimmerGain = 0.0f, shimmerHp = 0.0f;
    OctaveUp shifter;
};

//==============================================================================
// Tempo-synced stereo / ping-pong delay with filtered feedback.
class Echo
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        left.allocate ((int) (4.1f * sr));
        right.allocate ((int) (4.1f * sr));
        hpCoef = 1.0f - std::exp (-2.0f * kPi * 180.0f / sr);
        lpCoef = 1.0f - std::exp (-2.0f * kPi * 6500.0f / sr);
        clear();
    }

    void clear() noexcept
    {
        left.clear(); right.clear();
        hpL = hpR = lpL = lpR = 0.0f;
        delaySmoothed = -1.0f;
    }

    // Once per sub-block.
    void setDelay (float samples) noexcept
    {
        samples = std::clamp (samples, 2.0f, (float) left.capacity() - 2.0f);
        if (delaySmoothed < 0.0f) delaySmoothed = samples;
        else delaySmoothed += 0.15f * (samples - delaySmoothed);
    }

    void tick (float inL, float inR, float fb, bool pingPong, float& outL, float& outR) noexcept
    {
        const float rL = left.readFrac (delaySmoothed);
        const float rR = right.readFrac (delaySmoothed);

        const float fL = filter (rL, hpL, lpL);
        const float fR = filter (rR, hpR, lpR);

        if (pingPong)
        {
            left.push (0.5f * (inL + inR) + fb * fR);
            right.push (fb * fL);
        }
        else
        {
            left.push (inL + fb * fL);
            right.push (inR + fb * fR);
        }

        outL = rL;
        outR = rR;
    }

private:
    float filter (float x, float& hpState, float& lpState) const noexcept
    {
        hpState += hpCoef * (x - hpState);
        const float hp = x - hpState;
        lpState += lpCoef * (hp - lpState);
        return lpState;
    }

    float sr = 48000.0f;
    DelayLine left, right;
    float hpCoef = 0.0f, lpCoef = 1.0f, hpL = 0.0f, hpR = 0.0f, lpL = 0.0f, lpR = 0.0f;
    float delaySmoothed = -1.0f;
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
        reverb.prepare (sr);
        echo.prepare (sr);
        reset();
    }

    void reset() noexcept
    {
        smileSmoothed = 0.0f;
        idle = true;
        clearStates();
    }

    bool isIdle() const noexcept { return idle; }
    float getSmoothedSmile() const noexcept { return smileSmoothed; }

    // In-place stereo processing.
    void process (float* left, float* right, int numSamples, const Settings& s) noexcept
    {
        const float target = std::clamp (s.smile, 0.0f, 1.0f);
        const float outGain = dbToGain (s.outputDb);

        if (s.reverbType != reverb.getType())
            reverb.setType (s.reverbType);

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

        const int delayIndex = std::clamp (s.delayTime, 0, (int) kDelayBeats.size() - 1);
        const float delaySamples = (float) (kDelayBeats[(size_t) delayIndex] * 60.0 / std::max (s.bpm, 20.0) * sr);

        for (int start = 0; start < numSamples; start += kSubBlock)
        {
            const int n = std::min (kSubBlock, numSamples - start);

            // Coefficients use the macro value at the end of this sub-block.
            const float mEnd = target + (smileSmoothed - target) * std::pow (smoothCoef, (float) n);
            updateCoefficients (s, mEnd);

            // Turn the reverb/delay on with a clean buffer when they come back in.
            const bool echoOn = s.delay * mEnd > 1.0e-4f || s.delay * smileSmoothed > 1.0e-4f;
            const bool verbOn = s.reverb * mEnd > 1.0e-4f || s.reverb * smileSmoothed > 1.0e-4f;
            if (echoOn && ! echoActive) echo.clear();
            if (verbOn && ! verbActive) reverb.clear();
            echoActive = echoOn;
            verbActive = verbOn;

            if (echoOn)
            {
                echo.setDelay (delaySamples);
                echoFeedback = std::min (0.95f, s.feedback * (0.35f + 0.6f * mEnd));
            }
            if (verbOn)
                reverb.update (mEnd, s.decay, n);

            const float verbLevel = reverb.level();

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

                // 5) Delay
                const float da = s.delay * m;
                float eL = 0.0f, eR = 0.0f;
                if (echoActive)
                    echo.tick (l, r, echoFeedback, s.pingPong, eL, eR);

                // 6) Reverb (the echoes are fed in too, so they smear into the wash)
                const float ra = s.reverb * m;
                float vL = 0.0f, vR = 0.0f;
                if (verbActive)
                    reverb.tick (0.5f * (l + r) + 0.5f * da * (eL + eR), vL, vR);

                // 7) Wash: the dry fades out as the wet takes over (only as far as there is wet)
                const float wetPresence = std::min (1.0f, 1.5f * std::max (ra, da));
                const float dryGain = 1.0f - std::clamp (s.wash, 0.0f, 1.0f) * m * std::sqrt (m) * wetPresence;
                const float washBoost = 1.0f + 0.8f * (1.0f - dryGain);     // wet comes up as the dry goes
                l = l * dryGain + (eL * da * 0.9f + vL * ra * verbLevel) * washBoost;
                r = r * dryGain + (eR * da * 0.9f + vR * ra * verbLevel) * washBoost;

                // 8) Chop (tempo-synced gate, speeds up as the smile rises)
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

                // 9) Level ride
                if (s.rideDb != 0.0f)
                {
                    const float rg = fastDbToGain (s.rideDb * m);
                    l *= rg;
                    r *= rg;
                }

                // 10) Global mix
                if (s.mix < 1.0f)
                {
                    l = dryL + s.mix * (l - dryL);
                    r = dryR + s.mix * (r - dryR);
                }

                // 11) Safety clip (only touches peaks above about -1 dBFS)
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
            for (auto& st : z)
            {
                const float out = a * y + st;
                st = y - a * out;
                y = out;
            }
            fb = y;
            return y;
        }
        void clear() noexcept { z.fill (0.0f); fb = 0.0f; }
    };

    //------------------------------------------------------------------------------
    void updateCoefficients (const Settings& s, float m) noexcept
    {
        // High-pass: 20 Hz at rest, up to ~20 Hz * 300^filter (≈6 kHz) at full smile.
        const float maxRatio = std::pow (300.0f, std::clamp (s.filter, 0.0f, 1.0f));
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

    static float softClip (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.9f) return x;
        const float y = 0.9f + 0.1f * fastTanh ((a - 0.9f) * 10.0f);
        return x < 0.0f ? -y : y;
    }

    // 10^(db/20) via a short exp series, accurate across the ±12 dB ride range.
    static float fastDbToGain (float db) noexcept
    {
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
        reverb.clear();
        echo.clear();
        echoActive = verbActive = false;
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

    Reverb reverb;
    Echo echo;
    bool echoActive = false, verbActive = false;
    float echoFeedback = 0.0f;

    float gateState = 1.0f;
    double beatPos = 0.0;
};

} // namespace rise
