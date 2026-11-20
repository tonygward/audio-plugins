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

project(SongsmithPluginAnalyzer VERSION ${SONGSMITH_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

option(ANA_COPY_PLUGIN "Copy the VST3 to the system VST3 folder after build (needs admin)" OFF)
option(ANA_BUILD_TESTS "Build the PluginAnalyzerTests console app (measurement and plugin tests)" ON)

set(ANA_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

set(ANA_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# --- Names and identity ------------------------------------------------------
# A measurement utility for hosts: a Reference goes before another plugin and a Result after it. It shares the manufacturer
# code with the other Songsmith plugins but has its own plugin code, product name and bundle ID.
set(ANA_COMPANY_NAME "Songsmith Studio")
set(ANA_PRODUCT_NAME "Songsmith Plugin Analyzer")
set(ANA_BUNDLE_ID    com.songsmithstudio.songsmithpluginanalyzer)

# NEVER CHANGE THESE TWO once a build has been shared. JUCE builds the VST3 class ID from them; changing them orphans saved
# sessions. PluginAnalyzerTests pins both values and checks they differ from the other plugins' (Eq3b, Sfc1, Mxlm, Cmp1, Opt1, Peq1, Bus1).
set(ANA_MANUFACTURER_CODE Tnyw)
set(ANA_PLUGIN_CODE       Anl1)

# VST3 only: the Reference and the Result must be two instances of the same plugin in one host program, and that is what the VST3
# hosts the measurement is for. There is no Audio Unit and no standalone app (a measuring tool needs another plugin to measure).
set(ANA_FORMATS VST3)

juce_add_plugin(SongsmithPluginAnalyzer
    COMPANY_NAME                "${ANA_COMPANY_NAME}"
    PRODUCT_NAME                "${ANA_PRODUCT_NAME}"
    PLUGIN_MANUFACTURER_CODE    ${ANA_MANUFACTURER_CODE}
    PLUGIN_CODE                 ${ANA_PLUGIN_CODE}
    FORMATS                     ${ANA_FORMATS}
    BUNDLE_ID                   ${ANA_BUNDLE_ID}
    VERSION                     ${PROJECT_VERSION}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD     ${ANA_COPY_PLUGIN})

# Shared Songsmith UI library: the embedded Inter font (SIL OFL 1.1, shared/Resources/Fonts), the
# type scale, the title and the base look-and-feel. See docs/style-guide.md.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)

target_sources(SongsmithPluginAnalyzer PRIVATE ${ANA_SOURCES})
target_include_directories(SongsmithPluginAnalyzer PRIVATE Source)
target_compile_definitions(SongsmithPluginAnalyzer PUBLIC ${ANA_DEFINITIONS})

target_link_libraries(SongsmithPluginAnalyzer
    PRIVATE
        songsmith_ui
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests -------------------------------------------------------------------
if(ANA_BUILD_TESTS)
    juce_add_console_app(PluginAnalyzerTests PRODUCT_NAME "PluginAnalyzerTests")

    target_sources(PluginAnalyzerTests PRIVATE Tests/PluginAnalyzerTests.cpp ${ANA_SOURCES})
    target_include_directories(PluginAnalyzerTests PRIVATE Source)
    target_compile_definitions(PluginAnalyzerTests PRIVATE
        ${ANA_DEFINITIONS}
        JUCE_MODAL_LOOPS_PERMITTED=1   # the tests run the message loop so the session counts follow parameter changes
        "JucePlugin_Name=\"${ANA_PRODUCT_NAME}\""
        "JucePlugin_Manufacturer=\"${ANA_COMPANY_NAME}\""
        # The footer shows the one product version (the console app has no JucePlugin_VersionString of its own).
        "JucePlugin_VersionString=\"${PROJECT_VERSION}\""
        # Pinned by the identity test (the console app has no JucePlugin_*Code).
        "ANA_MANUFACTURER_CODE=\"${ANA_MANUFACTURER_CODE}\""
        "ANA_PLUGIN_CODE=\"${ANA_PLUGIN_CODE}\"")

    target_link_libraries(PluginAnalyzerTests
        PRIVATE
            songsmith_ui
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
