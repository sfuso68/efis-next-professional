#!/usr/bin/env python3
"""Build an EFIS NEXT release manifest without activating the release."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import pathlib
import re
import sys
import tempfile

from tools.deployment.validate_release_manifest import PLATFORMS, ROLES, validate


ARTIFACT_RE = re.compile(r"^(?P<path>[^:]+):(?P<mode>exec|data)$")


def _sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _utc(value: str) -> str:
    parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None or parsed.utcoffset() != dt.timedelta(0):
        raise ValueError("--built-utc must be an ISO-8601 UTC timestamp")
    return parsed.astimezone(dt.timezone.utc).isoformat().replace("+00:00", "Z")


def build_manifest(
    root: pathlib.Path,
    release_id: str,
    source_commit: str,
    built_utc: str,
    roles: list[str],
    platform: str,
    simulation_mode: bool,
    artifact_specs: list[str],
) -> dict[str, object]:
    root = root.resolve(strict=True)
    if not root.is_dir():
        raise ValueError("release root must be a directory")
    if len(roles) != len(set(roles)):
        raise ValueError("roles must be unique")
    artifacts: list[dict[str, object]] = []
    seen: set[str] = set()
    for spec in artifact_specs:
        match = ARTIFACT_RE.fullmatch(spec)
        if match is None:
            raise ValueError(f"invalid artifact specification: {spec}")
        relative = match.group("path")
        pure = pathlib.PurePosixPath(relative)
        if pure.is_absolute() or ".." in pure.parts or relative in seen:
            raise ValueError(f"unsafe or duplicate artifact path: {relative}")
        seen.add(relative)
        path = root.joinpath(*pure.parts)
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"artifact must be a regular non-symlink file: {relative}")
        executable = match.group("mode") == "exec"
        if executable and os.name != "nt" and not os.access(path, os.X_OK):
            raise ValueError(f"artifact declared executable but is not executable: {relative}")
        artifacts.append({
            "path": relative,
            "sha256": _sha256(path),
            "executable": executable,
        })
    manifest: dict[str, object] = {
        "schema": "efis-next.release-manifest.v1",
        "release_id": release_id,
        "source_commit": source_commit,
        "built_utc": _utc(built_utc),
        "roles": sorted(roles),
        "platform": platform,
        "repository": "sfuso68/efis-next-professional",
        "flight_approval": False,
        "simulation_mode": simulation_mode,
        "artifacts": artifacts,
    }
    validate(manifest, root, check_files=True)
    return manifest


def atomic_write(path: pathlib.Path, payload: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() or path.is_symlink():
        raise ValueError(f"refusing to replace existing manifest: {path}")
    data = (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode("utf-8")
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    temporary_path = pathlib.Path(temporary)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, path)
    finally:
        temporary_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--release-root", required=True, type=pathlib.Path)
    parser.add_argument("--release-id", required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--built-utc", required=True)
    parser.add_argument("--role", action="append", required=True, choices=sorted(ROLES))
    parser.add_argument("--platform", required=True, choices=sorted(PLATFORMS))
    parser.add_argument("--simulation-mode", action=argparse.BooleanOptionalAction, required=True)
    parser.add_argument("--artifact", action="append", required=True, metavar="PATH:exec|data")
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        manifest = build_manifest(
            args.release_root, args.release_id, args.source_commit, args.built_utc,
            args.role, args.platform, args.simulation_mode, args.artifact,
        )
        output = args.output or args.release_root / "release-manifest.json"
        atomic_write(output, manifest)
    except (OSError, ValueError) as exc:
        print(json.dumps({
            "schema": "efis-next.release-build.v1", "status": "FAIL",
            "error": str(exc), "flight_approval": False,
        }, sort_keys=True))
        return 1
    print(json.dumps({
        "schema": "efis-next.release-build.v1", "status": "PASS",
        "manifest": str(output), "release_id": manifest["release_id"],
        "artifacts": len(manifest["artifacts"]), "flight_approval": False,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
