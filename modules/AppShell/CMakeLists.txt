# App shell: logging, release notes, diagnostics and the About, What's new,
# bug report and shortcuts windows, plus the welcome card and status pill
# (DOCS/DECISIONS.md ADR-008). Held to the strict standards (ADR-002).

qt_add_library(app_shell STATIC
    AboutDialog.cpp
    AboutDialog.h
    BugReportDialog.cpp
    BugReportDialog.h
    Changelog.cpp
    Changelog.h
    Diagnostics.cpp
    Diagnostics.h
    FlowLayout.cpp
    FlowLayout.h
    LogSink.cpp
    LogSink.h
    Logging.cpp
    Logging.h
    ShellStyle.cpp
    ShellStyle.h
    ShortcutsDialog.cpp
    ShortcutsDialog.h
    StatusPill.cpp
    StatusPill.h
    WelcomeCard.cpp
    WelcomeCard.h
    WhatsNewDialog.cpp
    WhatsNewDialog.h
)
set_target_properties(app_shell PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    AUTOMOC ON
)
glate_set_warnings(app_shell)

target_include_directories(app_shell PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(app_shell PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)

target_compile_definitions(app_shell PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_KEYWORDS
    QT_NO_FOREACH
    QT_USE_QSTRINGBUILDER
)

if(MSVC)
    target_compile_options(app_shell PRIVATE /utf-8)
endif()
