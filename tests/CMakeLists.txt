find_package(Qt6 ${GLATE_QT_MIN_VERSION} REQUIRED COMPONENTS Test)

set(GLATE_TEST_ENV
    "QT_QPA_PLATFORM=offscreen"
    "QT_FORCE_STDERR_LOGGING=1"
)

set(GLATE_SNAP_RUNTIME "" CACHE PATH "kf6-core24 runtime snap root for running tests (dev builds)")
set(GLATE_SNAP_RUNTIME_LIB_PATH "" CACHE STRING "LD_LIBRARY_PATH for the snap runtime (dev builds)")
if(GLATE_SNAP_RUNTIME)
    list(APPEND GLATE_TEST_ENV
        "LD_LIBRARY_PATH=${GLATE_SNAP_RUNTIME_LIB_PATH}"
        "QT_PLUGIN_PATH=${GLATE_SNAP_RUNTIME}/usr/lib/x86_64-linux-gnu/qt6/plugins"
    )
endif()

# glate_add_test(<name> <source> <link target> [GUI])
function(glate_add_test name source link_target)
    qt_add_executable(${name} ${source})
    set_target_properties(${name} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
    glate_set_warnings(${name})
    target_link_libraries(${name} PRIVATE ${link_target} Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${GLATE_TEST_ENV}")
    if(ARGC GREATER 3 AND ARGV3 STREQUAL "GUI")
        set_tests_properties(${name} PROPERTIES LABELS "gui" TIMEOUT 120)
    else()
        set_tests_properties(${name} PROPERTIES LABELS "unit" TIMEOUT 30)
    endif()
endfunction()


# modules/OfflineTranslation
glate_add_test(tst_languages tst_languages.cpp offline_translation)
glate_add_test(tst_model_catalog tst_model_catalog.cpp offline_translation)
glate_add_test(tst_model_store tst_model_store.cpp offline_translation)
glate_add_test(tst_engine_config tst_engine_config.cpp offline_translation)
glate_add_test(tst_model_manager tst_model_manager.cpp offline_translation)
# Translates with real models when GLATE_TEST_MODELS points at a directory with
# en-de and de-en installed (scripts/fetch-test-models.sh); skipped otherwise.
glate_add_test(tst_offline_translator tst_offline_translator.cpp offline_translation)
set_tests_properties(tst_offline_translator PROPERTIES TIMEOUT 120)
set(GLATE_TEST_MODELS "${PROJECT_SOURCE_DIR}/third_party/test-models" CACHE PATH
    "Installed en-de and de-en models for tst_offline_translator")
target_compile_definitions(tst_offline_translator PRIVATE GLATE_TEST_MODELS="${GLATE_TEST_MODELS}")

# The languages dialog lives in the app (src/), so the test compiles it directly.
glate_add_test(tst_offline_models_dialog tst_offline_models_dialog.cpp offline_translation GUI)
target_sources(tst_offline_models_dialog PRIVATE
    "${PROJECT_SOURCE_DIR}/src/offlinemodelsdialog.cpp"
    "${PROJECT_SOURCE_DIR}/src/offlinemodelsdialog.h")
target_include_directories(tst_offline_models_dialog PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tst_offline_models_dialog PRIVATE Qt6::Widgets)

# modules/AppShell
glate_add_test(tst_app_shell tst_app_shell.cpp app_shell)
target_compile_definitions(tst_app_shell PRIVATE
    GLATE_METAINFO_PATH="${PROJECT_SOURCE_DIR}/src/com.ktechpit.glate.metainfo.xml"
    GLATE_VERSION="${PROJECT_VERSION}")
glate_add_test(tst_shell_dialogs tst_shell_dialogs.cpp app_shell GUI)

# modules/GlobalShortcut; the portal tests run against a fake portal on a
# private D-Bus daemon (fake_portal.h), never the session's.
glate_add_test(tst_global_shortcut tst_global_shortcut.cpp global_shortcut GUI)
if(UNIX AND NOT APPLE)
    target_link_libraries(tst_global_shortcut PRIVATE Qt6::DBus)
    target_compile_definitions(tst_global_shortcut PRIVATE GLATE_SHORTCUT_PORTAL)
endif()
