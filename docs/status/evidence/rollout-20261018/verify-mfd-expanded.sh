#!/usr/bin/env bash
set -Eeuo pipefail
[[ $(id -un) == pi && $(hostname) == pi5 && $(uname -m) == aarch64 ]]
build=/home/pi/efis-release-mfd-a0be9a5-stage/build
date -u
cmake --build "$build" --parallel 2 --target efis-recorder efis_core_tests efis_safety_tests \
  efis_derived_state_tests efis_freshness_budget_tests efis_ddp_v2_tests efis_edp_v1_tests \
  efis_engine_tests efis_engine_runtime_tests efis_engine_runtime_store_tests efis_ems_link_presentation_tests
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build" --output-on-failure \
  -R '^efis_(core|safety|derived_state|freshness_budget|ddp_v2|edp_v1|engine|engine_runtime|engine_runtime_store|ems_link_presentation|weather_service|pfd_aircraft_profile|hmi_golden_tools|avionix_hmi_contract|skyview_responsive)_tests$'
systemctl show efis-next-mfd-direct.service -p MainPID -p NRestarts -p ActiveState
[[ $(readlink -f /home/pi/efis-next-current) == /home/pi/efis-next-release-mfd-3432402-v1 ]]
vcgencmd get_throttled
vcgencmd measure_temp
echo MFD_EXPANDED_TESTS=PASS
echo ACTIVATION=NOT_RUN_POWER_GATE
