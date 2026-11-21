cmake_minimum_required(VERSION 3.22)

# The one version number for every Songsmith product lives in ../VERSION (x.y.z).
set(SONGSMITH_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION")
if(NOT EXISTS "${SONGSMITH_VERSION_FILE}")
    message(FATAL_ERROR "Missing ${SONGSMITH_VERSION_FILE}: the one-line x.y.z version file in the repository root.")
endif()
file(STRINGS "${SONGSMITH_VERSION_FILE}" SONGSMITH_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SONGSMITH_VERSION_FILE}")

project(SongsmithAnalogDesk VERSION ${SONGSMITH_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

# DESK-01: so far this is the engine and its tests. The plugin, its editor, presets and installers come in the following steps.
juce_add_console_app(AnalogDeskTests PRODUCT_NAME "AnalogDeskTests")

target_sources(AnalogDeskTests PRIVATE Tests/DeskEngineTests.cpp)
target_include_directories(AnalogDeskTests PRIVATE Source)
target_compile_definitions(AnalogDeskTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(AnalogDeskTests
    PRIVATE
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
