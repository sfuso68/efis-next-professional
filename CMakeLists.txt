cmake_minimum_required(VERSION 3.24)
project(efis_next VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EFIS_BUILD_TESTS "Build EFIS Next tests" ON)
option(EFIS_BUILD_QT_RENDERER "Build the Qt Quick PFD when Qt is available" ON)

add_library(efis_core
    src/core/alert_engine.cpp
    src/core/flight_model.cpp
    src/recording/snapshot_recorder.cpp
    src/recording/segmented_recorder.cpp
    src/recording/recorder_status.cpp
    src/display/display_model.cpp
    src/providers/simulation_provider.cpp
    src/providers/engine_simulation_provider.cpp
)
target_include_directories(efis_core PUBLIC include)
target_compile_features(efis_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(efis_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(efis_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(efis-core src/apps/efis_core_main.cpp)
target_link_libraries(efis-core PRIVATE efis_core)

add_executable(efis-recorder src/apps/efis_recorder_main.cpp)
target_link_libraries(efis-recorder PRIVATE efis_core)

add_library(efis_reference_renderer src/display/svg_pfd_renderer.cpp)
target_include_directories(efis_reference_renderer PUBLIC include)
target_link_libraries(efis_reference_renderer PUBLIC efis_core)

add_executable(efis-render-pfd src/apps/render_pfd_main.cpp)
target_link_libraries(efis-render-pfd PRIVATE efis_reference_renderer)

if(EFIS_BUILD_QT_RENDERER)
    find_package(Qt6 6.8 QUIET COMPONENTS Core Gui Qml Quick Quick3D QuickControls2 Concurrent Network)
    if(Qt6_FOUND)
        qt_standard_project_setup(REQUIRES 6.8)
        qt_add_executable(efis-pfd-qt
            src/renderers/qt/main.cpp
            src/renderers/qt/pfd_controller.cpp
            src/renderers/qt/pfd_controller.hpp
            src/renderers/qt/gpu_map_item.cpp
            src/renderers/qt/gpu_map_item.hpp
            src/renderers/qt/synthetic_vision_item.cpp
            src/renderers/qt/synthetic_vision_item.hpp
            src/renderers/qt/circular_gauge_item.cpp
            src/renderers/qt/circular_gauge_item.hpp
            src/renderers/qt/connectivity_service.cpp
            src/renderers/qt/connectivity_service.hpp
            src/renderers/qt/weather_service.cpp
            src/renderers/qt/weather_service.hpp
        )
        qt_add_qml_module(efis-pfd-qt
            URI EfisNext
            VERSION 1.0
            QML_FILES
                qml/Main.qml
                qml/DataTape.qml
                qml/Hsi.qml
                qml/MaintenanceOverlay.qml
                qml/FlightModeAnnunciator.qml
                qml/SystemStatusStrip.qml
                qml/HeadingTape.qml
                qml/SystemMfd.qml
                qml/CommandRail.qml
                qml/ContextLane.qml
                qml/MissionDock.qml
                qml/FlightCanvas.qml
                qml/AdaptivePfd.qml
                qml/PageSelector.qml
                qml/AdaptiveMissionView.qml
                qml/AdaptiveNavigationView.qml
                qml/AdaptiveFpvView.qml
                qml/AdaptiveShell.qml
                qml/AdaptiveEmsView.qml
                qml/Theme.qml
                qml/GpuMapSurface.qml
                qml/WeatherRadarOverlay.qml
                qml/ProfessionalTape.qml
                qml/ProfessionalFma.qml
                qml/ProfessionalVisionSurface.qml
                qml/Terrain3DSurface.qml
                qml/TerrainTile3D.qml
                qml/ProfessionalSystemsDeck.qml
                qml/ProfessionalCircularGauge.qml
                qml/AdaptiveCanvasView.qml
            RESOURCES
                qml/shaders/terrain_taws.frag
        )
        target_link_libraries(efis-pfd-qt PRIVATE
            efis_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D Qt6::QuickControls2 Qt6::Concurrent Qt6::Network)
    else()
        message(STATUS "Qt 6 not found: efis-pfd-qt will not be built")
    endif()
endif()

if(EFIS_BUILD_TESTS)
    enable_testing()
    add_executable(efis_core_tests tests/core_tests.cpp)
    target_link_libraries(efis_core_tests PRIVATE efis_core efis_reference_renderer)
    add_test(NAME efis_core_tests COMMAND efis_core_tests)
endif()
