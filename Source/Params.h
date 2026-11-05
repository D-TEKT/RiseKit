// Params.h — parameter IDs and factory presets.
#pragma once

#include <array>

namespace params
{
    inline constexpr const char* smile     = "smile";
    inline constexpr const char* rise      = "rise";
    inline constexpr const char* length    = "length";
    inline constexpr const char* curve     = "curve";
    inline constexpr const char* autoDrop  = "autoDrop";
    inline constexpr const char* filter    = "filter";
    inline constexpr const char* resonance = "resonance";
    inline constexpr const char* phaser    = "phaser";
    inline constexpr const char* reverb    = "reverb";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* width     = "width";
    inline constexpr const char* chop      = "chop";
    inline constexpr const char* ride      = "ride";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* output    = "output";
    inline constexpr const char* safety    = "safety";

    // Length choices, in bars.
    inline constexpr std::array<int, 6> lengthBars { 1, 2, 4, 8, 16, 32 };
    inline constexpr int defaultLengthIndex = 3;    // 8 bars

    struct Preset
    {
        const char* name;
        float filter, resonance, phaser, reverb, drive, width, chop, ride, curve;
    };

    // Presets set the character only; Smile, Rise, Length, Mix and Output stay as they are.
    inline constexpr std::array<Preset, 10> presets
    {{
        //  name               filt  reso  phas  verb  drive width chop  ride  curve
        { "Classic Smile",     0.80f, 0.30f, 0.50f, 0.55f, 0.30f, 0.60f, 0.00f,  2.0f, 0.50f },
        { "Big Room Wash",     0.90f, 0.40f, 0.30f, 0.85f, 0.20f, 0.80f, 0.00f,  3.0f, 0.50f },
        { "Hardstyle Lift",    0.85f, 0.50f, 0.20f, 0.50f, 0.60f, 0.50f, 0.40f,  2.0f, 0.60f },
        { "Clean Lift",        0.60f, 0.10f, 0.00f, 0.30f, 0.00f, 0.50f, 0.00f,  1.0f, 0.30f },
        { "Chop Build",        0.70f, 0.30f, 0.20f, 0.40f, 0.30f, 0.50f, 0.90f,  2.0f, 0.50f },
        { "Dirty Riser",       0.75f, 0.70f, 0.40f, 0.40f, 0.90f, 0.40f, 0.20f,  0.0f, 0.40f },
        { "Space Out",         0.50f, 0.20f, 0.70f, 1.00f, 0.10f, 1.00f, 0.00f,  0.0f, 0.20f },
        { "Thin Out",          1.00f, 0.20f, 0.00f, 0.20f, 0.10f, 0.30f, 0.00f,  0.0f, 0.70f },
        { "Phase Swirl",       0.65f, 0.30f, 1.00f, 0.45f, 0.15f, 0.70f, 0.00f,  1.0f, 0.40f },
        { "Last Second Panic", 0.90f, 0.60f, 0.30f, 0.60f, 0.50f, 0.60f, 1.00f,  3.0f, 1.00f },
    }};
}
