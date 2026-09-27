#!/usr/bin/env python3
"""Validate the five product evidence review gates without approving a run."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
GATES = {
    "engine": ROOT / "config/product/engine-source-review-gate-v1.json",
    "sku": ROOT / "config/product/sku-release-review-gate-v1.json",
    "fpv": ROOT / "config/video/fpv-benchmark-review-gate-v1.json",
    "voice": ROOT / "config/voice/voice-cabin-review-gate-v1.json",
    "hil": ROOT / "config/hil/hil-execution-review-gate-v1.json",
}
DIGEST = re.compile(r"[0-9a-f]{64}")


class ReviewGateError(ValueError):
    """A review gate is stale, incomplete, or no longer fail-closed."""


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ReviewGateError(f"{path}: root must be an object")
    return value


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise ReviewGateError(f"{control}: {message}")


def linked_intake(gate: dict, control: str, root: Path) -> dict:
    raw = gate.get("intake_path")
    require(isinstance(raw, str), control, "intake path missing")
    path = PurePosixPath(raw)
    require(not path.is_absolute() and ".." not in path.parts, control, "intake path escapes repository")
    target = (root / Path(*path.parts)).resolve()
    require(root.resolve() in target.parents and target.is_file() and not target.is_symlink(), control, "intake missing or unsafe")
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    require(DIGEST.fullmatch(str(gate.get("intake_sha256", ""))) is not None, control, "intake digest malformed")
    require(gate["intake_sha256"] == digest, control, "intake digest drift")
    intake = load(target)
    require(intake.get("status") == "NOT_RUN", control, "upstream intake is not NOT_RUN")
    require(intake.get("flight_approval") is False, control, "upstream intake claims flight approval")
    return intake


def common(gate: dict, schema: str, decision: str, roles: list[str], findings: list[str], control: str) -> None:
    require(gate.get("schema") == schema, control, "wrong schema")
    require(gate.get("status") == "PREPARED_SW_NOT_RUN", control, "review gate claims execution")
    require(gate.get("required_review_roles") == roles, control, "review authority changed")
    require(gate.get("required_findings") == findings, control, "required findings changed")
    require(gate.get("evidence_bundle_sha256") == "UNRESOLVED", control, "evidence asserted before review")
    require(gate.get("reviewed_by") == [], control, "reviewers asserted before review")
    require(gate.get("hardware_verified") is False, control, "hardware verification asserted")
    require(gate.get("flight_approval") is False, control, "flight approval asserted")
    require(gate.get("decision") == decision, control, "NO-GO decision changed")


def validate(paths: dict[str, Path] = GATES, root: Path = ROOT) -> dict:
    data = {name: load(path) for name, path in paths.items()}

    engine = data["engine"]
    common(engine, "avionix-next.engine-source-review-gate.v1", "NO_GO_PENDING_ENGINE_SOURCE_REVIEW",
           ["engine-owner", "engineering-reviewer", "configuration-controller"],
           ["serial-applicability", "manual-revision", "limit-page-traceability", "ul-uls-separation", "legacy-limit-disposition"], "PR-EMS-01")
    engine_intake = linked_intake(engine, "PR-EMS-02", root)
    require(engine.get("subjects") == [row["id"] for row in engine_intake["engines"]], "PR-EMS-03", "engine subjects changed")
    require(engine.get("engineering_review_complete") is False and engine.get("activation_allowed") is False, "PR-EMS-04", "engine limits activated before review")

    sku = data["sku"]
    common(sku, "avionix-next.sku-release-review-gate.v1", "NO_GO_PENDING_SKU_RELEASE_REVIEW",
           ["product-owner", "safety-reviewer", "release-manager"],
           ["capability-allowlist", "node-allocation", "single-codebase", "unknown-feature-rejection", "inflight-profile-lock"], "PR-SKU-01")
    sku_intake = linked_intake(sku, "PR-SKU-02", root)
    require(sku.get("subjects") == [row["id"] for row in sku_intake["profiles"]], "PR-SKU-03", "SKU subjects changed")
    require(sku.get("product_review_complete") is False and sku.get("deployment_allowed") is False, "PR-SKU-04", "SKU deployment allowed before review")

    fpv = data["fpv"]
    common(fpv, "avionix-next.fpv-benchmark-review-gate.v1", "NO_GO_PENDING_FPV_BENCHMARK_REVIEW",
           ["video-owner", "network-reviewer", "thermal-reviewer"],
           ["four-feed-capacity", "latency-and-drops", "decode-thermal-margin", "feed-loss-recovery", "domain-isolation", "telemetry-sidecar-sync"], "PR-FPV-01")
    fpv_intake = linked_intake(fpv, "PR-FPV-02", root)
    require(fpv.get("subjects") == [row["id"] for row in fpv_intake["feeds"]], "PR-FPV-03", "FPV subjects changed")
    require(fpv.get("benchmark_review_complete") is False and fpv.get("video_domain_release_allowed") is False and fpv.get("network_bridge_allowed") is False, "PR-FPV-04", "FPV release/isolation boundary weakened")

    voice = data["voice"]
    common(voice, "avionix-next.voice-cabin-review-gate.v1", "NO_GO_PENDING_VOICE_CABIN_REVIEW",
           ["phraseology-reviewer", "audio-reviewer", "pilot-reviewer"],
           ["source-authority", "critical-clip-integrity", "offline-tts-provenance", "cabin-intelligibility", "ptt-isolation", "cas-priority"], "PR-VOI-01")
    voice_intake = linked_intake(voice, "PR-VOI-02", root)
    require(voice.get("subjects") == [row["id"] for row in voice_intake["locales"]], "PR-VOI-03", "voice locales changed")
    require(voice.get("cabin_review_complete") is False and voice.get("operational_enable_allowed") is False and voice.get("automatic_transmission_allowed") is False, "PR-VOI-04", "voice operation/transmission enabled before review")

    hil = data["hil"]
    common(hil, "avionix-next.hil-execution-review-gate.v1", "NO_GO_PENDING_HIL_EXECUTION_REVIEW",
           ["hil-operator", "safety-reviewer", "evidence-reviewer"],
           ["fixture-isolation", "input-digest-resolution", "deterministic-replay", "fault-containment", "expected-outcome", "artifact-completeness"], "PR-HIL-01")
    hil_intake = linked_intake(hil, "PR-HIL-02", root)
    require(hil.get("subjects") == [row["id"] for row in hil_intake["scenarios"]], "PR-HIL-03", "HIL scenarios changed")
    unsafe = ("execution_review_complete", "execution_allowed", "production_network_allowed", "production_storage_allowed", "actuator_output_allowed", "flight_control_commands_allowed")
    require(all(hil.get(key) is False for key in unsafe), "PR-HIL-04", "HIL execution/production/actuator boundary weakened")

    return {
        "schema": "avionix-next.product-review-gates-validation.v1",
        "status": "PASS",
        "review_gates": 5,
        "subjects": {"engines": 2, "profiles": 2, "fpv_feeds": 4, "voice_locales": 2, "hil_scenarios": 6},
        "execution_status": "NOT_RUN",
        "hardware_verified": False,
        "deployment_allowed": False,
        "flight_approval": False,
        "decision": "NO_GO_PENDING_EVIDENCE_AND_INDEPENDENT_REVIEW",
    }


def main() -> int:
    try:
        report = validate()
    except (ReviewGateError, KeyError, TypeError, OSError, json.JSONDecodeError) as exc:
        print(f"AVIONIX product review gates: FAIL: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
