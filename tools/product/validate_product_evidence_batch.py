#!/usr/bin/env python3
"""Validate five product evidence intake templates without executing hardware."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
INTAKES = {
    "engine": ROOT / "config/product/engine-source-intake-v1.json",
    "sku": ROOT / "config/product/sku-approval-intake-v1.json",
    "fpv": ROOT / "config/video/fpv-benchmark-intake-v1.json",
    "voice": ROOT / "config/voice/phraseology-approval-intake-v1.json",
    "hil": ROOT / "config/hil/hil-run-intake-v1.json",
}
DIGEST = re.compile(r"[0-9a-f]{64}")


class EvidenceIntakeError(ValueError):
    """An evidence intake is unsafe, stale, or claims an unperformed run."""


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise EvidenceIntakeError(f"{path}: root must be an object")
    return value


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise EvidenceIntakeError(f"{control}: {message}")


def _registry(intake: dict, control: str, root: Path) -> dict:
    raw = intake.get("registry_path")
    require(isinstance(raw, str), control, "registry path missing")
    path = PurePosixPath(raw)
    require(not path.is_absolute() and ".." not in path.parts, control, "registry path escapes repository")
    target = (root / Path(*path.parts)).resolve()
    require(root.resolve() in target.parents and target.is_file() and not target.is_symlink(), control, "registry missing or unsafe")
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    require(DIGEST.fullmatch(str(intake.get("registry_sha256", ""))) is not None, control, "registry digest malformed")
    require(digest == intake["registry_sha256"], control, "registry digest drift")
    return load(target)


def _root(intake: dict, schema: str, decision: str, control: str) -> None:
    require(intake.get("schema") == schema, control, "wrong schema")
    require(intake.get("status") == "NOT_RUN", control, "template claims execution")
    require(intake.get("flight_approval") is False, control, "flight approval must remain false")
    require(intake.get("decision") == decision, control, "NO-GO decision changed")


def _empty_artifacts(rows: object, ids: list[str], control: str) -> list[dict]:
    require(isinstance(rows, list) and [row.get("id") for row in rows if isinstance(row, dict)] == ids, control, "subjects differ from frozen registry")
    for row in rows:
        require(row.get("artifacts") == [], control, f"{row.get('id')}: evidence asserted in NOT_RUN template")
    return rows


def validate(paths: dict[str, Path] = INTAKES, root: Path = ROOT) -> dict:
    data = {name: load(path) for name, path in paths.items()}

    engine = data["engine"]
    _root(engine, "avionix-next.engine-source-intake.v1", "NO_GO_PENDING_APPLICABLE_ENGINE_SOURCES", "PE-EMS-01")
    engine_registry = _registry(engine, "PE-EMS-02", root)
    engines = _empty_artifacts(engine.get("engines"), [row["id"] for row in engine_registry["engines"]], "PE-EMS-03")
    required_engine = ["data-plate-photo", "applicable-manual", "revision-page", "limit-pages", "review-signoff"]
    require(all(row.get("required_artifacts") == required_engine for row in engines), "PE-EMS-04", "engine source evidence set changed")
    require(engine.get("operator") is None and engine.get("captured_utc") is None and engine.get("engineering_reviewed") is False and engine.get("activation_allowed") is False, "PE-EMS-05", "unreviewed engine intake became active")

    sku = data["sku"]
    _root(sku, "avionix-next.sku-approval-intake.v1", "NO_GO_PENDING_PROFILE_AND_RELEASE_REVIEW", "PE-SKU-01")
    sku_registry = _registry(sku, "PE-SKU-02", root)
    profiles = _empty_artifacts(sku.get("profiles"), [row["id"] for row in sku_registry["profiles"]], "PE-SKU-03")
    require(all(row.get("required_artifacts") == ["capability-review", "node-allocation-review", "release-manifest", "operator-signoff"] for row in profiles), "PE-SKU-04", "SKU evidence set changed")
    require(all(sku.get(key) is False for key in ("single_codebase_confirmed", "inflight_switch_tested_disabled", "unknown_capability_rejection_verified", "approved")), "PE-SKU-05", "SKU approval asserted before review")

    fpv = data["fpv"]
    _root(fpv, "avionix-next.fpv-benchmark-intake.v1", "NO_GO_PENDING_ISOLATED_VIDEO_BENCH", "PE-FPV-01")
    fpv_registry = _registry(fpv, "PE-FPV-02", root)
    feeds = _empty_artifacts(fpv.get("feeds"), [row["id"] for row in fpv_registry["feeds"]], "PE-FPV-03")
    require(len(feeds) == 4 and fpv.get("isolated_fixture") is None and fpv.get("measurements") == [], "PE-FPV-04", "FPV measurement claimed without isolated fixture")
    require(len(fpv.get("required_measurements", [])) == 8, "PE-FPV-05", "FPV measurement set incomplete")
    require(fpv.get("network_bridge_enabled") is False and fpv.get("operational_wifi_enabled") is False and fpv.get("execution_authorized") is False, "PE-FPV-06", "FPV isolation/execution boundary weakened")

    voice = data["voice"]
    _root(voice, "avionix-next.phraseology-approval-intake.v1", "NO_GO_PENDING_SOURCE_AUDIO_AND_CABIN_REVIEW", "PE-VOI-01")
    voice_registry = _registry(voice, "PE-VOI-02", root)
    locales = _empty_artifacts(voice.get("locales"), voice_registry["supported_locales"], "PE-VOI-03")
    require(all(len(row.get("required_artifacts", [])) == 5 for row in locales), "PE-VOI-04", "voice evidence set incomplete")
    require(voice.get("offline_only") is True and voice.get("automatic_transmission_allowed") is False, "PE-VOI-05", "offline/manual transmission boundary weakened")
    require(voice.get("ptt_interface_verified") is False and voice.get("pilot_review_complete") is False and voice.get("approved") is False, "PE-VOI-06", "voice approval asserted before bench review")

    hil = data["hil"]
    _root(hil, "avionix-next.hil-run-intake.v1", "NO_GO_PENDING_FIXTURE_INPUTS_AND_EXECUTION_APPROVAL", "PE-HIL-01")
    hil_registry = _registry(hil, "PE-HIL-02", root)
    scenarios = _empty_artifacts(hil.get("scenarios"), [row["id"] for row in hil_registry["scenarios"]], "PE-HIL-03")
    require(hil.get("isolated_fixture") is None and all(row.get("input_sha256") == "UNRESOLVED" for row in scenarios), "PE-HIL-04", "HIL inputs/fixture asserted before approval")
    unsafe = ("production_network_used", "production_storage_used", "actuator_output_enabled", "flight_control_commands_enabled", "execution_authorized")
    require(all(hil.get(key) is False for key in unsafe), "PE-HIL-05", "HIL production or actuator boundary weakened")

    return {
        "schema": "avionix-next.product-evidence-validation.v1",
        "status": "PASS",
        "intakes": 5,
        "execution_status": "NOT_RUN",
        "execution_authorized": False,
        "hardware_verified": False,
        "flight_approval": False,
        "subjects": {"engines": len(engines), "profiles": len(profiles), "fpv_feeds": len(feeds), "voice_locales": len(locales), "hil_scenarios": len(scenarios)},
        "decision": "NO_GO_PENDING_INTERACTIVE_EVIDENCE",
    }


def main() -> int:
    try:
        report = validate()
    except (EvidenceIntakeError, KeyError, TypeError, OSError, json.JSONDecodeError) as exc:
        print(f"AVIONIX product evidence: FAIL: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
