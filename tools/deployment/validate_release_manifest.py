#!/usr/bin/env python3
"""Fail-closed validator for an EFIS NEXT immutable release directory."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import pathlib
import re
import sys
from typing import Any

SCHEMA = "efis-next.release-manifest.v1"
REPOSITORY = "sfuso68/efis-next-professional"
ROLES = {"PFD", "MFD", "EMS", "RECORDER", "BLACKBOX"}
PLATFORMS = {"RPI4_AARCH64", "RPI5_AARCH64"}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
RELEASE_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{6,63}$")


class ManifestError(ValueError):
    pass


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ManifestError(message)


def _parse_utc(value: Any) -> None:
    _require(isinstance(value, str), "built_utc must be a string")
    try:
        parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise ManifestError("built_utc must be ISO-8601") from exc
    _require(parsed.tzinfo is not None, "built_utc must include a timezone")
    _require(parsed.utcoffset() == dt.timedelta(0), "built_utc must be UTC")


def _safe_artifact(root: pathlib.Path, relative: Any) -> pathlib.Path:
    _require(isinstance(relative, str) and relative, "artifact path must be non-empty")
    candidate = pathlib.PurePosixPath(relative)
    _require(not candidate.is_absolute(), f"artifact path must be relative: {relative}")
    _require(".." not in candidate.parts, f"artifact path escapes release: {relative}")
    root_resolved = root.resolve()
    lexical = root_resolved / pathlib.Path(*candidate.parts)
    current = root_resolved
    for part in candidate.parts:
        current = current / part
        _require(not current.is_symlink(), f"artifact path contains symlink: {relative}")
    resolved = lexical.resolve()
    try:
        resolved.relative_to(root_resolved)
    except ValueError as exc:
        raise ManifestError(f"artifact resolves outside release: {relative}") from exc
    return resolved


def _digest(path: pathlib.Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def validate(
    manifest: dict[str, Any],
    root: pathlib.Path,
    expected_role: str | None = None,
    expected_platform: str | None = None,
    check_files: bool = False,
) -> dict[str, Any]:
    allowed = {
        "schema", "release_id", "source_commit", "built_utc", "roles",
        "platform", "repository", "flight_approval", "simulation_mode",
        "artifacts",
    }
    _require(set(manifest) == allowed, "manifest fields do not match V1 contract")
    _require(manifest["schema"] == SCHEMA, "unsupported manifest schema")
    _require(isinstance(manifest["release_id"], str) and RELEASE_RE.fullmatch(manifest["release_id"]) is not None, "invalid release_id")
    _require(isinstance(manifest["source_commit"], str) and COMMIT_RE.fullmatch(manifest["source_commit"]) is not None, "invalid source_commit")
    _parse_utc(manifest["built_utc"])
    roles = manifest["roles"]
    _require(isinstance(roles, list) and roles, "roles must be non-empty")
    _require(all(role in ROLES for role in roles), "invalid role")
    _require(len(roles) == len(set(roles)), "roles must be unique")
    _require(manifest["platform"] in PLATFORMS, "invalid platform")
    _require(manifest["repository"] == REPOSITORY, "unexpected repository")
    _require(manifest["flight_approval"] is False, "flight_approval must remain false")
    _require(type(manifest["simulation_mode"]) is bool, "simulation_mode must be boolean")
    if expected_role is not None:
        _require(expected_role in roles, "role does not match node policy")
    if expected_platform is not None:
        _require(manifest["platform"] == expected_platform, "platform does not match node policy")

    artifacts = manifest["artifacts"]
    _require(isinstance(artifacts, list) and artifacts, "artifacts must be non-empty")
    seen: set[str] = set()
    checked = 0
    for artifact in artifacts:
        _require(isinstance(artifact, dict), "artifact must be an object")
        _require(set(artifact) == {"path", "sha256", "executable"}, "artifact fields do not match V1 contract")
        relative = artifact["path"]
        _require(relative not in seen, f"duplicate artifact path: {relative}")
        seen.add(relative)
        _require(isinstance(artifact["sha256"], str) and SHA256_RE.fullmatch(artifact["sha256"]) is not None, f"invalid sha256: {relative}")
        _require(type(artifact["executable"]) is bool, f"invalid executable flag: {relative}")
        path = _safe_artifact(root, relative)
        if check_files:
            _require(path.is_file(), f"artifact missing: {relative}")
            _require(_digest(path) == artifact["sha256"], f"sha256 mismatch: {relative}")
            if artifact["executable"] and os.name != "nt":
                _require(os.access(path, os.X_OK), f"artifact is not executable: {relative}")
            checked += 1

    return {
        "schema": "efis-next.release-validation.v1",
        "status": "PASS",
        "release_id": manifest["release_id"],
        "roles": roles,
        "platform": manifest["platform"],
        "artifacts_declared": len(artifacts),
        "artifacts_checked": checked,
        "flight_approval": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True, type=pathlib.Path)
    parser.add_argument("--release-root", type=pathlib.Path)
    parser.add_argument("--expected-role", choices=sorted(ROLES))
    parser.add_argument("--expected-platform", choices=sorted(PLATFORMS))
    parser.add_argument("--check-files", action="store_true")
    args = parser.parse_args()
    try:
        _require(not args.manifest.is_symlink(), "manifest must not be a symlink")
        manifest_path = args.manifest.resolve(strict=True)
        root = (args.release_root or manifest_path.parent).resolve(strict=True)
        with manifest_path.open("r", encoding="utf-8") as stream:
            payload = json.load(stream)
        _require(isinstance(payload, dict), "manifest root must be an object")
        result = validate(payload, root, args.expected_role, args.expected_platform, args.check_files)
    except (OSError, json.JSONDecodeError, ManifestError) as exc:
        print(json.dumps({"schema": "efis-next.release-validation.v1", "status": "FAIL", "error": str(exc), "flight_approval": False}, sort_keys=True))
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
