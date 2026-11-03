cmake_minimum_required(VERSION 3.22)

project(SongsmithEQ VERSION 0.4.0 LANGUAGES C CXX)

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

# --- Names and identity ------------------------------------------------------
# Brand + product. Songsmith EQ is the first of a family (Songsmith Analogue EQ is
# planned), so code uses JucePlugin_Manufacturer / JucePlugin_Name, never literals.
# Songsmith EQ was called "TW Pro EQ" (company "TonyWard") up to v0.2.
set(EQ_COMPANY_NAME "Songsmith Studio")
set(EQ_PRODUCT_NAME "Songsmith EQ")
set(EQ_BUNDLE_ID    com.songsmithstudio.songsmitheq)

# NEVER CHANGE THESE TWO. JUCE builds the VST3 class ID and the AU identity
# (aufx Eq3b Tnyw) from them, so they are what lets Studio One / Logic sessions saved
# with "3-Band EQ" or "TW Pro EQ" open with Songsmith EQ. A future Songsmith plugin
# (e.g. Songsmith Analogue EQ) shares the manufacturer code Tnyw but needs its own
# PLUGIN_CODE. EQTests pins both values.
set(EQ_MANUFACTURER_CODE Tnyw)
set(EQ_PLUGIN_CODE       Eq3b)

# AU only exists on Apple platforms; Windows stays VST3 + Standalone.
set(EQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND EQ_FORMATS AU)
endif()

juce_add_plugin(SongsmithEQ
    COMPANY_NAME                "${EQ_COMPANY_NAME}"
    PRODUCT_NAME                "${EQ_PRODUCT_NAME}"
    PLUGIN_MANUFACTURER_CODE    ${EQ_MANUFACTURER_CODE}
    PLUGIN_CODE                 ${EQ_PLUGIN_CODE}
    FORMATS                     ${EQ_FORMATS}
    BUNDLE_ID                   ${EQ_BUNDLE_ID}
    VERSION                     ${PROJECT_VERSION}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             Fx EQ
    # Copying into "C:/Program Files/Common Files/VST3" needs admin rights,
    # so it is off by default. Enable with -DEQ_COPY_PLUGIN=ON from an elevated shell.
    COPY_PLUGIN_AFTER_BUILD     ${EQ_COPY_PLUGIN})

# Embedded UI font: Inter (SIL Open Font License 1.1, see Resources/Fonts/OFL.txt), so the
# editor looks the same on Windows and macOS. Header EQBinaryData.h, namespace EQBinaryData.
juce_add_binary_data(SongsmithEQFonts
    HEADER_NAME EQBinaryData.h
    NAMESPACE   EQBinaryData
    SOURCES
        Resources/Fonts/Inter-Regular.ttf
        Resources/Fonts/Inter-Medium.ttf
        Resources/Fonts/Inter-SemiBold.ttf)
set_target_properties(SongsmithEQFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(SongsmithEQ PRIVATE ${EQ_SOURCES})
target_include_directories(SongsmithEQ PRIVATE Source)
target_compile_definitions(SongsmithEQ PUBLIC ${EQ_DEFINITIONS})

target_link_libraries(SongsmithEQ
    PRIVATE
        SongsmithEQFonts
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
        "JucePlugin_Name=\"${EQ_PRODUCT_NAME}\""
        "JucePlugin_Manufacturer=\"${EQ_COMPANY_NAME}\""
        # Pinned by the identity test (the console app has no JucePlugin_*Code).
        "EQ_MANUFACTURER_CODE=\"${EQ_MANUFACTURER_CODE}\""
        "EQ_PLUGIN_CODE=\"${EQ_PLUGIN_CODE}\""
        "EQ_TEST_FIXTURES=\"${CMAKE_CURRENT_SOURCE_DIR}/Tests/fixtures\"")

    target_link_libraries(EQTests
        PRIVATE
            SongsmithEQFonts
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
