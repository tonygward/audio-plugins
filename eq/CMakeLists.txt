cmake_minimum_required(VERSION 3.22)

project(ThreeBandEQ VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

option(EQ_COPY_PLUGIN "Copy the VST3 to the system VST3 folder after build (needs admin)" OFF)
option(EQ_BUILD_TESTS "Build the EQTests console app (DSP tests + benchmark)" ON)

set(EQ_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EQEngine.cpp
    Source/EQEditController.cpp
    Source/EQDisplay.cpp
    Source/BandPanel.cpp
    Source/SpectrumAnalyser.cpp
    Source/PresetManager.cpp
    Source/PresetBar.cpp
    Source/LinearPhase.cpp)

set(EQ_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# AU only exists on Apple platforms; Windows stays VST3 + Standalone.
set(EQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND EQ_FORMATS AU)
endif()

juce_add_plugin(ThreeBandEQ
    COMPANY_NAME                "TonyWard"
    PRODUCT_NAME                "TW Pro EQ"
    PLUGIN_MANUFACTURER_CODE    Tnyw
    PLUGIN_CODE                 Eq3b
    FORMATS                     ${EQ_FORMATS}
    BUNDLE_ID                   com.tonyward.twproeq
    VERSION                     ${PROJECT_VERSION}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             Fx EQ
    # PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE define the VST3 class ID. Keep them unchanged
    # (even though the product was renamed from "3-Band EQ") so saved projects still find it.
    # Copying into "C:/Program Files/Common Files/VST3" needs admin rights,
    # so it is off by default. Enable with -DEQ_COPY_PLUGIN=ON from an elevated shell.
    COPY_PLUGIN_AFTER_BUILD     ${EQ_COPY_PLUGIN})

target_sources(ThreeBandEQ PRIVATE ${EQ_SOURCES})
target_include_directories(ThreeBandEQ PRIVATE Source)
target_compile_definitions(ThreeBandEQ PUBLIC ${EQ_DEFINITIONS})

target_link_libraries(ThreeBandEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests & benchmark -------------------------------------------------------
# Compiles the same sources into a console app that runs juce::UnitTest suites
# (DSP accuracy, smoothing, state, undo) and a CPU benchmark (--bench).
if(EQ_BUILD_TESTS)
    juce_add_console_app(EQTests PRODUCT_NAME "EQTests")

    target_sources(EQTests PRIVATE Tests/EQTests.cpp ${EQ_SOURCES})
    target_include_directories(EQTests PRIVATE Source)
    target_compile_definitions(EQTests PRIVATE
        ${EQ_DEFINITIONS}
        "JucePlugin_Name=\"TW Pro EQ\"")

    target_link_libraries(EQTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
