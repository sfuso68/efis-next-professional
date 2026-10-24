cmake_minimum_required(VERSION 3.24)
project(efis_next VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EFIS_BUILD_TESTS "Build EFIS Next tests" ON)
option(EFIS_BUILD_QT_RENDERER "Build the Qt Quick PFD when Qt is available" ON)
option(EFIS_REQUIRE_QT_RENDERER "Fail configuration if the Qt Quick PFD cannot be built" OFF)
option(EFIS_BUILD_SIL_WIRE_CHECK "Build offline JSBSim to canonical DDP/EDP consumer check (Qt Core only)" OFF)

if(MSVC)
    # Core, adapters and applications use standard C++ exceptions for
    # fail-closed validation and per-destination fault isolation.
    add_compile_options(/EHsc)
endif()

add_library(efis_core
    src/core/alert_engine.cpp
    src/emergency/emergency_assist.cpp
    src/emergency/emergency_input.cpp
    src/emergency/emergency_audio.cpp
    src/voice/voice_assist.cpp
    src/core/flight_model.cpp
    src/core/node_health.cpp
    src/core/snapshot_arbitrator.cpp
    src/core/flight_source.cpp
    src/core/engine_source.cpp
    src/core/derived_state.cpp
    src/core/engine_runtime.cpp
    src/core/engine_runtime_store.cpp
    src/core/stasis_monitor.cpp
    src/core/cross_check.cpp
    src/recording/flight_record.cpp
    src/recording/snapshot_recorder.cpp
    src/recording/segmented_recorder.cpp
    src/recording/recorder_status.cpp
    src/display/display_model.cpp
    src/display/terrain_projection.cpp
    src/providers/simulation_provider.cpp
    src/providers/simulation_source.cpp
    src/providers/ddp_udp_source.cpp
    src/providers/ddp_udp_publisher.cpp
    src/providers/edp_udp.cpp
    src/providers/engine_simulation_provider.cpp
    src/providers/sensor_adapter_health.cpp
    src/simulation/system_simulator.cpp
    src/simulation/mission_simulator.cpp
    src/providers/mavlink_parser.cpp
    src/providers/mavlink_adapter.cpp
    src/providers/esp32_can_adapter.cpp
    src/io/can_source.cpp
    src/providers/dronecan.cpp
    src/io/socketcan_source.cpp
    src/ingest/mavlink_pipeline.cpp
    src/ingest/recording_session.cpp
    src/ingest/blackbox_publisher_session.cpp
    src/io/byte_source.cpp
    src/io/udp_datagram.cpp
    src/io/readonly_udp_fanout.cpp
    src/bench/sensor_bench.cpp
    src/core/altimetry.cpp
    src/core/flight_dynamics.cpp
    src/core/signal_access.cpp
    src/protocol/ddp_v2.cpp
    src/protocol/ddp_session.cpp
    src/protocol/edp_v1.cpp
    src/instruments/dial.cpp
    src/instruments/panel.cpp
    src/instruments/panel_store.cpp
    src/instruments/horizon.cpp
    src/instruments/svg_dial_renderer.cpp
)
target_include_directories(efis_core PUBLIC include)
target_compile_features(efis_core PUBLIC cxx_std_20)
if(WIN32)
    target_link_libraries(efis_core PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(efis_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(efis_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(efis-core src/apps/efis_core_main.cpp)
target_link_libraries(efis-core PRIVATE efis_core)

add_executable(efis-can-monitor src/apps/efis_can_monitor_main.cpp)
target_link_libraries(efis-can-monitor PRIVATE efis_core)

add_executable(efis-recorder src/apps/efis_recorder_main.cpp)
target_link_libraries(efis-recorder PRIVATE efis_core)

add_executable(efis-simulator src/apps/efis_simulator_main.cpp)
target_link_libraries(efis-simulator PRIVATE efis_core)
add_executable(efis-mission-simulator src/apps/mission_simulator_main.cpp)
target_link_libraries(efis-mission-simulator PRIVATE efis_core)

add_executable(efis-ddp-bench-publisher src/apps/efis_ddp_bench_publisher_main.cpp)
target_link_libraries(efis-ddp-bench-publisher PRIVATE efis_core)

add_executable(efis-blackbox-broker src/apps/efis_blackbox_broker_main.cpp)
target_link_libraries(efis-blackbox-broker PRIVATE efis_core)

add_library(efis_reference_renderer src/display/svg_pfd_renderer.cpp)
target_include_directories(efis_reference_renderer PUBLIC include)
target_link_libraries(efis_reference_renderer PUBLIC efis_core)

add_executable(efis-render-pfd src/apps/render_pfd_main.cpp)
target_link_libraries(efis-render-pfd PRIVATE efis_reference_renderer)

# Produce quadranti in SVG per poterli guardare mentre si scrivono, e in
# seguito per confrontarli con le immagini di riferimento.
add_executable(efis-render-dials src/apps/render_dials_main.cpp)
target_link_libraries(efis-render-dials PRIVATE efis_core)

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
            src/renderers/qt/terrain_projection_adapter.cpp
            src/renderers/qt/terrain_projection_adapter.hpp
            src/renderers/qt/circular_gauge_item.cpp
            src/renderers/qt/circular_gauge_item.hpp
            src/renderers/qt/connectivity_service.cpp
            src/renderers/qt/connectivity_service.hpp
            src/renderers/qt/weather_service.cpp
            src/renderers/qt/weather_service.hpp
            src/renderers/qt/sensor_bench_controller.cpp
            src/renderers/qt/sensor_bench_controller.hpp
            src/setup/operational_profile_model.cpp
            src/setup/operational_profile_model.hpp
        )
        qt_add_qml_module(efis-pfd-qt
            URI EfisNext
            VERSION 1.0
            QML_FILES
                qml/Main.qml
                qml/DataTape.qml
                qml/Hsi.qml
                qml/MaintenanceOverlay.qml
                qml/SensorBenchPanel.qml
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
        qml/EmsStatusHeader.qml
                qml/EmsSetupOverlay.qml
                qml/OperationalProfileSetup.qml
                qml/EfisTouchKeyboard.qml
                qml/Theme.qml
                qml/HmiTokens.qml
                qml/HmiDataStateBadge.qml
                qml/HmiArcGaugeV4.qml
                qml/HmiCylinderMatrixV2.qml
                qml/HmiAirframePanel.qml
                qml/HmiFuelStatus.qml
                qml/HmiElectricalSynoptic.qml
                qml/SkyViewCompassArc.qml
                qml/HmiComponentShowcase.qml
                qml/HmiGoldenController.qml
                qml/HmiGoldenMain.qml
                qml/SkyViewGoldenMain.qml
                qml/GpuMapSurface.qml
                qml/WeatherRadarOverlay.qml
                qml/ProfessionalTape.qml
                qml/ProfessionalFma.qml
                qml/ProfessionalVisionSurface.qml
                qml/Terrain3DSurface.qml
                qml/TerrainTile3D.qml
                qml/ProfessionalSystemsDeck.qml
                qml/ProfessionalCircularGauge.qml
                qml/EmsGaugeV3.qml
                qml/EngineTemperatureBars.qml
                qml/EmsVectorIcon.qml
                qml/AdaptiveCanvasView.qml
                qml/SystemSimulatorOverlay.qml
                qml/EmergencyFlightSupport.qml
                qml/EmergencyAssistView.qml
                qml/RadioAssistView.qml
            RESOURCES
                qml/shaders/terrain_taws.frag
        )
        target_link_libraries(efis-pfd-qt PRIVATE
            efis_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D Qt6::QuickControls2 Qt6::Concurrent Qt6::Network)

        if(EFIS_BUILD_TESTS)
            qt_add_executable(efis_weather_service_tests
                tests/weather_service_tests.cpp
                src/renderers/qt/weather_service.cpp
                src/renderers/qt/weather_service.hpp
            )
            target_include_directories(efis_weather_service_tests PRIVATE
                src/renderers/qt)
            target_compile_definitions(efis_weather_service_tests PRIVATE
                EFIS_WEATHER_TEST_HOOKS=1)
            target_link_libraries(efis_weather_service_tests PRIVATE
                Qt6::Core Qt6::Gui Qt6::Network)
            add_test(NAME efis_weather_service_tests
                     COMMAND efis_weather_service_tests)

            qt_add_executable(efis_pfd_aircraft_profile_tests
                tests/pfd_aircraft_profile_tests.cpp
                src/renderers/qt/pfd_controller.cpp
                src/renderers/qt/pfd_controller.hpp
                src/setup/operational_profile_model.cpp
                src/setup/operational_profile_model.hpp
            )
            target_include_directories(efis_pfd_aircraft_profile_tests PRIVATE
                src/renderers/qt src/setup)
            target_link_libraries(efis_pfd_aircraft_profile_tests PRIVATE
                efis_core Qt6::Core Qt6::Network)
            add_test(NAME efis_pfd_aircraft_profile_tests
                     COMMAND efis_pfd_aircraft_profile_tests)
        endif()

        # Pagina analogica come applicazione a se'. Gli strumenti entreranno nel
        # selettore con la fase 3, quando esistera' la configurazione che li
        # dispone; fino ad allora serve poterli vedere sul display vero, che e'
        # l'unica prova valida per una modifica di interfaccia.
        qt_add_executable(efis-instruments-qt
            src/apps/instruments_qt_main.cpp
            src/renderers/qt/instrument_item.cpp
            src/renderers/qt/instrument_item.hpp
            src/renderers/qt/instrument_source.cpp
            src/renderers/qt/instrument_source.hpp
        )
        qt_add_qml_module(efis-instruments-qt
            URI EfisInstruments
            VERSION 1.0
            QML_FILES
                qml/InstrumentsMain.qml
                qml/AnalogSixPack.qml
        )
        target_include_directories(efis-instruments-qt PRIVATE src/renderers/qt)
        target_link_libraries(efis-instruments-qt PRIVATE
            efis_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)

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

    add_executable(efis_derived_state_tests tests/derived_state_tests.cpp)
    target_link_libraries(efis_derived_state_tests PRIVATE efis_core)
    add_test(NAME efis_derived_state_tests COMMAND efis_derived_state_tests)

    add_executable(efis_freshness_budget_tests tests/freshness_budget_tests.cpp)
    target_link_libraries(efis_freshness_budget_tests PRIVATE efis_core)
    add_test(NAME efis_freshness_budget_tests COMMAND efis_freshness_budget_tests)

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
    # DroneCAN ha un eseguibile proprio: verifica un protocollo e il suo
    # riassemblaggio multi-frame, categoria diversa dal calcolo di una grandezza.
    add_executable(efis_dronecan_tests tests/dronecan_tests.cpp)
    target_link_libraries(efis_dronecan_tests PRIVATE efis_core)
    target_compile_definitions(efis_dronecan_tests PRIVATE
        EFIS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME efis_dronecan_tests COMMAND efis_dronecan_tests)

    # La geometria degli strumenti analogici ha un eseguibile proprio: verifica
    # come un valore viene mostrato, non quanto vale. Una lancetta assente e una
    # ferma su zero contengono la stessa quantita' di inchiostro e due
    # informazioni opposte, e questa e' la categoria che se ne accorge.
    add_executable(efis_instrument_tests tests/instrument_tests.cpp)
    target_link_libraries(efis_instrument_tests PRIVATE efis_core)
    add_test(NAME efis_instrument_tests COMMAND efis_instrument_tests)

    add_executable(efis_terrain_projection_tests tests/terrain_projection_tests.cpp)
    target_link_libraries(efis_terrain_projection_tests PRIVATE efis_core)
    add_test(NAME efis_terrain_projection_tests COMMAND efis_terrain_projection_tests)

    add_executable(efis_safety_tests tests/safety_tests.cpp)
    target_link_libraries(efis_safety_tests PRIVATE efis_core)
    add_test(NAME efis_safety_tests COMMAND efis_safety_tests)

    add_executable(efis_emergency_assist_tests tests/emergency_assist_tests.cpp)
    target_link_libraries(efis_emergency_assist_tests PRIVATE efis_core)
    add_test(NAME efis_emergency_assist_tests COMMAND efis_emergency_assist_tests)

    add_executable(efis_emergency_io_tests tests/emergency_io_tests.cpp)
    target_link_libraries(efis_emergency_io_tests PRIVATE efis_core)
    add_test(NAME efis_emergency_io_tests COMMAND efis_emergency_io_tests)

    add_executable(efis_voice_assist_tests tests/voice_assist_tests.cpp)
    target_link_libraries(efis_voice_assist_tests PRIVATE efis_core)
    add_test(NAME efis_voice_assist_tests COMMAND efis_voice_assist_tests)

    find_package(Python3 QUIET COMPONENTS Interpreter)
    set(EFIS_EMS_GOLDEN_GATE_REGISTERED OFF)
    if(NOT (Python3_Interpreter_FOUND AND TARGET efis-pfd-qt))
        message(STATUS
            "EFIS EMS golden rerender gate: NOT REGISTERED "
            "(requires Python3 and target efis-pfd-qt).")
    endif()
    if(Python3_Interpreter_FOUND)
        add_test(NAME efis_edge_weather_cache_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/server/edge-node/test_edge_weather.py)
        set_tests_properties(efis_edge_weather_cache_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/server/edge-node)
        add_test(NAME efis_emergency_event_transport_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/blackbox/test_emergency_event_transport.py)
        add_test(NAME efis_voice_replay_adapter_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/voice/test_vosk_replay_adapter.py)
        add_test(NAME efis_mavlink_observer_policy_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mavlink_observer_gateway.py)
        add_test(NAME efis_mavlink_observer_sink_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mavlink_observer_sink.py)
        add_test(NAME efis_qgc_monitor_preflight_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qgc_monitor_preflight.py)
        add_test(NAME efis_udp_port_holder_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_udp_port_holder.py)
        add_test(NAME efis_mavlink_observer_soak_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mavlink_observer_soak.py)
        add_test(NAME efis_qgc_monitor_status_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qgc_monitor_status.py)
        add_test(NAME efis_edp_passive_observer_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_capture_edp_observer.py)
        add_test(NAME efis_edp_capture_assessment_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_assess_edp_capture.py)
        add_test(NAME efis_edp_replay_session_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_replay_edp_session.py)
        add_test(NAME efis_hmi_golden_tools_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_hmi_golden_tools)
        set_tests_properties(efis_hmi_golden_tools_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_msfs_readonly_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_msfs_readonly)
        set_tests_properties(efis_msfs_readonly_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_jsbsim_session_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_jsbsim_session)
        set_tests_properties(efis_jsbsim_session_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_avionix_hmi_contract_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_avionix_hmi_contract)
        set_tests_properties(efis_avionix_hmi_contract_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_skyview_responsive_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_skyview_responsive)
        set_tests_properties(efis_skyview_responsive_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_ems_state_presentation_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_ems_state_presentation)
        set_tests_properties(efis_ems_state_presentation_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_ems_responsive_state_matrix_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_ems_responsive_state_matrix)
        set_tests_properties(efis_ems_responsive_state_matrix_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        if(TARGET efis-pfd-qt AND WIN32)
            # The committed manifest declares a Windows reference. Running a
            # pixel gate through another font/graphics backend would measure
            # platform rasterisation instead of HMI regression.
            add_test(NAME efis_ems_golden_rerender_tests
                COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/ui/verify_ems_golden_rerender.ps1
                    -Python ${Python3_EXECUTABLE}
                    -Executable $<TARGET_FILE:efis-pfd-qt>
                    -ReferenceDir ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/ems-responsive-states)
            set_tests_properties(efis_ems_golden_rerender_tests PROPERTIES
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                TIMEOUT 180)
            set(EFIS_EMS_GOLDEN_GATE_REGISTERED ON)
        elseif(TARGET efis-pfd-qt AND UNIX AND
               EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/ems-responsive-states-linux/manifest.json")
            # Linux owns a separate reference because platform font and graphics
            # rasterisation must never be compared against the Windows baseline.
            add_test(NAME efis_ems_golden_rerender_tests
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/ui/verify_ems_golden_rerender.py
                    --executable $<TARGET_FILE:efis-pfd-qt>
                    --reference-dir ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/ems-responsive-states-linux
                    --output-dir ${CMAKE_CURRENT_BINARY_DIR}/linux-golden-rerender
                    --platform offscreen
                    --delay-ms 1200)
            set_tests_properties(efis_ems_golden_rerender_tests PROPERTIES
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                ENVIRONMENT "QT_SCALE_FACTOR=1;QSG_RHI_BACKEND=opengl;LIBGL_ALWAYS_SOFTWARE=1"
                TIMEOUT 240)
            set(EFIS_EMS_GOLDEN_GATE_REGISTERED ON)
        elseif(TARGET efis-pfd-qt AND UNIX)
            message(STATUS
                "EFIS EMS golden rerender gate: NOT REGISTERED "
                "(Linux reference tests/golden/ems-responsive-states-linux/manifest.json is absent).")
        endif()
        add_test(NAME efis_hmi_hardware_acceptance_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_hmi_hardware_acceptance)
        set_tests_properties(efis_hmi_hardware_acceptance_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_hmi_hardware_evidence_assembler_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_assemble_hmi_hardware_evidence)
        set_tests_properties(efis_hmi_hardware_evidence_assembler_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_hmi_hardware_session_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_manage_hmi_hardware_session)
        set_tests_properties(efis_hmi_hardware_session_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_synthetic_vision_benchmark_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_synthetic_vision_benchmark)
        set_tests_properties(efis_synthetic_vision_benchmark_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_synthetic_vision_session_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_manage_synthetic_vision_session)
        set_tests_properties(efis_synthetic_vision_session_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_synthetic_vision_run_assembler_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_assemble_synthetic_vision_run)
        set_tests_properties(efis_synthetic_vision_run_assembler_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_g1_sensor_node_capture_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_capture_g1_sensor_node)
        set_tests_properties(efis_g1_sensor_node_capture_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_g1_physical_inventory_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_g1_physical_inventory)
        set_tests_properties(efis_g1_physical_inventory_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_g1_visual_inventory_import_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_g1_visual_inventory_import)
        set_tests_properties(efis_g1_visual_inventory_import_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_g1_upstream_proposal_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_g1_upstream_proposal)
        set_tests_properties(efis_g1_upstream_proposal_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_g1_switch_acceptance_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_g1_switch_acceptance)
        set_tests_properties(efis_g1_switch_acceptance_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_g1_g2_readiness_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_g1_g2_readiness)
        set_tests_properties(efis_g1_g2_readiness_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_avionix_icd_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_avionix_icd)
        set_tests_properties(efis_avionix_icd_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_release_manifest_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_release_manifest)
        set_tests_properties(efis_release_manifest_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_release_supervision_asset_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_release_supervision_assets)
        set_tests_properties(efis_release_supervision_asset_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_release_preparation_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_release_preparation)
        set_tests_properties(efis_release_preparation_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_bbxnav_supervision_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_bbxnav_supervision)
        set_tests_properties(efis_bbxnav_supervision_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_blackbox_broker_observer_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_blackbox_broker_observer.py
                $<TARGET_FILE:efis-blackbox-broker>)
        add_test(NAME efis_mavlink_readonly_forwarder_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest
                tests.test_mavlink_readonly_forwarder)
        set_tests_properties(efis_mavlink_readonly_forwarder_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME efis_recorder_storage_failure_tests
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_recorder_storage_failure.py
                $<TARGET_FILE:efis-recorder>)
    endif()

    add_executable(efis_ingest_tests tests/ingest_tests.cpp)
    target_link_libraries(efis_ingest_tests PRIVATE efis_core)
    target_compile_definitions(efis_ingest_tests PRIVATE
        EFIS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME efis_ingest_tests COMMAND efis_ingest_tests)

    add_executable(efis_esp32_tests tests/esp32_tests.cpp)
    target_link_libraries(efis_esp32_tests PRIVATE efis_core)
    add_test(NAME efis_esp32_tests COMMAND efis_esp32_tests)

    add_executable(efis_can_source_tests tests/can_source_tests.cpp)
    target_link_libraries(efis_can_source_tests PRIVATE efis_core)
    target_compile_definitions(efis_can_source_tests PRIVATE
        EFIS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME efis_can_source_tests COMMAND efis_can_source_tests)

    # L'arbitrato ha un eseguibile proprio per la stessa ragione: verifica quale
    # sorgente fornisce una grandezza, non quanto vale. Un guasto qui non sposta
    # un numero, cambia chi lo dice.
    add_executable(efis_arbitration_tests tests/arbitration_tests.cpp)
    target_link_libraries(efis_arbitration_tests PRIVATE efis_core)
    target_compile_definitions(efis_arbitration_tests PRIVATE
        EFIS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME efis_arbitration_tests COMMAND efis_arbitration_tests)

    add_executable(efis_system_simulator_tests tests/system_simulator_tests.cpp)
    target_link_libraries(efis_system_simulator_tests PRIVATE efis_core)
    add_test(NAME efis_system_simulator_tests COMMAND efis_system_simulator_tests)
    add_executable(efis_mission_simulator_tests tests/mission_simulator_tests.cpp)
    target_link_libraries(efis_mission_simulator_tests PRIVATE efis_core)
    add_test(NAME efis_mission_simulator_tests COMMAND efis_mission_simulator_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME efis_mission_simulator_cli_tests
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/mission_simulator_cli_tests.py"
                "$<TARGET_FILE:efis-mission-simulator>")
        set_tests_properties(efis_mission_simulator_cli_tests PROPERTIES TIMEOUT 60)
    endif()

    add_executable(efis_ddp_v2_tests tests/ddp_v2_tests.cpp)
    target_link_libraries(efis_ddp_v2_tests PRIVATE efis_core)
    add_test(NAME efis_ddp_v2_tests COMMAND efis_ddp_v2_tests)

    add_executable(efis_ddp_udp_tests tests/ddp_udp_tests.cpp)
    target_link_libraries(efis_ddp_udp_tests PRIVATE efis_core)
    add_test(NAME efis_ddp_udp_tests COMMAND efis_ddp_udp_tests)

    add_executable(efis_mavlink_observer_fanout_tests
        tests/mavlink_observer_fanout_tests.cpp)
    target_link_libraries(efis_mavlink_observer_fanout_tests PRIVATE efis_core)
    add_test(NAME efis_mavlink_observer_fanout_tests
        COMMAND efis_mavlink_observer_fanout_tests)

    add_executable(efis_edp_v1_tests tests/edp_v1_tests.cpp)
    target_link_libraries(efis_edp_v1_tests PRIVATE efis_core)
    add_test(NAME efis_edp_v1_tests COMMAND efis_edp_v1_tests)

    add_executable(efis_avionix_icd_vector_tests
        tests/avionix_icd_vector_tests.cpp)
    target_link_libraries(efis_avionix_icd_vector_tests PRIVATE efis_core)
    target_compile_definitions(efis_avionix_icd_vector_tests PRIVATE
        EFIS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME efis_avionix_icd_vector_tests
        COMMAND efis_avionix_icd_vector_tests)
endif()

# --- Nodo EMS ---------------------------------------------------------------
# La sezione motore come processo a se', per il RPi4. Non e' una variante ridotta
# del PFD: e' il confine di modulo della ADR-002 portato a confine di processo.
# Da qui discendono i due vincoli di questo blocco. Primo, l'elenco dei
# componenti Qt e' suo e non tocca Quick3D, Network e Concurrent: un nodo motore
# non ha terreno da rasterizzare ne' meteo da scaricare, e chiederli lo
# renderebbe incostruibile dove servono di meno. Secondo, i file QML sono gli
# stessi del PFD, non copie: una modifica alla pagina motore fatta su questo nodo
# e' gia' nel display completo, che e' la ragione per cui il nodo esiste.
option(EFIS_BUILD_EMS_NODE "Build the standalone EMS engine node when Qt is available" ON)

if(EFIS_BUILD_EMS_NODE)
    set(EFIS_EMS_QT_COMPONENTS Core Gui Qml Quick QuickControls2)
    find_package(Qt6 6.8 QUIET COMPONENTS ${EFIS_EMS_QT_COMPONENTS})
    if(Qt6_FOUND)
        qt_standard_project_setup(REQUIRES 6.8)
        qt_add_executable(efis-ems-qt
            src/nodes/ems/main.cpp
            src/nodes/ems/ems_controller.cpp
            src/nodes/ems/ems_controller.hpp
            src/nodes/ems/capture_request.cpp
            src/nodes/ems/capture_request.hpp
            src/renderers/qt/circular_gauge_item.cpp
            src/renderers/qt/circular_gauge_item.hpp
            src/setup/operational_profile_model.cpp
            src/setup/operational_profile_model.hpp
        )
        qt_add_qml_module(efis-ems-qt
            URI EfisNextEms
            VERSION 1.0
            QML_FILES
                qml/EmsMain.qml
                qml/AdaptiveEmsView.qml
                qml/EmsStatusHeader.qml
                qml/CommandRail.qml
                qml/ProfessionalCircularGauge.qml
                qml/EmsGaugeV3.qml
                qml/EngineTemperatureBars.qml
                qml/EmsVectorIcon.qml
                qml/EmsSetupOverlay.qml
                qml/OperationalProfileSetup.qml
                qml/EfisTouchKeyboard.qml
                qml/Theme.qml
                qml/HmiTokens.qml
                qml/HmiDataStateBadge.qml
                qml/HmiArcGaugeV4.qml
                qml/HmiCylinderMatrixV2.qml
                qml/HmiAirframePanel.qml
                qml/HmiFuelStatus.qml
                qml/HmiElectricalSynoptic.qml
        )
        target_include_directories(efis-ems-qt PRIVATE
            src/nodes/ems src/renderers/qt)
        target_link_libraries(efis-ems-qt PRIVATE
            efis_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
    else()
        # Stesso principio del renderer completo: se il nodo non si costruisce va
        # detto, perche' un binario vecchio in build/ e una configurazione
        # riuscita insieme fanno credere che la modifica sia stata provata.
        message(WARNING
            "efis-ems-qt NON verra' costruito: Qt 6.8 con ${EFIS_EMS_QT_COMPONENTS} non trovato. "
            "Indicare l'installazione con -DCMAKE_PREFIX_PATH oppure configurare con "
            "-DEFIS_BUILD_EMS_NODE=OFF.")
    endif()
endif()

if(EFIS_BUILD_TESTS)
    # Le prove del motore hanno un eseguibile proprio perche' il nodo EMS e' un
    # processo proprio: quando cade, deve dire che e' caduta la sezione motore e
    # non "il core".
    add_executable(efis_engine_tests tests/engine_tests.cpp)
    target_link_libraries(efis_engine_tests PRIVATE efis_core)
    add_test(NAME efis_engine_tests COMMAND efis_engine_tests)

    add_executable(efis_ems_link_presentation_tests
        tests/ems_link_presentation_tests.cpp)
    target_link_libraries(efis_ems_link_presentation_tests PRIVATE efis_core)
    add_test(NAME efis_ems_link_presentation_tests
        COMMAND efis_ems_link_presentation_tests)

    if(TARGET Qt6::Quick)
        find_program(EFIS_QMLTESTRUNNER NAMES qmltestrunner
            HINTS "${Qt6_DIR}/../../../bin" REQUIRED)
        add_test(NAME efis_ems_engine_state_qml_tests
            COMMAND "${EFIS_QMLTESTRUNNER}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml"
                -import "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                -o "-,txt")
        set_tests_properties(efis_ems_engine_state_qml_tests PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
            TIMEOUT 60)
    endif()

    add_executable(efis_engine_runtime_tests tests/engine_runtime_tests.cpp)
    target_link_libraries(efis_engine_runtime_tests PRIVATE efis_core)
    add_test(NAME efis_engine_runtime_tests COMMAND efis_engine_runtime_tests)

    add_executable(efis_engine_runtime_store_tests tests/engine_runtime_store_tests.cpp)
    target_link_libraries(efis_engine_runtime_store_tests PRIVATE efis_core)
    add_test(NAME efis_engine_runtime_store_tests COMMAND efis_engine_runtime_store_tests)

    add_executable(efis_sensor_adapter_health_tests
        tests/sensor_adapter_health_tests.cpp)
    target_link_libraries(efis_sensor_adapter_health_tests PRIVATE efis_core)
    add_test(NAME efis_sensor_adapter_health_tests
        COMMAND efis_sensor_adapter_health_tests)

    if(TARGET Qt6::Core)
        add_executable(efis_operational_profile_model_tests
            tests/operational_profile_model_tests.cpp
            src/setup/operational_profile_model.cpp
            src/setup/operational_profile_model.hpp)
        target_include_directories(efis_operational_profile_model_tests PRIVATE src/setup)
        target_link_libraries(efis_operational_profile_model_tests PRIVATE Qt6::Core)
        target_compile_definitions(efis_operational_profile_model_tests PRIVATE
            EFIS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME efis_operational_profile_model_tests COMMAND efis_operational_profile_model_tests)
    endif()
endif()

if(EFIS_BUILD_SIL_WIRE_CHECK)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    add_executable(efis-sil-wire-check tools/simulation/jsbsim_wire_check.cpp)
    target_link_libraries(efis-sil-wire-check PRIVATE efis_core Qt6::Core)
    if(EFIS_BUILD_TESTS AND Python3_Interpreter_FOUND)
        add_test(NAME efis_jsbsim_wire_check_tests
            COMMAND ${Python3_EXECUTABLE} -m tests.jsbsim_wire_check_tests $<TARGET_FILE:efis-sil-wire-check>)
        set_tests_properties(efis_jsbsim_wire_check_tests PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} TIMEOUT 60)
    endif()
endif()
