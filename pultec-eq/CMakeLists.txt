cmake_minimum_required(VERSION 3.22)

# Pultec-style EQ (PEQ-01). Slice 1 builds the DSP engine and its tests only; the plugin
# target (stable identity, parameters, editor) is added by the later slices listed in
# docs/reference.md. The one version number lives in ../VERSION, as for every plugin.
set(SONGSMITH_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION")
if(NOT EXISTS "${SONGSMITH_VERSION_FILE}")
    message(FATAL_ERROR "Missing ${SONGSMITH_VERSION_FILE}: the one-line x.y.z version file in the repository root.")
endif()
file(STRINGS "${SONGSMITH_VERSION_FILE}" SONGSMITH_VERSION LIMIT_COUNT 1)
if(NOT SONGSMITH_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "${SONGSMITH_VERSION_FILE} must hold one version as x.y.z (found '${SONGSMITH_VERSION}').")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SONGSMITH_VERSION_FILE}")

project(SongsmithPultecEQ VERSION ${SONGSMITH_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

juce_add_console_app(PassiveTests PRODUCT_NAME "PassiveTests")

target_sources(PassiveTests PRIVATE
    Tests/PassiveTests.cpp
    Source/PassiveEngine.cpp)
target_include_directories(PassiveTests PRIVATE Source)
target_compile_definitions(PassiveTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(PassiveTests
    PRIVATE
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
