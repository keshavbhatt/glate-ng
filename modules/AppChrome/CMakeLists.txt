cmake_minimum_required(VERSION 3.21)

# Find Qt6 if not already found by a parent project. Needs >= 6.5 for
# QStyleHints::colorScheme()/colorSchemeChanged (live OS light/dark tracking).
if(NOT Qt6_FOUND)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
endif()

add_library(app_chrome STATIC
    AppTheme.h
    AppTheme.cpp
    ThemeController.h
    ThemeController.cpp
    WindowChrome.h
    WindowChrome.cpp
    TitleBarController.h
    TitleBarController.cpp
    resources/appchrome.qrc
)

# ThemeController / TitleBarController use Q_OBJECT — must be processed by MOC,
# and the bundled themes/icons (appchrome.qrc) by RCC, regardless of the parent
# project's settings.
set_target_properties(app_chrome PROPERTIES AUTOMOC ON AUTORCC ON)

target_include_directories(app_chrome PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(app_chrome PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Suppress debug output in non-debug builds (consistent with the rest of the app).
target_compile_definitions(app_chrome PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:QT_NO_DEBUG_OUTPUT>
)

if(MSVC)
    target_compile_options(app_chrome PRIVATE /utf-8)
endif()
