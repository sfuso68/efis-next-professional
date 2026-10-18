#!/usr/bin/env bash
set -Eeuo pipefail
[[ $(id -un) == pi && $(hostname) == pi5 && $(uname -m) == aarch64 ]]
stage=/home/pi/efis-release-mfd-a0be9a5-stage
package=$stage/pfdrec-a0be9a5-main-v1
[[ ! -e "$package" && ! -L "$package" ]]
[[ $(cat "$stage/source/SOURCE_COMMIT") == a0be9a52494ea7ccb5a3890e94d03b671162383b ]]
ctest --test-dir "$stage/build" --output-on-failure -R '^efis_recorder_storage_failure_tests$'
mkdir "$package"
cp -a "$stage/source/." "$package/"
mkdir "$package/bin"
install -m 0755 "$stage/build/efis-pfd-qt" "$package/bin/efis-pfd-qt"
install -m 0755 "$stage/build/efis-recorder" "$package/bin/efis-recorder"
python3 "$package/tools/deployment/build_release_manifest.py" \
  --release-root "$package" --release-id pfdrec-a0be9a5-main-v1 \
  --source-commit a0be9a52494ea7ccb5a3890e94d03b671162383b \
  --built-utc "$(date -u +%Y-%m-%dT%H:%M:%SZ)" --role PFD --role RECORDER \
  --platform RPI5_AARCH64 --simulation-mode \
  --artifact bin/efis-pfd-qt:exec --artifact bin/efis-recorder:exec
python3 "$package/tools/deployment/validate_release_manifest.py" \
  --manifest "$package/release-manifest.json" --release-root "$package" \
  --expected-role PFD --expected-role RECORDER --expected-platform RPI5_AARCH64 --check-files
tar -czf "$stage/pfdrec-a0be9a5-main-v1.tar.gz" -C "$stage" pfdrec-a0be9a5-main-v1
sha256sum "$stage/pfdrec-a0be9a5-main-v1.tar.gz" "$package/release-manifest.json" "$package/bin/efis-recorder"
echo PFD_PACKAGE=PASS
echo BUILD_HOST=pi5
echo ACTIVATION=NOT_RUN_MFD_CANARY_REQUIRED
