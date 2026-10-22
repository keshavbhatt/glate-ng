# Fetches Mozilla's translation inference engine (mozilla/translations, inference/,
# MPL-2.0) at a pinned commit and adds it to the build (DOCS/DECISIONS.md ADR-003).
#
# Only the inference/ directory and the submodules it needs are fetched, not the
# training pipeline. The checkout lives in GLATE_ENGINE_SOURCE_DIR (outside the
# build tree by default) so a clean build does not fetch again. Set it to an
# existing checkout to build offline, as the Flatpak build must.
#
# Defines the target glate::translations_engine.

set(GLATE_ENGINE_REPOSITORY "https://github.com/mozilla/translations.git"
    CACHE STRING "Git repository of the translation engine")
# 2026-09-25, inference engine v0.6.0 (inference/BERGAMOT_VERSION).
set(GLATE_ENGINE_COMMIT "69455acaecbe8650cdba988dbcf7c10ca20e7c48"
    CACHE STRING "Pinned commit of the translation engine")
set(GLATE_ENGINE_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/translations"
    CACHE PATH "Checkout of the translation engine (fetched when missing)")

set(_glate_engine_submodules
    inference/3rd_party/ssplit-cpp
    inference/marian-fork/src/3rd_party/sentencepiece
    inference/marian-fork/src/3rd_party/intgemm
    inference/marian-fork/src/3rd_party/ruy
    inference/marian-fork/src/3rd_party/onnxjs
    inference/marian-fork/src/3rd_party/simd_utils
)

function(_glate_engine_git)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${GLATE_ENGINE_SOURCE_DIR}"
        RESULT_VARIABLE _result
        OUTPUT_QUIET
        ERROR_VARIABLE _error
    )
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "git ${ARGN} failed in ${GLATE_ENGINE_SOURCE_DIR}:\n${_error}")
    endif()
endfunction()

option(GLATE_ENGINE_PREFETCHED
    "GLATE_ENGINE_SOURCE_DIR already holds the engine at GLATE_ENGINE_COMMIT (offline builds)" OFF)

set(_glate_engine_stamp "${GLATE_ENGINE_SOURCE_DIR}/.glate-fetched-${GLATE_ENGINE_COMMIT}")
if(GLATE_ENGINE_PREFETCHED)
    if(NOT EXISTS "${GLATE_ENGINE_SOURCE_DIR}/inference/CMakeLists.txt")
        message(FATAL_ERROR "GLATE_ENGINE_PREFETCHED is ON but ${GLATE_ENGINE_SOURCE_DIR} has no engine")
    endif()
    set(_glate_revision "${GLATE_ENGINE_SOURCE_DIR}/inference/marian-fork/src/common/git_revision.h")
    if(NOT EXISTS "${_glate_revision}")
        file(WRITE "${_glate_revision}" "#define GIT_REVISION \"${GLATE_ENGINE_COMMIT}\"\n")
    endif()
elseif(NOT EXISTS "${_glate_engine_stamp}")
    find_package(Git REQUIRED)
    message(STATUS "Fetching the translation engine at ${GLATE_ENGINE_COMMIT}")
    file(REMOVE_RECURSE "${GLATE_ENGINE_SOURCE_DIR}")
    file(MAKE_DIRECTORY "${GLATE_ENGINE_SOURCE_DIR}")
    _glate_engine_git(init --quiet)
    _glate_engine_git(remote add origin "${GLATE_ENGINE_REPOSITORY}")
    _glate_engine_git(sparse-checkout set inference)
    _glate_engine_git(fetch --quiet --depth 1 --filter=blob:none origin "${GLATE_ENGINE_COMMIT}")
    _glate_engine_git(checkout --quiet FETCH_HEAD)
    _glate_engine_git(submodule update --init --depth 1 --recommend-shallow -- ${_glate_engine_submodules})
    # ruy builds its bundled cpuinfo; nothing else below the submodules is used.
    _glate_engine_git(-C inference/marian-fork/src/3rd_party/ruy
        submodule update --init --depth 1 -- third_party/cpuinfo)
    # Keep a plain source tree: the engine's CMake runs `git submodule update
    # --recursive` on configure, which would pull eigen, fbgemm and more that
    # this build does not use. Without .git those calls do nothing.
    file(GLOB_RECURSE _glate_engine_gitlinks LIST_DIRECTORIES false "${GLATE_ENGINE_SOURCE_DIR}/*.git")
    foreach(_link IN LISTS _glate_engine_gitlinks)
        get_filename_component(_name "${_link}" NAME)
        if(_name STREQUAL ".git")
            file(REMOVE "${_link}")
        endif()
    endforeach()
    file(REMOVE_RECURSE "${GLATE_ENGINE_SOURCE_DIR}/.git")
    # marian writes common/git_revision.h from `git log`, rebuilt whenever
    # .git/logs/HEAD changes. Write it from the pinned commit and leave a stub
    # (not a valid repository) so the rule is satisfied and never runs.
    file(WRITE "${GLATE_ENGINE_SOURCE_DIR}/.git/logs/HEAD" "")
    file(WRITE "${GLATE_ENGINE_SOURCE_DIR}/inference/marian-fork/src/common/git_revision.h"
        "#define GIT_REVISION \"${GLATE_ENGINE_COMMIT}\"\n")
    file(TOUCH "${_glate_engine_stamp}")
