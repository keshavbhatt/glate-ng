# Global shortcut: a key that works while another app has focus. X11 key grab,
# Windows RegisterHotKey, or the xdg-desktop-portal GlobalShortcuts portal on
# Wayland (DOCS/DECISIONS.md ADR-009). Held to the strict standards (ADR-002).

set(GLOBAL_SHORTCUT_SOURCES
    GlobalShortcut.cpp
    GlobalShortcut.h
    KeyGrab.h
    KeyNames.cpp
    KeyNames.h
    ShortcutLogging.h
)
if(WIN32)
    list(APPEND GLOBAL_SHORTCUT_SOURCES KeyGrab_win.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND GLOBAL_SHORTCUT_SOURCES KeyGrab_x11.cpp PortalShortcuts.cpp PortalShortcuts.h)
else()
    list(APPEND GLOBAL_SHORTCUT_SOURCES KeyGrab_none.cpp)
endif()

qt_add_library(global_shortcut STATIC ${GLOBAL_SHORTCUT_SOURCES})
set_target_properties(global_shortcut PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    AUTOMOC ON
)
glate_set_warnings(global_shortcut)

target_include_directories(global_shortcut PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(global_shortcut PUBLIC Qt6::Core Qt6::Gui)

if(WIN32)
    target_link_libraries(global_shortcut PRIVATE user32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(global_shortcut PRIVATE Qt6::DBus X11::xcb)
    target_compile_definitions(global_shortcut PRIVATE GLATE_SHORTCUT_PORTAL)
endif()

target_compile_definitions(global_shortcut PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_KEYWORDS
    QT_NO_FOREACH
    QT_USE_QSTRINGBUILDER
)

if(MSVC)
    target_compile_options(global_shortcut PRIVATE /utf-8)
endif()
