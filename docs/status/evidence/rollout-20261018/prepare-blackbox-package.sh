#!/usr/bin/env bash
set -Eeuo pipefail
[[ $(id -un) == pi && $(hostname) == pi5 && $(uname -m) == aarch64 ]]
stage=/home/pi/efis-release-mfd-a0be9a5-stage
package=$stage/blackbox-a0be9a5-main-v1
[[ ! -e "$package" && ! -L "$package" ]]
[[ $(cat "$stage/source/SOURCE_COMMIT") == a0be9a52494ea7ccb5a3890e94d03b671162383b ]]
cmake --build "$stage/build" --parallel 2 --target efis-blackbox-broker
mkdir "$package"
cp -a "$stage/source/." "$package/"
mkdir "$package/bin"
install -m 0755 "$stage/build/efis-blackbox-broker" "$package/bin/efis-blackbox-broker"
export PYTHONPATH="$package"
python3 "$package/tools/deployment/build_release_manifest.py" \
  --release-root "$package" --release-id blackbox-a0be9a5-main-v1 \
  --source-commit a0be9a52494ea7ccb5a3890e94d03b671162383b \
  --built-utc "$(date -u +%Y-%m-%dT%H:%M:%SZ)" --role BLACKBOX \
  --platform RPI5_AARCH64 --simulation-mode --artifact bin/efis-blackbox-broker:exec
python3 "$package/tools/deployment/validate_release_manifest.py" \
  --manifest "$package/release-manifest.json" --release-root "$package" \
  --expected-role BLACKBOX --expected-platform RPI5_AARCH64 --check-files
tar -czf "$stage/blackbox-a0be9a5-main-v1.tar.gz" -C "$stage" blackbox-a0be9a5-main-v1
sha256sum "$stage/blackbox-a0be9a5-main-v1.tar.gz" "$package/release-manifest.json"
echo BLACKBOX_PACKAGE=PASS
echo ACTIVATION=NOT_RUN_POWER_GATE
