#!/usr/bin/env bash
# Build and stage the merged asynchronous weather writer on the PFD.
# This script never changes the active release. Run as user `efis`.
set -Eeuo pipefail

source_commit=3c7bb4d5495e1ce9f582dd185be48b9563993e48
source_sha256=beb4cce4eed80e950dda6e245ff3bae42842f96242f4836cddeb5fe7310c61f3
release_id=pfdrec-3c7bb4d-weather-async-v1
stage=/home/efis/efis-release-pfd-weather-stage
archive="$stage/efis-next-${source_commit}.tar.gz"
source_dir="$stage/src-${source_commit}"
build_dir="$stage/build-${source_commit}"
release="$stage/release-${release_id}"
rollback=/home/efis/efis-next-releases/pfdrec-7051a47-retention-v1
current=/home/efis/efis-next-current

[[ ${EUID} -ne 0 ]] || { echo "run as user efis, not root" >&2; exit 1; }
[[ $(id -un) == efis ]] || { echo "wrong user: expected efis" >&2; exit 1; }
[[ $(hostname) == efis-pfd-01 ]] || { echo "wrong node: expected efis-pfd-01" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ $(readlink -f "$current") == "$rollback" ]] || {
  echo "unexpected active release; expected $rollback" >&2
  exit 1
}
[[ -f "$archive" && ! -L "$archive" ]] || { echo "missing source archive: $archive" >&2; exit 1; }
echo "$source_sha256  $archive" | sha256sum --check --strict
[[ ! -e "$source_dir" && ! -e "$build_dir" && ! -e "$release" ]] || {
  echo "refusing to replace an existing staging path" >&2
  exit 1
}

free_bytes=$(df --output=avail -B1 /home/efis | tail -n 1 | tr -d ' ')
[[ "$free_bytes" =~ ^[0-9]+$ && "$free_bytes" -ge 12884901888 ]] || {
  echo "less than the mandatory 12 GiB filesystem reserve" >&2
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
cmake --build "$build_dir" --target efis-pfd-qt efis-recorder efis_weather_service_tests
ctest --test-dir "$build_dir" --output-on-failure -R '^efis_weather_service_tests$'

mkdir -p "$release/bin"
cp -a "$source_dir/." "$release/"
install -m 0755 "$build_dir/efis-pfd-qt" "$release/bin/efis-pfd-qt"
install -m 0755 "$build_dir/efis-recorder" "$release/bin/efis-recorder"

built_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)
export PYTHONPATH="$release"
python3 "$release/tools/deployment/build_release_manifest.py" \
  --release-root "$release" \
  --release-id "$release_id" \
  --source-commit "$source_commit" \
  --built-utc "$built_utc" \
  --role PFD --role RECORDER \
  --platform RPI5_AARCH64 \
  --simulation-mode \
  --artifact bin/efis-pfd-qt:exec \
  --artifact bin/efis-recorder:exec
for role in PFD RECORDER; do
  python3 "$release/tools/deployment/validate_release_manifest.py" \
    --manifest "$release/release-manifest.json" \
    --release-root "$release" \
    --expected-role "$role" \
    --expected-platform RPI5_AARCH64 \
    --check-files
done

sha256sum "$release/bin/efis-pfd-qt" "$release/bin/efis-recorder"
trap - EXIT
echo PFD_WEATHER_ASYNC_RELEASE_PREPARATION=PASS
echo EXECUTION_AUTHORIZED=false
echo NEXT_REQUIRED=ROOT_ACTIVATION_WINDOW