endif()

# Stop the engine's own git calls from walking up into glate's repository.
get_filename_component(_glate_engine_parent "${GLATE_ENGINE_SOURCE_DIR}" DIRECTORY)
set(ENV{GIT_CEILING_DIRECTORIES} "${_glate_engine_parent}")

# Engine build switches. Float matrix products go through ruy so no system BLAS
# is needed; int8 products use intgemm on x86 and ruy on ARM.
set(COMPILE_CPU ON CACHE BOOL "" FORCE)
set(COMPILE_CUDA OFF CACHE BOOL "" FORCE)
set(COMPILE_TESTS OFF CACHE BOOL "" FORCE)
set(COMPILE_PYTHON OFF CACHE BOOL "" FORCE)
set(USE_RUY ON CACHE BOOL "" FORCE)
set(USE_RUY_SGEMM ON CACHE BOOL "" FORCE)
set(USE_FBGEMM OFF CACHE BOOL "" FORCE)
set(USE_MKL OFF CACHE BOOL "" FORCE)
set(USE_NCCL OFF CACHE BOOL "" FORCE)
set(USE_MPI OFF CACHE BOOL "" FORCE)
set(USE_DOXYGEN OFF CACHE BOOL "" FORCE)
set(USE_CCACHE OFF CACHE BOOL "" FORCE)
set(USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(GIT_SUBMODULE OFF CACHE BOOL "" FORCE)
# PCRE2 for the sentence splitter: the system copy on Linux (SDK, runtime and
# Flatpak all carry it); a bundled build elsewhere.
if(WIN32 OR APPLE)
    set(SSPLIT_USE_INTERNAL_PCRE2 ON CACHE BOOL "" FORCE)
else()
    set(SSPLIT_USE_INTERNAL_PCRE2 OFF CACHE BOOL "" FORCE)
endif()
# Portable code: intgemm picks AVX2/AVX512 at run time, so only the baseline is fixed.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(BUILD_ARCH "armv8-a" CACHE STRING "" FORCE)
elseif(MSVC)
    set(BUILD_ARCH "x86-64" CACHE STRING "" FORCE)
else()
    set(BUILD_ARCH "x86-64-v2" CACHE STRING "" FORCE)
endif()

# The engine still declares cmake_minimum_required below 3.5.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(_glate_saved_tidy "${CMAKE_CXX_CLANG_TIDY}")
set(CMAKE_CXX_CLANG_TIDY "")
add_subdirectory("${GLATE_ENGINE_SOURCE_DIR}/inference" "${CMAKE_BINARY_DIR}/translations-engine" EXCLUDE_FROM_ALL SYSTEM)
set(CMAKE_CXX_CLANG_TIDY "${_glate_saved_tidy}")
unset(CMAKE_POLICY_VERSION_MINIMUM)

# marian builds with -Werror (/WX), which newer compilers trip on with false
# positives in the standard library. Warnings in third-party code are not ours to fix.
foreach(_tgt marian bergamot-translator-source)
    if(TARGET ${_tgt})
        get_target_property(_opts ${_tgt} COMPILE_OPTIONS)
        if(_opts)
            list(REMOVE_ITEM _opts -Werror /WX)
            set_target_properties(${_tgt} PROPERTIES COMPILE_OPTIONS "${_opts}")
        endif()
    endif()
endforeach()

add_library(glate_translations_engine INTERFACE)
target_link_libraries(glate_translations_engine INTERFACE bergamot-translator-source)
add_library(glate::translations_engine ALIAS glate_translations_engine)
