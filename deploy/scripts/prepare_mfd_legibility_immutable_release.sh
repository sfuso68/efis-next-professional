#!/usr/bin/env bash
# Build and stage the reviewed PFD-legibility candidate on the MFD RPi5.
# This script never changes the active symlink or restarts a service.
set -Eeuo pipefail

source_commit=76afead639599a08cb187bf93f1127c84a4df9ed
source_sha256=b318d9da3914826cdc4c5a07b5803be25d17973b4685df64633646a053ee740b
release_id=mfd-76afead-legibility-v1
stage=/home/pi/efis-release-mfd-legibility-stage
archive="$stage/efis-next-76afead.tar.gz"
source_dir="$stage/src-76afead"
build_dir="$stage/build-76afead"
release=/home/pi/efis-next-release-mfd-76afead-v1
rollback=/home/pi/efis-next-release-mfd-3432402-v1
current=/home/pi/efis-next-current

[[ ${EUID} -ne 0 ]] || { echo "run as user pi, not root" >&2; exit 1; }
[[ $(id -un) == pi ]] || { echo "wrong user: expected pi" >&2; exit 1; }
[[ $(hostname) == pi5 ]] || { echo "wrong node: expected pi5" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ $(readlink -f "$current") == "$rollback" ]] || {
  echo "unexpected active release; expected $rollback" >&2
  exit 1
}
[[ -f "$archive" && ! -L "$archive" ]] || {
  echo "missing source archive: $archive" >&2
  exit 1
}
echo "$source_sha256  $archive" | sha256sum --check --strict
[[ ! -e "$source_dir" && ! -L "$source_dir" ]] || {
  echo "refusing to replace source staging path: $source_dir" >&2
  exit 1
}
[[ ! -e "$build_dir" && ! -L "$build_dir" ]] || {
  echo "refusing to replace build staging path: $build_dir" >&2
  exit 1
}
[[ ! -e "$release" && ! -L "$release" ]] || {
  echo "refusing to replace release: $release" >&2
  exit 1
}

free_bytes=$(df --output=avail -B1 /home/pi | tail -n 1 | tr -d ' ')
[[ "$free_bytes" =~ ^[0-9]+$ && "$free_bytes" -ge 8589934592 ]] || {
  echo "less than the mandatory 8 GiB filesystem reserve" >&2
  exit 1
}

cleanup_on_error() {
  local rc=$?
  if [[ $rc -ne 0 ]]; then
    rm -rf -- "$source_dir" "$build_dir" "$release"
  fi
  exit "$rc"
}
trap cleanup_on_error EXIT

mkdir -p "$source_dir"
tar -xzf "$archive" --strip-components=1 -C "$source_dir"
printf '%s\n' "$source_commit" > "$source_dir/SOURCE_COMMIT"

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DEFIS_BUILD_QT_RENDERER=ON \
  -DEFIS_REQUIRE_QT_RENDERER=ON \
  -DEFIS_BUILD_TESTS=ON
cmake --build "$build_dir" --target efis-pfd-qt
ctest --test-dir "$build_dir" --output-on-failure \
  -R '^efis_(hmi_golden_tools|avionix_hmi_contract|skyview_responsive)_tests$'

mkdir -p "$release/bin"
cp -a "$source_dir/." "$release/"
install -m 0755 "$build_dir/efis-pfd-qt" "$release/bin/efis-pfd-qt"

built_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)
export PYTHONPATH="$release"
python3 "$release/tools/deployment/build_release_manifest.py" \
  --release-root "$release" \
  --release-id "$release_id" \
  --source-commit "$source_commit" \
  --built-utc "$built_utc" \
  --role MFD \
  --platform RPI5_AARCH64 \
  --simulation-mode \
  --artifact bin/efis-pfd-qt:exec
python3 "$release/tools/deployment/validate_release_manifest.py" \
  --manifest "$release/release-manifest.json" \
  --release-root "$release" \
  --expected-role MFD \
  --expected-platform RPI5_AARCH64 \
  --check-files
python3 "$rollback/tools/deployment/validate_release_manifest.py" \
  --manifest "$rollback/release-manifest.json" \
  --release-root "$rollback" \
  --expected-role MFD \
  --expected-platform RPI5_AARCH64 \
  --check-files

sha256sum "$release/bin/efis-pfd-qt" "$release/release-manifest.json"
trap - EXIT
echo MFD_LEGIBILITY_RELEASE_PREPARATION=PASS
echo EXECUTION_AUTHORIZED=false
echo NEXT_REQUIRED=ROOT_ACTIVATION_WINDOW
