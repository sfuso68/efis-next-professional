#!/usr/bin/env python3
"""Prepare a non-executable EFIS NEXT release transition and rollback record."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import sys
import tempfile
from typing import Any

from tools.deployment.validate_release_manifest import validate


def _sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_policy(path: pathlib.Path, node: str, role: str) -> dict[str, Any]:
    if path.is_symlink():
        raise ValueError("policy must not be a symlink")
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError("policy root must be an object")
    if set(payload) != {"schema", "repository", "flight_approval", "nodes"}:
        raise ValueError("policy fields do not match V1 contract")
    if payload["schema"] != "efis-next.node-release-policy.v1":
        raise ValueError("unsupported node release policy")
    if payload["repository"] != "sfuso68/efis-next-professional" or payload["flight_approval"] is not False:
        raise ValueError("policy repository or flight approval is invalid")
    nodes = payload.get("nodes")
    if not isinstance(nodes, list):
        raise ValueError("policy nodes must be an array")
    required = {"node", "role", "platform", "service_scope", "service", "current", "executable"}
    for item in nodes:
        if not isinstance(item, dict) or set(item) != required:
            raise ValueError("node policy fields do not match V1 contract")
        if item["service_scope"] not in {"system", "user"}:
            raise ValueError("invalid service scope")
        if not pathlib.PurePosixPath(item["current"]).is_absolute():
            raise ValueError("current link must be absolute")
        executable = pathlib.PurePosixPath(item["executable"])
        if executable.is_absolute() or ".." in executable.parts:
            raise ValueError("policy executable must be a safe relative path")
    matches = [item for item in nodes if item.get("node") == node and item.get("role") == role]
    if len(matches) != 1:
        raise ValueError("node and role must select exactly one policy profile")
    return matches[0]


def prepare(
    policy_path: pathlib.Path,
    manifest_path: pathlib.Path,
    candidate_root: pathlib.Path,
    previous_target: pathlib.Path,
    node: str,
    role: str,
) -> dict[str, Any]:
    profile = load_policy(policy_path, node, role)
    if not candidate_root.is_absolute() or not previous_target.is_absolute():
        raise ValueError("candidate and previous targets must be absolute")
    if candidate_root.is_symlink() or previous_target.is_symlink():
        raise ValueError("candidate and previous targets must not be symlinks")
    candidate_root = candidate_root.resolve(strict=True)
    previous_target = previous_target.resolve(strict=True)
    if not candidate_root.is_dir() or not previous_target.is_dir():
        raise ValueError("candidate and previous targets must be directories")
    if candidate_root == previous_target:
        raise ValueError("candidate and rollback target must differ")
    if manifest_path.is_symlink():
        raise ValueError("manifest must not be a symlink")
    manifest_path = manifest_path.resolve(strict=True)
    if manifest_path.parent != candidate_root or manifest_path.name != "release-manifest.json":
        raise ValueError("manifest must be candidate_root/release-manifest.json")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(manifest, dict):
        raise ValueError("manifest root must be an object")
    result = validate(
        manifest, candidate_root, profile["role"], profile["platform"], True,
    )
    expected = pathlib.Path(profile["current"])
    return {
        "schema": "efis-next.release-transition-plan.v1",
        "status": "PREPARED_NOT_EXECUTED",
        "decision": "NO_GO_UNTIL_SEPARATE_HARDWARE_AUTHORIZATION",
        "flight_approval": False,
        "execution_authorized": False,
        "node": node,
        "role": role,
        "platform": profile["platform"],
        "service_scope": profile["service_scope"],
        "service": profile["service"],
        "current_link": str(expected),
        "candidate_target": str(candidate_root),
        "rollback_target": str(previous_target),
        "release_id": result["release_id"],
        "source_commit": manifest["source_commit"],
        "manifest_sha256": _sha256(manifest_path),
        "policy_sha256": _sha256(policy_path),
        "artifacts_checked": result["artifacts_checked"],
        "required_gates": [
            "current_link_and_owner_reconfirmed_read_only",
            "single_supervisor_confirmed",
            "candidate_manifest_pass",
            "candidate_service_syntax_pass",
            "rollback_target_manifest_pass",
            "operator_and_bench_window_authorized",
            "post_switch_runtime_acceptance_pass",
            "reboot_acceptance_pass",
            "rollback_drill_pass",
        ],
        "mutating_commands": [],
    }


def atomic_write(path: pathlib.Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() or path.is_symlink():
        raise ValueError(f"refusing to replace existing plan: {path}")
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    temporary_path = pathlib.Path(temporary)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(payload, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, path)
    finally:
        temporary_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--policy", required=True, type=pathlib.Path)
    parser.add_argument("--manifest", required=True, type=pathlib.Path)
    parser.add_argument("--candidate-root", required=True, type=pathlib.Path)
    parser.add_argument("--previous-target", required=True, type=pathlib.Path)
    parser.add_argument("--node", required=True)
    parser.add_argument("--role", required=True)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    try:
        payload = prepare(
            args.policy, args.manifest, args.candidate_root, args.previous_target,
            args.node, args.role,
        )
        atomic_write(args.output, payload)
    except (OSError, ValueError, TypeError, json.JSONDecodeError, KeyError) as exc:
        print(json.dumps({
            "schema": "efis-next.release-transition-plan.v1", "status": "FAIL",
            "error": str(exc), "flight_approval": False,
            "execution_authorized": False,
        }, sort_keys=True))
        return 1
    print(json.dumps(payload, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
