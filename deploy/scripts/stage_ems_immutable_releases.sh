#!/usr/bin/env bash
# Stage independently built EMS primary and rollback releases. This script does
# not change the active symlink or systemd service.
set -euo pipefail

stage=/home/pi/efis-release-ems-stage
primary=/home/pi/efis-next-releases/ems-7655799-v1
rollback=/home/pi/efis-next-releases/ems-67aee2e-rollback-v1
primary_commit=765579959f7970a17eb212ca1ef13628aaa4d71f
rollback_commit=67aee2e52cecc06bad8b9089e31105c2fc0f2d17

for path in "$primary" "$rollback"; do
  if [[ -e "$path" || -L "$path" ]]; then
    echo "refusing to replace existing release: $path" >&2
    exit 1
  fi
done

mkdir -p "$(dirname "$primary")"
cp -a "$stage/src-7655799" "$primary"
cp -a "$stage/src-67aee2e" "$rollback"
install -D -m 0755 "$stage/build-7655799/efis-ems-qt" "$primary/bin/efis-ems-qt"
install -D -m 0755 "$stage/build-67aee2e/efis-ems-qt" "$rollback/bin/efis-ems-qt"

built_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)
export PYTHONPATH="$stage"
python3 "$stage/tools/deployment/build_release_manifest.py" \
  --release-root "$primary" \
  --release-id ems-7655799-primary-v1 \
  --source-commit "$primary_commit" \
  --built-utc "$built_utc" \
  --role EMS \
  --platform RPI4_AARCH64 \
  --simulation-mode \
  --artifact bin/efis-ems-qt:exec
python3 "$stage/tools/deployment/build_release_manifest.py" \
  --release-root "$rollback" \
  --release-id ems-67aee2e-rollback-v1 \
  --source-commit "$rollback_commit" \
  --built-utc "$built_utc" \
  --role EMS \
  --platform RPI4_AARCH64 \
  --simulation-mode \
  --artifact bin/efis-ems-qt:exec

for path in "$primary" "$rollback"; do
  python3 "$stage/tools/deployment/validate_release_manifest.py" \
    --manifest "$path/release-manifest.json" \
    --release-root "$path" \
    --expected-role EMS \
    --expected-platform RPI4_AARCH64 \
    --check-files
done

sha256sum "$primary/bin/efis-ems-qt" "$rollback/bin/efis-ems-qt"
echo "EMS_RELEASE_STAGING=PASS"
