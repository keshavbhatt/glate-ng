# Adds the project-wide warning set to a target.
# Usage: glate_set_warnings(<target>)
#
# New code (modules/OfflineTranslation, tests) uses this with GLATE_WERROR in CI.
# The legacy app target gets glate_set_legacy_warnings() instead, which never
# fails the build, until it is cleaned up (DOCS/DECISIONS.md ADR-002).
function(glate_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /w14640 /w14265 /w14062)
        if(GLATE_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
        )
        if(GLATE_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(glate_set_legacy_warnings target)
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()
