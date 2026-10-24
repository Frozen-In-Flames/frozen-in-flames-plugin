cmake_minimum_required(VERSION 3.22)

project(FrozenInFlames VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# JUCE pinned to 8.0.15 (latest tag of the 8.0 series; see BUILD-LOG.md).
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# Formats: VST3 and Standalone everywhere; AU is added on Apple platforms,
# which is the standard JUCE arrangement (AU can only build on macOS).
set(FIF_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FIF_FORMATS AU)
endif()

juce_add_plugin(FrozenInFlames
    COMPANY_NAME                "Frozen In Flames"
    BUNDLE_ID                   com.frozeninflames.frozeninflames
    PLUGIN_MANUFACTURER_CODE    FrIF
    PLUGIN_CODE                 Fif1
    FORMATS                     ${FIF_FORMATS}
    PRODUCT_NAME                "Frozen In Flames"
    DESCRIPTION                 "Spectral freeze (ice) and multi-model flame saturation (fire) in one effect."
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect
)

juce_generate_juce_header(FrozenInFlames)

target_sources(FrozenInFlames PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/FiDsp.h
    Source/SpectralFreezer.h
)

target_include_directories(FrozenInFlames PRIVATE Source)

target_compile_definitions(FrozenInFlames PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(FrozenInFlames PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# JUCE-free DSP test harness, also wired into CTest.
enable_testing()
add_executable(dsp_tests Tests/dsp_test.cpp)
target_include_directories(dsp_tests PRIVATE Source)
add_test(NAME dsp_tests COMMAND dsp_tests)
