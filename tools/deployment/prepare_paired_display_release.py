#!/usr/bin/env python3
"""Build a fail-closed, non-executable paired PFD/MFD release plan."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import sys
import tempfile
from typing import Any


SCHEMA = "efis-next.release-transition-plan.v1"
SHA40 = re.compile(r"^[0-9a-f]{40}$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
RELEASE_ID = re.compile(r"^[a-z0-9][a-z0-9._-]{6,63}$")
SERVICE = re.compile(r"^[a-z0-9@_.-]+\.service$")
EXPECTED = {
    "MFD": {
        "node": "pi5", "platform": "RPI5_AARCH64",
        "service": "efis-next-mfd-direct.service", "service_scope": "system",
        "current_link": "/home/pi/efis-next-current",
    },
    "PFD": {
        "node": "efis-pfd-01", "platform": "RPI5_AARCH64",
        "service": "efis-pfd.service", "service_scope": "user",
        "current_link": "/home/efis/efis-next-current",
    },
}
TRANSITION_KEYS = {
    "schema", "status", "decision", "flight_approval", "execution_authorized",
    "node", "role", "platform", "service_scope", "service", "current_link",
    "candidate_target", "rollback_target", "release_id", "source_commit",
    "manifest_sha256", "policy_sha256", "artifacts_checked", "required_gates",
    "mutating_commands",
}


def _canonical_bytes(payload: dict[str, Any]) -> bytes:
    return json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")


def _digest(payload: dict[str, Any]) -> str:
    return hashlib.sha256(_canonical_bytes(payload)).hexdigest()


def _load(path: pathlib.Path) -> dict[str, Any]:
    if path.is_symlink():
        raise ValueError(f"transition plan must not be a symlink: {path}")
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError("transition plan root must be an object")
    return payload


def _validate_transition(plan: dict[str, Any], role: str) -> None:
    if set(plan) != TRANSITION_KEYS:
        raise ValueError(f"{role} transition fields do not match V1 contract")
    if plan["schema"] != SCHEMA or plan["status"] != "PREPARED_NOT_EXECUTED":
        raise ValueError(f"{role} transition is not a prepared V1 plan")
    if plan["decision"] != "NO_GO_UNTIL_SEPARATE_HARDWARE_AUTHORIZATION":
        raise ValueError(f"{role} decision is not fail-closed")
    if plan["execution_authorized"] is not False or plan["flight_approval"] is not False:
        raise ValueError(f"{role} transition attempts to authorize execution or flight")
    if plan["mutating_commands"] != []:
        raise ValueError(f"{role} transition contains mutating commands")
    if plan["role"] != role:
        raise ValueError(f"expected {role} transition, got {plan['role']!r}")
    for field, expected in EXPECTED[role].items():
        if plan.get(field) != expected:
            raise ValueError(f"{role} {field} differs from node release policy")
    if not SHA40.fullmatch(plan.get("source_commit", "")):
        raise ValueError(f"{role} source commit is not a full Git SHA")
    if not RELEASE_ID.fullmatch(plan.get("release_id", "")):
        raise ValueError(f"{role} release identifier is invalid")
    if not SERVICE.fullmatch(plan.get("service", "")):
        raise ValueError(f"{role} service identifier is invalid")
    for field in ("manifest_sha256", "policy_sha256"):
        if not SHA256.fullmatch(plan.get(field, "")):
            raise ValueError(f"{role} {field} is invalid")
    if not isinstance(plan["artifacts_checked"], int) or isinstance(plan["artifacts_checked"], bool) or plan["artifacts_checked"] < 1:
        raise ValueError(f"{role} has no validated artifacts")
    paths = {}
    for field in ("current_link", "candidate_target", "rollback_target"):
        value = plan.get(field)
        path = pathlib.PurePosixPath(value) if isinstance(value, str) else None
        if path is None or not path.is_absolute() or ".." in path.parts:
            raise ValueError(f"{role} {field} is not a safe absolute path")
        paths[field] = path
    if paths["candidate_target"] == paths["rollback_target"]:
        raise ValueError(f"{role} candidate and rollback targets are identical")
    gates = plan["required_gates"]
    if not isinstance(gates, list) or len(gates) < 9 or len(gates) != len(set(gates)):
        raise ValueError(f"{role} required gates are invalid")


def prepare(pfd_plan: dict[str, Any], mfd_plan: dict[str, Any]) -> dict[str, Any]:
    _validate_transition(pfd_plan, "PFD")
    _validate_transition(mfd_plan, "MFD")
    if pfd_plan["source_commit"] != mfd_plan["source_commit"]:
        raise ValueError("PFD and MFD source commits differ")
    if pfd_plan["policy_sha256"] != mfd_plan["policy_sha256"]:
        raise ValueError("PFD and MFD node release policies differ")
    if pfd_plan["release_id"] == mfd_plan["release_id"]:
        raise ValueError("PFD and MFD release identifiers must remain role-specific")

    def node(plan: dict[str, Any]) -> dict[str, Any]:
        return {
            key: plan[key] for key in (
                "role", "node", "platform", "service", "service_scope",
                "current_link", "candidate_target", "rollback_target",
                "release_id", "manifest_sha256", "artifacts_checked",
            )
        } | {"transition_plan_sha256": _digest(plan)}

    return {
        "schema": "efis-next.paired-display-release-plan.v1",
        "status": "PREPARED_NOT_EXECUTED",
        "decision": "NO_GO_UNTIL_SEPARATE_HARDWARE_AUTHORIZATION",
        "flight_approval": False,
        "execution_authorized": False,
        "source_commit": pfd_plan["source_commit"],
        "policy_sha256": pfd_plan["policy_sha256"],
        "activation_order": ["MFD", "PFD"],
        "rollback_order": ["PFD", "MFD"],
        "nodes": [node(mfd_plan), node(pfd_plan)],
        "required_gates": [
            "both_transition_plans_validate_independently",
            "same_full_source_commit",
            "same_node_release_policy_digest",
            "both_candidate_manifests_pass",
            "both_rollback_manifests_pass",
            "mfd_activated_and_accepted_before_pfd",
            "pfd_primary_display_continuity_preserved",
            "ddp_live_and_provenance_visible_on_both_nodes",
            "c12_tiles_and_dataset_manifests_match",
            "paired_reboot_acceptance_pass",
            "paired_rollback_drill_pass",
            "operator_and_bench_window_separately_authorized",
        ],
        "required_evidence": [
            "preflight-node-identity.json",
            "release-manifest-digests.json",
            "service-state-before-after.json",
            "ddp-health-before-after.json",
            "thermal-throttling.csv",
            "paired-reboot-acceptance.json",
            "paired-rollback-drill.json",
            "operator-authorization.json",
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
        if os.name != "nt":
            os.chmod(path, 0o644)
    finally:
        temporary_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--pfd-plan", required=True, type=pathlib.Path)
    parser.add_argument("--mfd-plan", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    try:
        payload = prepare(_load(args.pfd_plan), _load(args.mfd_plan))
        atomic_write(args.output, payload)
    except (OSError, ValueError, TypeError, KeyError, json.JSONDecodeError) as exc:
        print(json.dumps({
            "schema": "efis-next.paired-display-release-plan.v1",
            "status": "FAIL", "error": str(exc), "flight_approval": False,
            "execution_authorized": False,
        }, sort_keys=True))
        return 1
    print(json.dumps(payload, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
