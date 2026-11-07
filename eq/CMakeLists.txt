cmake_minimum_required(VERSION 3.22)

# The one version number for every Songsmith plugin lives in ../VERSION (x.y.z).
set(SONGSMITH_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION")
if(NOT EXISTS "${SONGSMITH_VERSION_FILE}")
    message(FATAL_ERROR "Missing ${SONGSMITH_VERSION_FILE}: the one-line x.y.z version file in the repository root.")
endif()
file(STRINGS "${SONGSMITH_VERSION_FILE}" SONGSMITH_VERSION LIMIT_COUNT 1)
if(NOT SONGSMITH_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "${SONGSMITH_VERSION_FILE} must hold one version as x.y.z (found '${SONGSMITH_VERSION}').")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SONGSMITH_VERSION_FILE}")

project(SongsmithEQ VERSION ${SONGSMITH_VERSION} LANGUAGES C CXX)

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
    JUCE_VST3_CAN_REPLACE_VST2=0)

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

# Shared Songsmith UI library: the embedded Inter font (SIL OFL 1.1, shared/Resources/Fonts), the
# type scale, the title and the base look-and-feel. See docs/style-guide.md.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)

target_sources(SongsmithEQ PRIVATE ${EQ_SOURCES})
target_include_directories(SongsmithEQ PRIVATE Source)
target_compile_definitions(SongsmithEQ PUBLIC ${EQ_DEFINITIONS})

target_link_libraries(SongsmithEQ
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
            songsmith_ui
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
