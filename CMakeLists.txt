cmake_minimum_required(VERSION 3.24)
project(efis_next VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EFIS_BUILD_TESTS "Build EFIS Next tests" ON)
option(EFIS_BUILD_QT_RENDERER "Build the Qt Quick PFD when Qt is available" ON)
option(EFIS_REQUIRE_QT_RENDERER "Fail configuration if the Qt Quick PFD cannot be built" OFF)

add_library(efis_core
    src/core/alert_engine.cpp
    src/core/flight_model.cpp
    src/core/node_health.cpp
    src/core/snapshot_arbitrator.cpp
    src/core/stasis_monitor.cpp
    src/core/cross_check.cpp
    src/recording/flight_record.cpp
    src/recording/snapshot_recorder.cpp
    src/recording/segmented_recorder.cpp
    src/recording/recorder_status.cpp
    src/display/display_model.cpp
    src/providers/simulation_provider.cpp
    src/providers/engine_simulation_provider.cpp
    src/providers/mavlink_parser.cpp
    src/providers/mavlink_adapter.cpp
    src/io/byte_source.cpp
    src/bench/sensor_bench.cpp
    src/instruments/dial.cpp
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
    set(EFIS_QT_COMPONENTS Core Gui Qml Quick Quick3D QuickControls2 Concurrent Network)
    find_package(Qt6 6.8 QUIET COMPONENTS ${EFIS_QT_COMPONENTS})
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
        # Un fallimento silenzioso qui e' inaccettabile: senza questo target non
        # esiste alcuna interfaccia di volo, e una configurazione riuscita darebbe
        # l'impressione contraria. Si dichiara sempre quale componente manca.
        set(EFIS_QT_MISSING_COMPONENTS "")
        foreach(efis_qt_component IN LISTS EFIS_QT_COMPONENTS)
            find_package(Qt6${efis_qt_component} 6.8 QUIET)
            if(NOT Qt6${efis_qt_component}_FOUND)
                list(APPEND EFIS_QT_MISSING_COMPONENTS "Qt6${efis_qt_component}")
            endif()
        endforeach()

        if(EFIS_QT_MISSING_COMPONENTS)
            set(EFIS_QT_REASON "componenti mancanti nell'installazione Qt: ${EFIS_QT_MISSING_COMPONENTS}")
        else()
            set(EFIS_QT_REASON "Qt 6.8 o superiore non trovato; indicare l'installazione con -DCMAKE_PREFIX_PATH")
        endif()

        set(EFIS_QT_MESSAGE
            "efis-pfd-qt NON verra' costruito: ${EFIS_QT_REASON}. "
            "Gli eseguibili gia' presenti in build/ restano quelli di una configurazione precedente "
            "e non riflettono le modifiche a src/renderers/qt o a qml/. "
            "Installare i componenti mancanti (su Windows con il MaintenanceTool di Qt) "
            "oppure configurare con -DEFIS_BUILD_QT_RENDERER=OFF per una build del solo core.")

        if(EFIS_REQUIRE_QT_RENDERER)
            message(FATAL_ERROR ${EFIS_QT_MESSAGE})
        endif()
        message(WARNING ${EFIS_QT_MESSAGE})
    endif()
endif()

if(EFIS_BUILD_TESTS)
    enable_testing()
    add_executable(efis_core_tests tests/core_tests.cpp)
    target_include_directories(efis_core_tests PRIVATE tests)
    target_link_libraries(efis_core_tests PRIVATE efis_core efis_reference_renderer)
    # Le registrazioni di riferimento restano file binari sul disco invece di
    # essere trasformate in array C++: cosi' si possono riascoltare con gli
    # strumenti MAVLink e confrontare con l'hardware, che e' il motivo per cui
    # sono state acquisite.
    target_compile_definitions(efis_core_tests PRIVATE
        EFIS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME efis_core_tests COMMAND efis_core_tests)

    # Eseguibile separato: il banco prova si alimenta dalla registrazione reale
    # dell'hardware, e tenerlo distinto dai test del core rende evidente quale
    # dei due stia fallendo.
    add_executable(efis_bench_tests tests/bench_tests.cpp)
    target_link_libraries(efis_bench_tests PRIVATE efis_core)
    target_compile_definitions(efis_bench_tests PRIVATE
        EFIS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME efis_bench_tests COMMAND efis_bench_tests)

    # Le protezioni contro il dato plausibile ma falso hanno un eseguibile
    # proprio: verificano una categoria diversa dal calcolo, e tenerle separate
    # fa vedere subito quale delle due e' caduta.
    add_executable(efis_safety_tests tests/safety_tests.cpp)
    target_link_libraries(efis_safety_tests PRIVATE efis_core)
    add_test(NAME efis_safety_tests COMMAND efis_safety_tests)

    # La geometria degli strumenti analogici ha un eseguibile proprio: verifica
    # come un valore viene mostrato, non quanto vale. Una lancetta assente e una
    # ferma su zero contengono la stessa quantita' di inchiostro e due
    # informazioni opposte, e questa e' la categoria che se ne accorge.
    add_executable(efis_instrument_tests tests/instrument_tests.cpp)
    target_link_libraries(efis_instrument_tests PRIVATE efis_core)
    add_test(NAME efis_instrument_tests COMMAND efis_instrument_tests)

    # L'arbitrato ha un eseguibile proprio per la stessa ragione: verifica quale
    # sorgente fornisce una grandezza, non quanto vale. Un guasto qui non sposta
    # un numero, cambia chi lo dice.
    add_executable(efis_arbitration_tests tests/arbitration_tests.cpp)
    target_link_libraries(efis_arbitration_tests PRIVATE efis_core)
    target_compile_definitions(efis_arbitration_tests PRIVATE
        EFIS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME efis_arbitration_tests COMMAND efis_arbitration_tests)
endif()
