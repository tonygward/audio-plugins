cmake_minimum_required(VERSION 3.22)

# The one version number for every Songsmith plugin lives in ../VERSION (x.y.z).
set(SONGSMITH_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION")
if(NOT EXISTS "${SONGSMITH_VERSION_FILE}")
    message(FATAL_ERROR "Missing ${SONGSMITH_VERSION_FILE}: the one-line x.y.z version file in the repository root.")
endif()
file(STRINGS "${SONGSMITH_VERSION_FILE}" SONGSMITH_VERSION LIMIT_COUNT 1)
if(NOT SONGSMITH_VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+$")
    message(FATAL_ERROR "${SONGSMITH_VERSION_FILE} must hold one version as x.y.z (found '${SONGSMITH_VERSION}').")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SONGSMITH_VERSION_FILE}")

project(SongsmithAnalogDesk VERSION ${SONGSMITH_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

option(DESK_COPY_PLUGIN "Copy the VST3 to the system VST3 folder after build (needs admin)" OFF)
option(DESK_BUILD_TESTS "Build the AnalogDeskTests console app (engine and plugin tests)" ON)

set(DESK_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DeskPresets.cpp)

set(DESK_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# --- Names and identity ------------------------------------------------------
# It shares the manufacturer code with the other Songsmith plugins but has its own plugin code, product name and
# bundle ID, so hosts list it separately.
set(DESK_COMPANY_NAME "Songsmith Studio")
set(DESK_PRODUCT_NAME "Songsmith Analog Desk")
set(DESK_BUNDLE_ID    com.songsmithstudio.songsmithanalogdesk)

# NEVER CHANGE THESE TWO once a build has been shared. JUCE builds the VST3 class ID and the AU
# identity (aufx Desk Tnyw) from them; changing them orphans saved sessions. AnalogDeskTests pins
# both values and checks they differ from the other plugins'.
set(DESK_MANUFACTURER_CODE Tnyw)
set(DESK_PLUGIN_CODE       Desk)

# AU only exists on Apple platforms; Windows stays VST3 + Standalone.
set(DESK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DESK_FORMATS AU)
endif()

juce_add_plugin(SongsmithAnalogDesk
    COMPANY_NAME                "${DESK_COMPANY_NAME}"
    PRODUCT_NAME                "${DESK_PRODUCT_NAME}"
    PLUGIN_MANUFACTURER_CODE    ${DESK_MANUFACTURER_CODE}
    PLUGIN_CODE                 ${DESK_PLUGIN_CODE}
    FORMATS                     ${DESK_FORMATS}
    BUNDLE_ID                   ${DESK_BUNDLE_ID}
    VERSION                     ${PROJECT_VERSION}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx EQ Dynamics
    COPY_PLUGIN_AFTER_BUILD     ${DESK_COPY_PLUGIN})

# Shared Songsmith UI library: the embedded Inter font (SIL OFL 1.1, shared/Resources/Fonts), the
# type scale, the title and the base look-and-feel. See docs/style-guide.md.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)

target_sources(SongsmithAnalogDesk PRIVATE ${DESK_SOURCES})
target_include_directories(SongsmithAnalogDesk PRIVATE Source)
target_compile_definitions(SongsmithAnalogDesk PUBLIC ${DESK_DEFINITIONS})

target_link_libraries(SongsmithAnalogDesk
    PRIVATE
        songsmith_ui
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests -------------------------------------------------------------------
if(DESK_BUILD_TESTS)
    juce_add_console_app(AnalogDeskTests PRODUCT_NAME "AnalogDeskTests")

    target_sources(AnalogDeskTests PRIVATE Tests/DeskPluginTests.cpp Tests/DeskEngineTests.cpp ${DESK_SOURCES})
    target_include_directories(AnalogDeskTests PRIVATE Source)
    target_compile_definitions(AnalogDeskTests PRIVATE
        ${DESK_DEFINITIONS}
        JUCE_MODAL_LOOPS_PERMITTED=1
        "JucePlugin_Name=\"${DESK_PRODUCT_NAME}\""
        "JucePlugin_Manufacturer=\"${DESK_COMPANY_NAME}\""
        # The footer shows the one product version (the console app has no JucePlugin_VersionString of its own).
        "JucePlugin_VersionString=\"${PROJECT_VERSION}\""
        # Pinned by the identity test (the console app has no JucePlugin_*Code).
        "DESK_MANUFACTURER_CODE=\"${DESK_MANUFACTURER_CODE}\""
        "DESK_PLUGIN_CODE=\"${DESK_PLUGIN_CODE}\"")

    target_link_libraries(AnalogDeskTests
        PRIVATE
            songsmith_ui
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
