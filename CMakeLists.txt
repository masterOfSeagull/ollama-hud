cmake_minimum_required(VERSION 3.24)

project(OllamaHud VERSION 0.1.0 LANGUAGES CXX)

option(OLLAMA_HUD_BUILD_TESTS "Build native Qt tests" ON)
option(OLLAMA_HUD_HOT_RELOAD "Enable QML debugging and disable the QML cache for live preview builds" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network Concurrent)
get_filename_component(QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)

qt_standard_project_setup(REQUIRES 6.9)

set(OLLAMA_HUD_SOURCES
    native/src/AppController.cpp
    native/src/CaptureService.cpp
    native/src/ChatLogService.cpp
    native/src/OllamaService.cpp
    native/src/SettingsStore.cpp
    native/src/Shortcut.cpp
)

set(OLLAMA_HUD_HEADERS
    native/src/AppController.h
    native/src/CaptureService.h
    native/src/ChatLogService.h
    native/src/OllamaService.h
    native/src/SettingsStore.h
    native/src/Shortcut.h
)

qt_add_executable(OllamaHud
    native/src/main.cpp
    ${OLLAMA_HUD_SOURCES}
    ${OLLAMA_HUD_HEADERS}
)

target_include_directories(OllamaHud PRIVATE native/src)
target_compile_definitions(OllamaHud PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

set(OLLAMA_HUD_QML_MODULE_OPTIONS)
if(OLLAMA_HUD_HOT_RELOAD)
    target_compile_definitions(OllamaHud PRIVATE QT_QML_DEBUG OLLAMA_HUD_HOT_RELOAD)
    list(APPEND OLLAMA_HUD_QML_MODULE_OPTIONS NO_CACHEGEN)
endif()

target_link_libraries(OllamaHud PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Concurrent
)

if(WIN32)
    target_link_libraries(OllamaHud PRIVATE user32)
endif()

qt_add_qml_module(OllamaHud
    URI OllamaHud
    VERSION 1.0
    ${OLLAMA_HUD_QML_MODULE_OPTIONS}
    QML_FILES
        native/qml/OllamaHud/Main.qml
        native/qml/OllamaHud/Overlay.qml
)

file(GLOB_RECURSE GENYDL_QML_RESOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/native/qml/GenyDL/*"
)

qt_add_resources(OllamaHud GenyDLQmlResources
    PREFIX /
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${GENYDL_QML_RESOURCE_FILES}
)

file(GLOB_RECURSE GENYDL_ASSET_RESOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/genydl/resources/*"
)

qt_add_resources(OllamaHud GenyDLAssetResources
    PREFIX /
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/genydl"
    FILES ${GENYDL_ASSET_RESOURCE_FILES}
)

install(TARGETS OllamaHud RUNTIME DESTINATION .)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${Qt6_DIR}/../../../bin" "C:/Qt/*/*/bin"
    )
endif()

if(OLLAMA_HUD_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.9 REQUIRED COMPONENTS Test Qml QuickTest)

    qt_add_executable(ollama_hud_native_tests
        native/tests/native_tests.cpp
        ${OLLAMA_HUD_SOURCES}
        ${OLLAMA_HUD_HEADERS}
    )
    qt_add_qml_module(ollama_hud_native_tests
        URI OllamaHud
        VERSION 1.0
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_qml/OllamaHud"
        QML_FILES
            native/qml/OllamaHud/Main.qml
            native/qml/OllamaHud/Overlay.qml
    )
    target_include_directories(ollama_hud_native_tests PRIVATE native/src)
    target_compile_definitions(ollama_hud_native_tests PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
        PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(ollama_hud_native_tests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Network
        Qt6::Concurrent
        Qt6::Test
        Qt6::Qml
    )
    if(WIN32)
        target_link_libraries(ollama_hud_native_tests PRIVATE user32)
    endif()
    qt_add_resources(ollama_hud_native_tests GenyDLQmlTestResources
        PREFIX /
        BASE "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES ${GENYDL_QML_RESOURCE_FILES}
    )
    qt_add_resources(ollama_hud_native_tests GenyDLAssetTestResources
        PREFIX /
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/genydl"
        FILES ${GENYDL_ASSET_RESOURCE_FILES}
    )
    add_test(NAME ollama_hud_native_tests COMMAND ollama_hud_native_tests)
    set_tests_properties(ollama_hud_native_tests PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:ollama_hud_native_tests>;PATH=path_list_prepend:${QT_BIN_DIR}"
    )
endif()
