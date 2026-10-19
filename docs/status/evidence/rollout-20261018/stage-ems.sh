#!/usr/bin/env bash
set -Eeuo pipefail
# Staging only; never sudo, switch current, or restart a service.
[[ $(id -un) == pi && $(hostname) == efis && $(uname -m) == aarch64 ]]
stage=/home/pi/efis-release-ems-a0be9a5-stage
source_dir=$stage/source
build_dir=$stage/build
release=/home/pi/efis-next-releases/ems-a0be9a5-main-v1
rollback=/home/pi/efis-next-releases/ems-62ba90b-hmi-cas-v1
[[ $(readlink -f /home/pi/efis-next-current) == "$rollback" ]]
[[ $(vcgencmd get_throttled) == throttled=0x0 ]]
pid=$(systemctl show efis-ems.service -p MainPID --value)
[[ "$pid" =~ ^[1-9][0-9]*$ ]]
for path in "$source_dir" "$build_dir" "$release"; do
  [[ ! -e "$path" && ! -L "$path" ]] || { echo "Refusing existing path: $path"; exit 1; }
done
[[ $(df --output=avail -B1 /home/pi | tail -n 1 | tr -d ' ') -ge 8589934592 ]]
echo "0ab89831f0887b1729aea5837feb98d329edb252bfe27e3dcad0b913b8334c15  $stage/efis-next-a0be9a5-arm64.tar.gz" | sha256sum -c --strict
trap 'echo STAGING_FAILED_EVIDENCE_PRESERVED >&2' ERR
date -u
mkdir "$source_dir"
tar -xzf "$stage/efis-next-a0be9a5-arm64.tar.gz" --strip-components=1 -C "$source_dir"
printf '%s\n' a0be9a52494ea7ccb5a3890e94d03b671162383b > "$source_dir/SOURCE_COMMIT"
cmake -S "$source_dir" -B "$build_dir" -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DEFIS_BUILD_QT_RENDERER=OFF -DEFIS_BUILD_EMS_NODE=ON -DEFIS_BUILD_TESTS=ON
cmake --build "$build_dir" --parallel 2 --target efis-ems-qt efis_engine_tests \
  efis_engine_runtime_tests efis_engine_runtime_store_tests efis_edp_v1_tests efis_ems_link_presentation_tests
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build_dir" --output-on-failure \
  -R '^efis_(engine|engine_runtime|engine_runtime_store|edp_v1|ems_link_presentation|ems_state_presentation|ems_responsive_state_matrix)_tests$'
mkdir -p "$release/bin"
cp -a "$source_dir/." "$release/"
install -m 0755 "$build_dir/efis-ems-qt" "$release/bin/efis-ems-qt"
python3 "$release/tools/deployment/build_release_manifest.py" \
  --release-root "$release" --release-id ems-a0be9a5-main-v1 \
  --source-commit a0be9a52494ea7ccb5a3890e94d03b671162383b \
  --built-utc "$(date -u +%Y-%m-%dT%H:%M:%SZ)" --role EMS --platform RPI4_AARCH64 \
  --simulation-mode --artifact bin/efis-ems-qt:exec
for path in "$release" "$rollback"; do
  python3 "$release/tools/deployment/validate_release_manifest.py" \
    --manifest "$path/release-manifest.json" --release-root "$path" \
    --expected-role EMS --expected-platform RPI4_AARCH64 --check-files
done
ldd "$release/bin/efis-ems-qt"
if ldd "$release/bin/efis-ems-qt" | grep -q 'not found'; then exit 1; fi
sha256sum "$release/bin/efis-ems-qt" "$release/release-manifest.json"
[[ $(readlink -f /home/pi/efis-next-current) == "$rollback" ]]
[[ $(systemctl show efis-ems.service -p MainPID --value) == "$pid" ]]
[[ $(systemctl is-active efis-ems.service) == active ]]
[[ $(vcgencmd get_throttled) == throttled=0x0 ]]
systemctl show efis-ems.service -p MainPID -p NRestarts -p ActiveState
vcgencmd measure_temp
echo EMS_STAGING=PASS
echo ACTIVATION=NOT_RUN_PRIVILEGE_GATE
echo FLIGHT_APPROVAL=false
