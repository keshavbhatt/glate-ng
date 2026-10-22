# Dev builds against the kde-qt6-core24-sdk snap on a non-Ubuntu host: some
# imported targets export the SDK's generic /usr/include (Ubuntu glibc
# headers), which shadow the host toolchain's libc headers and break
# libstdc++. The Qt headers proper live under .../include/<arch>/qt6 and are
# unaffected, but some libraries keep their headers directly
# in that directory. So the directory is swapped for a small farm in the
# build tree that links only the headers this project needs from it.
# Harmless in snapcraft builds (the dir then equals the real sysroot's).
#
# Must be included AFTER every find_package() call.

if(CMAKE_PREFIX_PATH MATCHES "/snap/kde-qt6-core24-sdk/")
    set(_glate_farm "${CMAKE_BINARY_DIR}/sdk-include")
    set(_glate_farmed sqlite3.h sqlite3ext.h unicode zstd.h zstd_errors.h X11 xcb)
    file(MAKE_DIRECTORY "${_glate_farm}")

    get_property(_glate_imported DIRECTORY PROPERTY IMPORTED_TARGETS)
    foreach(_tgt IN LISTS _glate_imported)
        get_target_property(_incs ${_tgt} INTERFACE_INCLUDE_DIRECTORIES)
        if(_incs)
            set(_filtered "")
            foreach(_dir IN LISTS _incs)
                if(_dir MATCHES "/snap/kde-qt6-core24-sdk/[^/]+/usr/include$")
                    foreach(_entry IN LISTS _glate_farmed)
                        if(EXISTS "${_dir}/${_entry}" AND NOT EXISTS "${_glate_farm}/${_entry}")
                            file(CREATE_LINK "${_dir}/${_entry}" "${_glate_farm}/${_entry}" SYMBOLIC)
                        endif()
                    endforeach()
                    list(APPEND _filtered "${_glate_farm}")
                else()
                    list(APPEND _filtered "${_dir}")
                endif()
            endforeach()
            list(REMOVE_DUPLICATES _filtered)
            set_property(TARGET ${_tgt} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${_filtered}")
        endif()
    endforeach()
    unset(_glate_imported)
    unset(_glate_farm)
    unset(_glate_farmed)
    unset(_filtered)
    unset(_incs)
endif()
