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

project(SongsmithBusCompressor VERSION ${SONGSMITH_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

option(BUS_COPY_PLUGIN "Copy the VST3 to the system VST3 folder after build (needs admin)" OFF)
option(BUS_BUILD_TESTS "Build the BusCompressorTests console app (engine and plugin tests)" ON)

set(BUS_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BusPresets.cpp)

set(BUS_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# --- Names and identity ------------------------------------------------------
# Seventh plugin of the Songsmith family. It shares the manufacturer code with the others but has its
# own plugin code, product name and bundle ID, so hosts list it separately from the other Songsmith plugins.
set(BUS_COMPANY_NAME "Songsmith Studio")
set(BUS_PRODUCT_NAME "Songsmith Bus Compressor")
set(BUS_BUNDLE_ID    com.songsmithstudio.songsmithbuscompressor)

# NEVER CHANGE THESE TWO once a build has been shared. JUCE builds the VST3 class ID and the AU
# identity (aufx Bus1 Tnyw) from them; changing them orphans saved sessions. BusCompressorTests pins
# both values and checks they differ from the other plugins' (Eq3b, Sfc1, Mxlm, Cmp1, Opt1, Peq1).
set(BUS_MANUFACTURER_CODE Tnyw)
set(BUS_PLUGIN_CODE       Bus1)

# AU only exists on Apple platforms; Windows stays VST3 + Standalone.
set(BUS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND BUS_FORMATS AU)
endif()

juce_add_plugin(SongsmithBusCompressor
    COMPANY_NAME                "${BUS_COMPANY_NAME}"
    PRODUCT_NAME                "${BUS_PRODUCT_NAME}"
    PLUGIN_MANUFACTURER_CODE    ${BUS_MANUFACTURER_CODE}
    PLUGIN_CODE                 ${BUS_PLUGIN_CODE}
    FORMATS                     ${BUS_FORMATS}
    BUNDLE_ID                   ${BUS_BUNDLE_ID}
    VERSION                     ${PROJECT_VERSION}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     ${BUS_COPY_PLUGIN})

# Shared Songsmith UI library: the embedded Inter font (SIL OFL 1.1, shared/Resources/Fonts), the
# type scale, the title and the base look-and-feel. See docs/style-guide.md.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)

target_sources(SongsmithBusCompressor PRIVATE ${BUS_SOURCES})
target_include_directories(SongsmithBusCompressor PRIVATE Source)
target_compile_definitions(SongsmithBusCompressor PUBLIC ${BUS_DEFINITIONS})

target_link_libraries(SongsmithBusCompressor
    PRIVATE
        songsmith_ui
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests -------------------------------------------------------------------
if(BUS_BUILD_TESTS)
    juce_add_console_app(BusCompressorTests PRODUCT_NAME "BusCompressorTests")

    target_sources(BusCompressorTests PRIVATE Tests/BusCompressorTests.cpp Tests/EngineTests.cpp ${BUS_SOURCES})
    target_include_directories(BusCompressorTests PRIVATE Source)
    target_compile_definitions(BusCompressorTests PRIVATE
        ${BUS_DEFINITIONS}
        "JucePlugin_Name=\"${BUS_PRODUCT_NAME}\""
        "JucePlugin_Manufacturer=\"${BUS_COMPANY_NAME}\""
        # The footer shows the one product version (the console app has no JucePlugin_VersionString of its own).
        "JucePlugin_VersionString=\"${PROJECT_VERSION}\""
        # Pinned by the identity test (the console app has no JucePlugin_*Code).
        "BUS_MANUFACTURER_CODE=\"${BUS_MANUFACTURER_CODE}\""
        "BUS_PLUGIN_CODE=\"${BUS_PLUGIN_CODE}\"")

    target_link_libraries(BusCompressorTests
        PRIVATE
            songsmith_ui
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
