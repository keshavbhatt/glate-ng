cmake_minimum_required(VERSION 3.25)
project(glate VERSION 4.4.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(GLATE_BUILD_TESTS "Build unit tests" OFF)
option(GLATE_WERROR "Treat warnings as errors (new code only, ADR-002)" OFF)
option(GLATE_ENABLE_CLANG_TIDY "Run clang-tidy during the build" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Version)
include(Warnings)

# ---------------------------------------------------------------------------
# Qt. Linux dev builds use 6.11 from the kde-qt6-core24-sdk snap, the Qt the
# snap ships (ADR-001); Windows CI installs 6.10.
# ---------------------------------------------------------------------------
set(GLATE_QT_MIN_VERSION "6.10" CACHE STRING "Minimum required Qt6 version")
find_package(Qt6 ${GLATE_QT_MIN_VERSION} REQUIRED COMPONENTS
    Core Gui Widgets Network Multimedia LinguistTools)

# QHotkey's X11 backend. Found here, as imported targets, so the SDK include
# workaround below covers them too.
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED COMPONENTS xcb)
endif()

include(SnapSdkWorkaround)

if(GLATE_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy requested but not found")
    endif()
endif()

# Standalone reusable account & licence module (Qt6::Core + Qt6::Network only)
add_subdirectory(modules/AccountAndLicense)

# Standalone reusable theming + client-side-decoration module (Qt6 Widgets)
add_subdirectory(modules/AppChrome)

# Offline translation: model downloads and Mozilla's engine (ADR-003)
add_subdirectory(modules/OfflineTranslation)

add_subdirectory(src)

if(GLATE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
