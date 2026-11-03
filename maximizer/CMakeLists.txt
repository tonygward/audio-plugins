cmake_minimum_required(VERSION 3.22)

project(SongsmithMaximiser VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

option(MAX_COPY_PLUGIN "Copy the VST3 to the system VST3 folder after build (needs admin)" OFF)
option(MAX_BUILD_TESTS "Build the MaximizerTests console app (DSP tests + benchmark)" ON)

set(MAX_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BrickwallLimiter.cpp)

set(MAX_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# --- Names and identity ------------------------------------------------------
# Second plug-in of the Songsmith family. It shares the manufacturer code with Songsmith EQ
# but has its own plug-in code, product name and bundle ID, so hosts list it separately.
set(MAX_COMPANY_NAME "Songsmith Studio")
set(MAX_PRODUCT_NAME "Songsmith Maximiser")
set(MAX_BUNDLE_ID    com.songsmithstudio.songsmithmaximiser)

# NEVER CHANGE THESE TWO once a build has been shared. JUCE builds the VST3 class ID and the
# AU identity (aufx Mxlm Tnyw) from them; changing them orphans saved sessions.
# MaximizerTests pins both values and checks they differ from Songsmith EQ's (Eq3b).
set(MAX_MANUFACTURER_CODE Tnyw)
set(MAX_PLUGIN_CODE       Mxlm)

# AU only exists on Apple platforms; Windows stays VST3 + Standalone.
set(MAX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MAX_FORMATS AU)
endif()

juce_add_plugin(SongsmithMaximiser
    COMPANY_NAME                "${MAX_COMPANY_NAME}"
    PRODUCT_NAME                "${MAX_PRODUCT_NAME}"
    PLUGIN_MANUFACTURER_CODE    ${MAX_MANUFACTURER_CODE}
    PLUGIN_CODE                 ${MAX_PLUGIN_CODE}
    FORMATS                     ${MAX_FORMATS}
    BUNDLE_ID                   ${MAX_BUNDLE_ID}
    VERSION                     ${PROJECT_VERSION}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     ${MAX_COPY_PLUGIN})

target_sources(SongsmithMaximiser PRIVATE ${MAX_SOURCES})
target_include_directories(SongsmithMaximiser PRIVATE Source)
target_compile_definitions(SongsmithMaximiser PUBLIC ${MAX_DEFINITIONS})

target_link_libraries(SongsmithMaximiser
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests & benchmark -------------------------------------------------------
# Compiles the same sources into a console app that runs juce::UnitTest suites.
if(MAX_BUILD_TESTS)
    juce_add_console_app(MaximizerTests PRODUCT_NAME "MaximizerTests")

    target_sources(MaximizerTests PRIVATE Tests/MaximizerTests.cpp ${MAX_SOURCES})
    target_include_directories(MaximizerTests PRIVATE Source)
    target_compile_definitions(MaximizerTests PRIVATE
        ${MAX_DEFINITIONS}
        "JucePlugin_Name=\"${MAX_PRODUCT_NAME}\""
        "JucePlugin_Manufacturer=\"${MAX_COMPANY_NAME}\""
        # Pinned by the identity test (the console app has no JucePlugin_*Code).
        "MAX_MANUFACTURER_CODE=\"${MAX_MANUFACTURER_CODE}\""
        "MAX_PLUGIN_CODE=\"${MAX_PLUGIN_CODE}\"")

    target_link_libraries(MaximizerTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
