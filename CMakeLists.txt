cmake_minimum_required(VERSION 3.22)

project(RiseKit VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE (downloaded automatically during configure) -----------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master          # JUCE's master branch = latest stable release
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---- The plugin --------------------------------------------------------------
juce_add_plugin(RiseKit
    COMPANY_NAME             "D-TEKT"
    PRODUCT_NAME             "RiseKit"
    PLUGIN_MANUFACTURER_CODE Dtkt
    PLUGIN_CODE              Rsk1
    FORMATS                  VST3
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES          Fx
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

target_sources(RiseKit PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(RiseKit PRIVATE Source)

target_compile_definitions(RiseKit PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(RiseKit
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---- DSP test (no JUCE needed) ---------------------------------------------
enable_testing()
add_executable(RiseKitDSPTest tests/dsp_test.cpp)
target_include_directories(RiseKitDSPTest PRIVATE Source)
add_test(NAME dsp COMMAND RiseKitDSPTest)
