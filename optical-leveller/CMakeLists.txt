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

project(SongsmithOpticalLeveller VERSION ${SONGSMITH_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

option(OPT_COPY_PLUGIN "Copy the VST3 to the system VST3 folder after build (needs admin)" OFF)
option(OPT_BUILD_TESTS "Build the OpticalLevellerTests console app (engine and plugin tests)" ON)

set(OPT_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/OptPresets.cpp)

set(OPT_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# --- Names and identity ------------------------------------------------------
# Fifth plugin of the Songsmith family. It shares the manufacturer code with the others but has its
# own plugin code, product name and bundle ID, so hosts list it separately from Songsmith Analog Compressor and Songsmith Compressor.
set(OPT_COMPANY_NAME "Songsmith Studio")
set(OPT_PRODUCT_NAME "Songsmith Optical Leveller")
set(OPT_BUNDLE_ID    com.songsmithstudio.songsmithopticalleveller)

# NEVER CHANGE THESE TWO once a build has been shared. JUCE builds the VST3 class ID and the AU
# identity (aufx Opt1 Tnyw) from them; changing them orphans saved sessions. OpticalLevellerTests pins
# both values and checks they differ from the other plugins' (Eq3b, Sfc1, Mxlm, Cmp1).
set(OPT_MANUFACTURER_CODE Tnyw)
set(OPT_PLUGIN_CODE       Opt1)

# AU only exists on Apple platforms; Windows stays VST3 + Standalone.
set(OPT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND OPT_FORMATS AU)
endif()

juce_add_plugin(SongsmithOpticalLeveller
    COMPANY_NAME                "${OPT_COMPANY_NAME}"
    PRODUCT_NAME                "${OPT_PRODUCT_NAME}"
    PLUGIN_MANUFACTURER_CODE    ${OPT_MANUFACTURER_CODE}
    PLUGIN_CODE                 ${OPT_PLUGIN_CODE}
    FORMATS                     ${OPT_FORMATS}
    BUNDLE_ID                   ${OPT_BUNDLE_ID}
    VERSION                     ${PROJECT_VERSION}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     ${OPT_COPY_PLUGIN})

# Shared Songsmith UI library: the embedded Inter font (SIL OFL 1.1, shared/Resources/Fonts), the
# type scale, the title and the base look-and-feel. See docs/style-guide.md.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)

target_sources(SongsmithOpticalLeveller PRIVATE ${OPT_SOURCES})
target_include_directories(SongsmithOpticalLeveller PRIVATE Source)
target_compile_definitions(SongsmithOpticalLeveller PUBLIC ${OPT_DEFINITIONS})

target_link_libraries(SongsmithOpticalLeveller
    PRIVATE
        songsmith_ui
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests -------------------------------------------------------------------
if(OPT_BUILD_TESTS)
    juce_add_console_app(OpticalLevellerTests PRODUCT_NAME "OpticalLevellerTests")

    target_sources(OpticalLevellerTests PRIVATE Tests/OpticalLevellerTests.cpp Tests/EngineTests.cpp ${OPT_SOURCES})
    target_include_directories(OpticalLevellerTests PRIVATE Source)
    target_compile_definitions(OpticalLevellerTests PRIVATE
        ${OPT_DEFINITIONS}
        "JucePlugin_Name=\"${OPT_PRODUCT_NAME}\""
        "JucePlugin_Manufacturer=\"${OPT_COMPANY_NAME}\""
        # The footer shows the one product version (the console app has no JucePlugin_VersionString of its own).
        "JucePlugin_VersionString=\"${PROJECT_VERSION}\""
        # Pinned by the identity test (the console app has no JucePlugin_*Code).
        "OPT_MANUFACTURER_CODE=\"${OPT_MANUFACTURER_CODE}\""
        "OPT_PLUGIN_CODE=\"${OPT_PLUGIN_CODE}\"")

    target_link_libraries(OpticalLevellerTests
        PRIVATE
            songsmith_ui
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
