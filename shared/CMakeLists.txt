# Songsmith shared UI library: the embedded Inter font, the type scale, the title and the base
# look-and-feel that every Songsmith Studio plugin uses (see docs/style-guide.md).
#
# Each plugin pulls it in from its own CMakeLists.txt, so every plugin still builds on its own:
#
#   add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)
#   target_link_libraries (MyPlugin PRIVATE songsmith_ui)
#
# Needs JUCE to be added first (juce_add_binary_data).

if(TARGET songsmith_ui)
    return()
endif()

# Inter (SIL Open Font License 1.1, see Resources/Fonts/OFL.txt): Regular, Medium and SemiBold.
# Header SongsmithFonts.h, namespace SongsmithFonts.
juce_add_binary_data(SongsmithUIFonts
    HEADER_NAME SongsmithFonts.h
    NAMESPACE   SongsmithFonts
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/Inter-Regular.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/Inter-Medium.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/Inter-SemiBold.ttf)
set_target_properties(SongsmithUIFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Header-only: SongsmithType.h (typeface, type scale, title) and SongsmithLookAndFeel.h.
add_library(songsmith_ui INTERFACE)
target_include_directories(songsmith_ui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_link_libraries(songsmith_ui INTERFACE SongsmithUIFonts)
