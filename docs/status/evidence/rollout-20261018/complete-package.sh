#!/usr/bin/env bash
# Completion after the direct Python invocation lacked the repository import root.
set -Eeuo pipefail
[[ $(id -un) == pi && $(uname -m) == aarch64 ]]
case ${1:-} in
  PFD)
    [[ $(hostname) == pi5 ]]
    stage=/home/pi/efis-release-mfd-a0be9a5-stage
    release=$stage/pfdrec-a0be9a5-main-v1
    id=pfdrec-a0be9a5-main-v1
    platform=RPI5_AARCH64
    roles=(--role PFD --role RECORDER)
    expected=(--expected-role PFD --expected-role RECORDER)
    artifacts=(--artifact bin/efis-pfd-qt:exec --artifact bin/efis-recorder:exec)
    ;;
  EMS)
    [[ $(hostname) == efis ]]
    release=/home/pi/efis-next-releases/ems-a0be9a5-main-v1
    id=ems-a0be9a5-main-v1
    platform=RPI4_AARCH64
    roles=(--role EMS)
    expected=(--expected-role EMS)
    artifacts=(--artifact bin/efis-ems-qt:exec)
    ;;
  *) exit 2 ;;
esac
[[ -d "$release" && ! -L "$release" && ! -e "$release/release-manifest.json" ]]
[[ $(cat "$release/SOURCE_COMMIT") == a0be9a52494ea7ccb5a3890e94d03b671162383b ]]
export PYTHONPATH="$release"
python3 "$release/tools/deployment/build_release_manifest.py" \
  --release-root "$release" --release-id "$id" \
  --source-commit a0be9a52494ea7ccb5a3890e94d03b671162383b \
  --built-utc "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "${roles[@]}" \
  --platform "$platform" --simulation-mode "${artifacts[@]}"
python3 "$release/tools/deployment/validate_release_manifest.py" \
  --manifest "$release/release-manifest.json" --release-root "$release" \
  "${expected[@]}" --expected-platform "$platform" --check-files
if [[ "$1" == PFD ]]; then
  [[ ! -e "$stage/pfdrec-a0be9a5-main-v1.tar.gz" ]]
  tar -czf "$stage/pfdrec-a0be9a5-main-v1.tar.gz" -C "$stage" pfdrec-a0be9a5-main-v1
  sha256sum "$stage/pfdrec-a0be9a5-main-v1.tar.gz"
else
  rollback=/home/pi/efis-next-releases/ems-62ba90b-hmi-cas-v1
  [[ $(readlink -f /home/pi/efis-next-current) == "$rollback" ]]
  python3 "$release/tools/deployment/validate_release_manifest.py" \
    --manifest "$rollback/release-manifest.json" --release-root "$rollback" \
    --expected-role EMS --expected-platform RPI4_AARCH64 --check-files
  ldd "$release/bin/efis-ems-qt"
  if ldd "$release/bin/efis-ems-qt" | grep -q 'not found'; then exit 1; fi
  [[ $(vcgencmd get_throttled) == throttled=0x0 ]]
  systemctl show efis-ems.service -p MainPID -p NRestarts -p ActiveState
  [[ $(systemctl show efis-ems.service -p MainPID --value) == 949 ]]
  [[ $(systemctl is-active efis-ems.service) == active ]]
fi
sha256sum "$release/release-manifest.json" "$release"/bin/*
echo PACKAGE_COMPLETION=PASS
echo ACTIVATION=NOT_RUN
