cmake_minimum_required(VERSION 3.22)

project(SongsmithAnalogCompressor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

option(FET_COPY_PLUGIN "Copy the VST3 to the system VST3 folder after build (needs admin)" OFF)
option(FET_BUILD_TESTS "Build the FETTests console app (DSP tests + benchmark)" ON)

set(FET_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/FETEngine.cpp
    Source/FETPresets.cpp)

set(FET_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# --- Names and identity ------------------------------------------------------
# Second plugin of the Songsmith family (see eq/CMakeLists.txt). Code uses
# JucePlugin_Manufacturer / JucePlugin_Name, never literals.
set(FET_COMPANY_NAME "Songsmith Studio")
set(FET_PRODUCT_NAME "Songsmith Analog Compressor")
set(FET_BUNDLE_ID    com.songsmithstudio.songsmithanalogcompressor)

# NEVER CHANGE THESE TWO once released. JUCE builds the VST3 class ID and the AU identity
# (aufx Sfc1 Tnyw) from them, so they are what lets saved sessions find the plugin again.
# The manufacturer code is shared with Songsmith EQ (Tnyw); the plugin code must differ from
# the EQ's (Eq3b). FETTests pins both values.
set(FET_MANUFACTURER_CODE Tnyw)
set(FET_PLUGIN_CODE       Sfc1)

# AU only exists on Apple platforms; Windows stays VST3 + Standalone.
set(FET_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FET_FORMATS AU)
endif()

juce_add_plugin(SongsmithAnalogCompressor
    COMPANY_NAME                "${FET_COMPANY_NAME}"
    PRODUCT_NAME                "${FET_PRODUCT_NAME}"
    PLUGIN_MANUFACTURER_CODE    ${FET_MANUFACTURER_CODE}
    PLUGIN_CODE                 ${FET_PLUGIN_CODE}
    FORMATS                     ${FET_FORMATS}
    BUNDLE_ID                   ${FET_BUNDLE_ID}
    VERSION                     ${PROJECT_VERSION}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     ${FET_COPY_PLUGIN})

# Shared Songsmith UI library: the embedded Inter font (SIL OFL 1.1, shared/Resources/Fonts), the
# type scale, the title and the base look-and-feel. See docs/style-guide.md.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)

target_sources(SongsmithAnalogCompressor PRIVATE ${FET_SOURCES})
target_include_directories(SongsmithAnalogCompressor PRIVATE Source)
target_compile_definitions(SongsmithAnalogCompressor PUBLIC ${FET_DEFINITIONS})

target_link_libraries(SongsmithAnalogCompressor
    PRIVATE
        songsmith_ui
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests & benchmark -------------------------------------------------------
if(FET_BUILD_TESTS)
    juce_add_console_app(FETTests PRODUCT_NAME "FETTests")

    target_sources(FETTests PRIVATE Tests/FETTests.cpp Tests/EngineTests.cpp Tests/ColourTests.cpp Tests/AllButtonsEditorTests.cpp Tests/PresetTests.cpp ${FET_SOURCES})
    target_include_directories(FETTests PRIVATE Source)
    target_compile_definitions(FETTests PRIVATE
        ${FET_DEFINITIONS}
        "JucePlugin_Name=\"${FET_PRODUCT_NAME}\""
        "JucePlugin_Manufacturer=\"${FET_COMPANY_NAME}\""
        "FET_MANUFACTURER_CODE=\"${FET_MANUFACTURER_CODE}\""
        "FET_PLUGIN_CODE=\"${FET_PLUGIN_CODE}\"")

    target_link_libraries(FETTests
        PRIVATE
            songsmith_ui
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
