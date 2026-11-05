// Params.h — parameter IDs, choice lists and factory presets.
#pragma once

#include <array>

namespace params
{
    inline constexpr const char* smile      = "smile";
    inline constexpr const char* rise       = "rise";
    inline constexpr const char* length     = "length";
    inline constexpr const char* curve      = "curve";
    inline constexpr const char* autoDrop   = "autoDrop";

    inline constexpr const char* filter     = "filter";
    inline constexpr const char* resonance  = "resonance";
    inline constexpr const char* phaser     = "phaser";
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* width      = "width";
    inline constexpr const char* chop       = "chop";

    inline constexpr const char* reverb     = "reverb";
    inline constexpr const char* decay      = "decay";
    inline constexpr const char* reverbType = "reverbType";
    inline constexpr const char* delay      = "delay";
    inline constexpr const char* feedback   = "feedback";
    inline constexpr const char* delayTime  = "delayTime";
    inline constexpr const char* pingPong   = "pingPong";
    inline constexpr const char* wash       = "wash";

    inline constexpr const char* ride       = "ride";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* output     = "output";
    inline constexpr const char* safety     = "safety";

    // Length choices, in bars.
    inline constexpr std::array<int, 6> lengthBars { 1, 2, 4, 8, 16, 32 };
    inline constexpr int defaultLengthIndex = 3;    // 8 bars

    // Must match rise::ReverbType order.
    inline constexpr std::array<const char*, 5> reverbTypeNames { "Hall", "Plate", "Room", "Shimmer", "Infinite" };

    // Must match rise::kDelayBeats order.
    inline constexpr std::array<const char*, 8> delayTimeNames { "1/2", "1/4 dotted", "1/4", "1/4 triplet",
                                                                 "1/8 dotted", "1/8", "1/8 triplet", "1/16" };
    inline constexpr int defaultDelayTimeIndex = 4; // 1/8 dotted

    struct Preset
    {
        const char* name;
        float filter, resonance, phaser, drive, width, chop;
        float reverb, decay; int reverbType;
        float delay, feedback; int delayTime; bool pingPong;
        float wash, ride, curve;
    };

    // Presets set the character only; Smile, Rise, Length, Mix and Output stay as they are.
    // reverbType: 0 Hall, 1 Plate, 2 Room, 3 Shimmer, 4 Infinite
    // delayTime:  0 1/2, 1 1/4., 2 1/4, 3 1/4T, 4 1/8., 5 1/8, 6 1/8T, 7 1/16
    inline constexpr std::array<Preset, 12> presets
    {{
        //  name                 filt   reso   phas   drive  width  chop    verb   decay type  delay  fb    time  pp     wash   ride   curve
        { "Classic Smile",       0.80f, 0.30f, 0.50f, 0.30f, 0.60f, 0.00f,  0.65f, 0.60f, 0,   0.40f, 0.60f, 4,  true,  0.85f, 2.0f, 0.50f },
        { "Big Room Wash",       0.90f, 0.40f, 0.30f, 0.20f, 0.80f, 0.00f,  0.85f, 0.80f, 0,   0.50f, 0.70f, 4,  true,  1.00f, 3.0f, 0.50f },
        { "Hardstyle Lift",      0.85f, 0.50f, 0.20f, 0.60f, 0.50f, 0.40f,  0.60f, 0.50f, 1,   0.35f, 0.55f, 2,  false, 0.70f, 2.0f, 0.60f },
        { "Clean Lift",          0.60f, 0.10f, 0.00f, 0.00f, 0.50f, 0.00f,  0.45f, 0.40f, 1,   0.25f, 0.40f, 4,  true,  0.50f, 1.0f, 0.30f },
        { "Chop Build",          0.70f, 0.30f, 0.20f, 0.30f, 0.50f, 0.90f,  0.50f, 0.50f, 2,   0.30f, 0.50f, 7,  true,  0.60f, 2.0f, 0.50f },
        { "Dirty Riser",         0.75f, 0.70f, 0.40f, 0.90f, 0.40f, 0.20f,  0.50f, 0.50f, 2,   0.30f, 0.60f, 5,  false, 0.60f, 0.0f, 0.40f },
        { "Space Out",           0.50f, 0.20f, 0.70f, 0.10f, 1.00f, 0.00f,  1.00f, 1.00f, 4,   0.60f, 0.75f, 1,  true,  1.00f, 0.0f, 0.20f },
        { "Thin Out",            1.00f, 0.20f, 0.00f, 0.10f, 0.30f, 0.00f,  0.40f, 0.40f, 1,   0.20f, 0.40f, 4,  true,  0.50f, 0.0f, 0.70f },
        { "Phase Swirl",         0.65f, 0.30f, 1.00f, 0.15f, 0.70f, 0.00f,  0.60f, 0.60f, 0,   0.40f, 0.60f, 6,  true,  0.80f, 1.0f, 0.40f },
        { "Shimmer Heaven",      0.70f, 0.30f, 0.30f, 0.10f, 0.80f, 0.00f,  0.90f, 0.80f, 3,   0.45f, 0.65f, 2,  true,  0.95f, 1.0f, 0.50f },
        { "Echo Out",            0.70f, 0.30f, 0.20f, 0.20f, 0.60f, 0.00f,  0.50f, 0.60f, 0,   1.00f, 0.85f, 4,  true,  0.90f, 1.0f, 0.50f },
        { "Last Second Panic",   0.90f, 0.60f, 0.30f, 0.50f, 0.60f, 1.00f,  0.70f, 0.60f, 0,   0.40f, 0.60f, 7,  true,  0.85f, 3.0f, 1.00f },
    }};
}
