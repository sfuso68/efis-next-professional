#!/usr/bin/env bash
# Staging only. No service, current symlink, or system configuration changes.
set -Eeuo pipefail
commit=a0be9a52494ea7ccb5a3890e94d03b671162383b
stage=/home/pi/efis-release-mfd-a0be9a5-stage
archive=$stage/efis-next-a0be9a5-arm64.tar.gz
source_dir=$stage/source
build_dir=$stage/build
release=/home/pi/efis-next-release-mfd-a0be9a5-v1
rollback=/home/pi/efis-next-release-mfd-3432402-v1
[[ $(id -un) == pi && $(hostname) == pi5 && $(uname -m) == aarch64 ]]
[[ $(readlink -f /home/pi/efis-next-current) == "$rollback" ]]
[[ -f "$archive" && ! -L "$archive" ]]
echo "0ab89831f0887b1729aea5837feb98d329edb252bfe27e3dcad0b913b8334c15  $archive" | sha256sum -c --strict
for path in "$source_dir" "$build_dir" "$release"; do
  [[ ! -e "$path" && ! -L "$path" ]] || { echo "Refusing existing path: $path"; exit 1; }
done
free_bytes=$(df --output=avail -B1 /home/pi | tail -n 1 | tr -d ' ')
[[ "$free_bytes" =~ ^[0-9]+$ && "$free_bytes" -ge 8589934592 ]]
trap 'echo STAGING_FAILED_PRESERVED_FOR_DIAGNOSIS >&2' ERR
date -u
vcgencmd get_throttled
systemctl show efis-next-mfd-direct.service -p MainPID -p NRestarts -p ActiveState
mkdir "$source_dir"
tar -xzf "$archive" --strip-components=1 -C "$source_dir"
printf '%s\n' "$commit" > "$source_dir/SOURCE_COMMIT"
cmake -S "$source_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DEFIS_BUILD_QT_RENDERER=ON -DEFIS_REQUIRE_QT_RENDERER=ON -DEFIS_BUILD_TESTS=ON
cmake --build "$build_dir" --parallel 2 --target efis-pfd-qt efis_weather_service_tests efis_pfd_aircraft_profile_tests
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build_dir" --output-on-failure \
  -R '^efis_(weather_service|pfd_aircraft_profile|hmi_golden_tools|avionix_hmi_contract|skyview_responsive)_tests$'
mkdir -p "$release/bin"
cp -a "$source_dir/." "$release/"
install -m 0755 "$build_dir/efis-pfd-qt" "$release/bin/efis-pfd-qt"
export PYTHONPATH="$release"
python3 "$release/tools/deployment/build_release_manifest.py" \
  --release-root "$release" --release-id mfd-a0be9a5-main-v1 --source-commit "$commit" \
  --built-utc "$(date -u +%Y-%m-%dT%H:%M:%SZ)" --role MFD --platform RPI5_AARCH64 \
  --simulation-mode --artifact bin/efis-pfd-qt:exec
for path in "$release" "$rollback"; do
  python3 "$path/tools/deployment/validate_release_manifest.py" \
    --manifest "$path/release-manifest.json" --release-root "$path" \
    --expected-role MFD --expected-platform RPI5_AARCH64 --check-files
done
sha256sum "$release/bin/efis-pfd-qt" "$release/release-manifest.json"
[[ $(readlink -f /home/pi/efis-next-current) == "$rollback" ]]
systemctl show efis-next-mfd-direct.service -p MainPID -p NRestarts -p ActiveState
vcgencmd get_throttled
echo STAGING=PASS
echo ACTIVATION=NOT_RUN_POWER_GATE
echo FLIGHT_APPROVAL=false
