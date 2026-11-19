cmake_minimum_required(VERSION 3.22)

# The one version number for every Songsmith product lives in ../VERSION (x.y.z).
set(SONGSMITH_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION")
if(NOT EXISTS "${SONGSMITH_VERSION_FILE}")
    message(FATAL_ERROR "Missing ${SONGSMITH_VERSION_FILE}: the one-line x.y.z version file in the repository root.")
endif()
file(STRINGS "${SONGSMITH_VERSION_FILE}" SONGSMITH_VERSION LIMIT_COUNT 1)
if(NOT SONGSMITH_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "${SONGSMITH_VERSION_FILE} must hold one version as x.y.z (found '${SONGSMITH_VERSION}').")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SONGSMITH_VERSION_FILE}")

project(SongsmithAudioAnalyzer VERSION ${SONGSMITH_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the local JUCE checkout (override with -DJUCE_DIR=...)
set(JUCE_DIR "C:/dev/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

option(ANALYZER_BUILD_TESTS "Build the AudioAnalyzerTests console app (analysis, advice and window tests)" ON)

# --- Names and identity ------------------------------------------------------
# A standalone application, not a plugin: no VST3 and no Audio Unit, and it does not need a recording program.
# NEVER CHANGE the product name or the bundle ID once a build has been shared: the installers recognise an
# existing install by them (AudioAnalyzerTests pins both).
set(ANALYZER_COMPANY_NAME "Songsmith Studio")
set(ANALYZER_PRODUCT_NAME "Songsmith Audio Analyzer")
set(ANALYZER_BUNDLE_ID    com.songsmithstudio.songsmithaudioanalyzer)

set(ANALYZER_SOURCES
    Source/AnalyzerComponent.cpp
    Source/GuidanceView.cpp)

set(ANALYZER_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    "ANALYZER_PRODUCT_NAME=\"${ANALYZER_PRODUCT_NAME}\""
    "ANALYZER_VERSION=\"${PROJECT_VERSION}\"")

juce_add_gui_app(SongsmithAudioAnalyzer
    COMPANY_NAME        "${ANALYZER_COMPANY_NAME}"
    PRODUCT_NAME        "${ANALYZER_PRODUCT_NAME}"
    BUNDLE_ID           ${ANALYZER_BUNDLE_ID}
    VERSION             ${PROJECT_VERSION}
    # macOS: Finder can open these with the app, and a file dropped on its icon arrives as a file to open.
    DOCUMENT_EXTENSIONS wav aif aiff flac
    # No microphone, camera or network use: the app only reads the file you choose.
    NEEDS_CURL          FALSE
    NEEDS_WEB_BROWSER   FALSE)

# Shared Songsmith UI library: the embedded Inter font (SIL OFL 1.1, shared/Resources/Fonts), the
# type scale, the title and the base look-and-feel. See docs/style-guide.md.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)

target_sources(SongsmithAudioAnalyzer PRIVATE Source/Main.cpp ${ANALYZER_SOURCES})
target_include_directories(SongsmithAudioAnalyzer PRIVATE Source)
target_compile_definitions(SongsmithAudioAnalyzer PRIVATE ${ANALYZER_DEFINITIONS})

target_link_libraries(SongsmithAudioAnalyzer
    PRIVATE
        songsmith_ui
        juce::juce_gui_extra
        juce::juce_audio_formats
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests -------------------------------------------------------------------
if(ANALYZER_BUILD_TESTS)
    juce_add_console_app(AudioAnalyzerTests PRODUCT_NAME "AudioAnalyzerTests")

    target_sources(AudioAnalyzerTests PRIVATE Tests/AudioAnalyzerTests.cpp ${ANALYZER_SOURCES})
    target_include_directories(AudioAnalyzerTests PRIVATE Source)
    target_compile_definitions(AudioAnalyzerTests PRIVATE
        ${ANALYZER_DEFINITIONS}
        JUCE_MODAL_LOOPS_PERMITTED=1   # the tests run the message loop to wait for the background reader
        "ANALYZER_BUNDLE_ID=\"${ANALYZER_BUNDLE_ID}\"")

    target_link_libraries(AudioAnalyzerTests
        PRIVATE
            songsmith_ui
            juce::juce_gui_extra
            juce::juce_audio_formats
            juce::juce_audio_utils
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
