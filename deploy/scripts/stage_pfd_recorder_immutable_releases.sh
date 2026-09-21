#!/usr/bin/env bash
# Stage PFD+recorder primary and rollback releases without changing runtime.
# Expected stage layout:
#   /home/efis/efis-release-pfd-stage/src-d52e377/
#   /home/efis/efis-release-pfd-stage/src-60b3526/
#   /home/efis/efis-release-pfd-stage/build-d52e377/{efis-pfd-qt,efis-recorder}
#   /home/efis/efis-release-pfd-stage/build-60b3526/{efis-pfd-qt,efis-recorder}
set -Eeuo pipefail

stage=/home/efis/efis-release-pfd-stage
primary=/home/efis/efis-next-releases/pfdrec-d52e377-primary-v1
rollback=/home/efis/efis-next-releases/pfdrec-60b3526-rollback-v1
primary_commit=d52e377d495dd8d33f1e7e40c3b67a663168a749
rollback_commit=60b352626257a011f94b65ad1989aaa6b5a29d3f

[[ ${EUID} -ne 0 ]] || { echo "run as the efis user, not root" >&2; exit 1; }
[[ $(id -un) == efis ]] || { echo "wrong user: expected efis" >&2; exit 1; }
[[ $(hostname) == efis-pfd-01 ]] || { echo "wrong node: expected efis-pfd-01" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }

for path in "$primary" "$rollback"; do
  [[ ! -e "$path" && ! -L "$path" ]] || {
    echo "refusing to replace existing release: $path" >&2
    exit 1
  }
done

for suffix in d52e377 60b3526; do
  [[ -d "$stage/src-$suffix" ]] || { echo "missing source stage: src-$suffix" >&2; exit 1; }
  for binary in efis-pfd-qt efis-recorder; do
    candidate="$stage/build-$suffix/$binary"
    [[ -f "$candidate" && ! -L "$candidate" && -x "$candidate" ]] || {
      echo "missing regular executable: $candidate" >&2
      exit 1
    }
  done
done

mkdir -p "$(dirname "$primary")"
cp -a "$stage/src-d52e377" "$primary"
cp -a "$stage/src-60b3526" "$rollback"
install -D -m 0755 "$stage/build-d52e377/efis-pfd-qt" "$primary/bin/efis-pfd-qt"
install -D -m 0755 "$stage/build-d52e377/efis-recorder" "$primary/bin/efis-recorder"
install -D -m 0755 "$stage/build-60b3526/efis-pfd-qt" "$rollback/bin/efis-pfd-qt"
install -D -m 0755 "$stage/build-60b3526/efis-recorder" "$rollback/bin/efis-recorder"

built_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)
export PYTHONPATH="$stage"
for specification in \
  "$primary|pfdrec-d52e377-primary-v1|$primary_commit" \
  "$rollback|pfdrec-60b3526-rollback-v1|$rollback_commit"; do
  IFS='|' read -r root release_id commit <<< "$specification"
  python3 "$stage/tools/deployment/build_release_manifest.py" \
    --release-root "$root" \
    --release-id "$release_id" \
    --source-commit "$commit" \
    --built-utc "$built_utc" \
    --role PFD \
    --role RECORDER \
    --platform RPI5_AARCH64 \
    --simulation-mode \
    --artifact bin/efis-pfd-qt:exec \
    --artifact bin/efis-recorder:exec

  for role in PFD RECORDER; do
    python3 "$stage/tools/deployment/validate_release_manifest.py" \
      --manifest "$root/release-manifest.json" \
      --release-root "$root" \
      --expected-role "$role" \
      --expected-platform RPI5_AARCH64 \
      --check-files
  done
done

sha256sum \
  "$primary/bin/efis-pfd-qt" "$primary/bin/efis-recorder" \
  "$rollback/bin/efis-pfd-qt" "$rollback/bin/efis-recorder"
echo PFD_RECORDER_RELEASE_STAGING=PASS
echo EXECUTION_AUTHORIZED=false
