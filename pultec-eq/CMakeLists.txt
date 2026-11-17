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

project(SongsmithPassiveEQ VERSION ${SONGSMITH_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

option(PEQ_COPY_PLUGIN "Copy the VST3 to the system VST3 folder after build (needs admin)" OFF)
option(PEQ_BUILD_TESTS "Build the PassiveEQTests console app (engine and plugin tests)" ON)

set(PEQ_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PeqPresets.cpp
    Source/PassiveEngine.cpp)

set(PEQ_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# --- Names and identity ------------------------------------------------------
# Sixth plugin of the Songsmith family. It shares the manufacturer code with the others but has its
# own plugin code, product name and bundle ID, so hosts list it separately from the other plugins.
set(PEQ_COMPANY_NAME "Songsmith Studio")
set(PEQ_PRODUCT_NAME "Songsmith Passive EQ")
set(PEQ_BUNDLE_ID    com.songsmithstudio.songsmithpassiveeq)

# NEVER CHANGE THESE TWO once a build has been shared. JUCE builds the VST3 class ID and the AU
# identity (aufx Peq1 Tnyw) from them; changing them orphans saved sessions. PassiveEQTests pins
# both values and checks they differ from the other plugins' (Eq3b, Sfc1, Mxlm, Cmp1, Opt1).
set(PEQ_MANUFACTURER_CODE Tnyw)
set(PEQ_PLUGIN_CODE       Peq1)

# AU only exists on Apple platforms; Windows stays VST3 + Standalone.
set(PEQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PEQ_FORMATS AU)
endif()

juce_add_plugin(SongsmithPassiveEQ
    COMPANY_NAME                "${PEQ_COMPANY_NAME}"
    PRODUCT_NAME                "${PEQ_PRODUCT_NAME}"
    PLUGIN_MANUFACTURER_CODE    ${PEQ_MANUFACTURER_CODE}
    PLUGIN_CODE                 ${PEQ_PLUGIN_CODE}
    FORMATS                     ${PEQ_FORMATS}
    BUNDLE_ID                   ${PEQ_BUNDLE_ID}
    VERSION                     ${PROJECT_VERSION}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx EQ
    COPY_PLUGIN_AFTER_BUILD     ${PEQ_COPY_PLUGIN})

# Shared Songsmith UI library: the embedded Inter font (SIL OFL 1.1, shared/Resources/Fonts), the
# type scale, the title and the base look-and-feel. See docs/style-guide.md.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)

target_sources(SongsmithPassiveEQ PRIVATE ${PEQ_SOURCES})
target_include_directories(SongsmithPassiveEQ PRIVATE Source)
target_compile_definitions(SongsmithPassiveEQ PUBLIC ${PEQ_DEFINITIONS})

target_link_libraries(SongsmithPassiveEQ
    PRIVATE
        songsmith_ui
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests -------------------------------------------------------------------
if(PEQ_BUILD_TESTS)
    juce_add_console_app(PassiveEQTests PRODUCT_NAME "PassiveEQTests")

    target_sources(PassiveEQTests PRIVATE Tests/PassiveEQTests.cpp Tests/EngineTests.cpp ${PEQ_SOURCES})
    target_include_directories(PassiveEQTests PRIVATE Source)
    target_compile_definitions(PassiveEQTests PRIVATE
        ${PEQ_DEFINITIONS}
        "JucePlugin_Name=\"${PEQ_PRODUCT_NAME}\""
        "JucePlugin_Manufacturer=\"${PEQ_COMPANY_NAME}\""
        # The footer shows the one product version (the console app has no JucePlugin_VersionString of its own).
        "JucePlugin_VersionString=\"${PROJECT_VERSION}\""
        # Pinned by the identity test (the console app has no JucePlugin_*Code).
        "PEQ_MANUFACTURER_CODE=\"${PEQ_MANUFACTURER_CODE}\""
        "PEQ_PLUGIN_CODE=\"${PEQ_PLUGIN_CODE}\"")

    target_link_libraries(PassiveEQTests
        PRIVATE
            songsmith_ui
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
