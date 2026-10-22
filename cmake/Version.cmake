# Derives version metadata for the build.
#
# GLATE_VERSION         -> "4.4.0" (from project(VERSION))
# GLATE_VERSION_SHORT   -> "4.4"
# GLATE_GIT_REVISION    -> short hash or "unknown"
#
# Handed to the code through compile definitions, never string literals in sources.

set(GLATE_VERSION "${PROJECT_VERSION}")
set(GLATE_VERSION_SHORT "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse --short HEAD
        OUTPUT_VARIABLE GLATE_GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT GLATE_GIT_REVISION)
    set(GLATE_GIT_REVISION "unknown")
endif()

message(STATUS "glate ${GLATE_VERSION} (${GLATE_GIT_REVISION})")
