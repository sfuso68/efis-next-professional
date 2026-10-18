#!/usr/bin/env bash
set -Eeuo pipefail
[[ $(id -un) == efis && $(hostname) == efis-pfd-01 && $(uname -m) == aarch64 ]]
stage=/home/efis/efis-release-pfdrec-a0be9a5-stage
release=$stage/pfdrec-a0be9a5-main-v1
rollback=/home/efis/efis-next-releases/pfdrec-7051a47-retention-v1
[[ $(readlink -f /home/efis/efis-next-current) == "$rollback" ]]
[[ $(vcgencmd get_throttled) == throttled=0x0 ]]
[[ ! -e "$release" && ! -L "$release" ]]
[[ $(df --output=avail -B1 /home/efis | tail -n 1 | tr -d ' ') -ge 12884901888 ]]
echo "3f02772bc623dc63b1bbf3e66b138ba6fa4ad5015a845dc2ccc996857d78f5c7  $stage/pfdrec-a0be9a5-main-v1.tar.gz" | sha256sum -c --strict
pfd_pid=$(systemctl --user show efis-pfd.service -p MainPID --value)
recorder_pid=$(systemctl --user show efis-recorder.service -p MainPID --value)
tar -xzf "$stage/pfdrec-a0be9a5-main-v1.tar.gz" -C "$stage"
export PYTHONPATH="$release"
for path in "$release" "$rollback"; do
  python3 "$release/tools/deployment/validate_release_manifest.py" \
    --manifest "$path/release-manifest.json" --release-root "$path" \
    --expected-role PFD --expected-role RECORDER --expected-platform RPI5_AARCH64 --check-files
done
for binary in "$release/bin/efis-pfd-qt" "$release/bin/efis-recorder"; do
  ldd "$binary"
  if ldd "$binary" | grep -q 'not found'; then exit 1; fi
done
[[ $(readlink -f /home/efis/efis-next-current) == "$rollback" ]]
[[ $(systemctl --user show efis-pfd.service -p MainPID --value) == "$pfd_pid" ]]
[[ $(systemctl --user show efis-recorder.service -p MainPID --value) == "$recorder_pid" ]]
[[ $(systemctl --user is-active efis-pfd.service) == active ]]
[[ $(systemctl --user is-active efis-recorder.service) == active ]]
systemctl --user show efis-pfd.service efis-recorder.service -p MainPID -p NRestarts -p ActiveState
sha256sum "$release/release-manifest.json" "$release"/bin/*
df -h /
vcgencmd get_throttled
echo PFD_STAGING=PASS
echo ACTIVATION=NOT_RUN_MFD_CANARY_REQUIRED
echo FLIGHT_APPROVAL=false
