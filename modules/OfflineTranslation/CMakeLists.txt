# Offline translation: model catalogue, downloads, on-disk store and the engine
# wrapper (DOCS/DECISIONS.md ADR-003, ADR-004). Held to the strict standards (ADR-002).

option(GLATE_OFFLINE_ENGINE "Build Mozilla's translation engine for offline mode" ON)

qt_add_library(offline_translation STATIC
    EngineConfig.cpp
    EngineConfig.h
    Languages.cpp
    Languages.h
    ModelCatalog.cpp
    ModelCatalog.h
    ModelManager.cpp
    ModelManager.h
    ModelStore.cpp
    ModelStore.h
    OfflineTranslator.cpp
    OfflineTranslator.h
    TranslationEngine.cpp
    TranslationEngine.h
)
set_target_properties(offline_translation PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    AUTOMOC ON
)
glate_set_warnings(offline_translation)

target_include_directories(offline_translation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(offline_translation PUBLIC Qt6::Core Qt6::Network)

# Qt hygiene, as in omnidict: explicit string conversions, no keyword macros.
target_compile_definitions(offline_translation PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_KEYWORDS
    QT_NO_FOREACH
    QT_USE_QSTRINGBUILDER
)

if(MSVC)
    target_compile_options(offline_translation PRIVATE /utf-8)
endif()

if(GLATE_OFFLINE_ENGINE)
    include(FetchTranslationsEngine)
    # Only TranslationEngine.cpp sees the engine's headers; they are not
    # warning-clean, so they come in as system headers.
    target_link_libraries(offline_translation PRIVATE glate::translations_engine)
    target_compile_definitions(offline_translation PUBLIC GLATE_HAVE_TRANSLATIONS_ENGINE)
    # The engine's headers (marian's bundled spdlog) use constructor forms that
    # C++20 removed; GCC 13, as on Ubuntu 24.04 and core24, rejects them. The one
    # file that includes them builds as C++17.
    if(MSVC)
        set_source_files_properties(TranslationEngine.cpp PROPERTIES COMPILE_OPTIONS "/std:c++17")
    else()
        set_source_files_properties(TranslationEngine.cpp PROPERTIES COMPILE_OPTIONS "-std=c++17")
    endif()
endif()
