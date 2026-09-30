#!/usr/bin/env bash
# Build and stage the Blackbox broker with the fail-closed bbxnav policy as a
# manifest-covered release artifact. This script never changes the active
# release or any systemd unit. Run as user `pi` on efisblackbox.
set -Eeuo pipefail

source_commit=46e56a955b6c3a8bdf647789930c3e7855051fa5
source_sha256=cb969e12e3f055f5be2aeff1c71f13abbcfaca967bb11f2d8223809ada065b1c
release_id=blackbox-46e56a9-nav-supervision-v1
stage=/home/pi/efis-release-blackbox-nav-stage
archive="$stage/efis-next-${source_commit}.tar"
source_dir="$stage/src-${source_commit}"
build_dir="$stage/build-${source_commit}"
release="$stage/release-${release_id}"
rollback=/opt/efis-blackbox-releases/blackbox-7655799-v1
current=/opt/efis-blackbox-current

[[ ${EUID} -ne 0 ]] || { echo "run as user pi, not root" >&2; exit 1; }
[[ $(id -un) == pi ]] || { echo "wrong user: expected pi" >&2; exit 1; }
[[ $(hostname) == efisblackbox ]] || { echo "wrong node: expected efisblackbox" >&2; exit 1; }
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

free_bytes=$(df --output=avail -B1 /opt | tail -n 1 | tr -d ' ')
[[ "$free_bytes" =~ ^[0-9]+$ && "$free_bytes" -ge 4294967296 ]] || {
  echo "less than the mandatory 4 GiB filesystem reserve" >&2
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
tar -xf "$archive" -C "$source_dir"
printf '%s\n' "$source_commit" > "$source_dir/SOURCE_COMMIT"

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DEFIS_BUILD_QT_RENDERER=OFF \
  -DEFIS_BUILD_TESTS=ON
cmake --build "$build_dir" --target efis-blackbox-broker efis_core_tests
ctest --test-dir "$build_dir" --output-on-failure \
  -R '^(efis_core_tests|efis_bbxnav_supervision_tests|efis_release_supervision_asset_tests)$'

mkdir -p "$release/bin" "$release/share/systemd"
cp -a "$source_dir/." "$release/"
install -m 0755 "$build_dir/efis-blackbox-broker" "$release/bin/efis-blackbox-broker"
install -m 0644 "$source_dir/deploy/systemd/bbxnav-fail-closed.override.conf" \
  "$release/share/systemd/bbxnav-fail-closed.override.conf"

built_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)
export PYTHONPATH="$release"
python3 "$release/tools/deployment/build_release_manifest.py" \
  --release-root "$release" \
  --release-id "$release_id" \
  --source-commit "$source_commit" \
  --built-utc "$built_utc" \
  --role BLACKBOX \
  --platform RPI5_AARCH64 \
  --simulation-mode \
  --artifact bin/efis-blackbox-broker:exec \
  --artifact share/systemd/bbxnav-fail-closed.override.conf:data
python3 "$release/tools/deployment/validate_release_manifest.py" \
  --manifest "$release/release-manifest.json" \
  --release-root "$release" \
  --expected-role BLACKBOX \
  --expected-platform RPI5_AARCH64 \
  --check-files
python3 "$release/tools/navigation/validate_bbxnav_supervision.py" \
  --override "$release/share/systemd/bbxnav-fail-closed.override.conf"

sha256sum "$release/bin/efis-blackbox-broker" \
  "$release/share/systemd/bbxnav-fail-closed.override.conf"
trap - EXIT
echo BLACKBOX_NAV_IMMUTABLE_RELEASE_PREPARATION=PASS
echo EXECUTION_AUTHORIZED=false
echo NEXT_REQUIRED=ROOT_ACTIVATION_WINDOW
