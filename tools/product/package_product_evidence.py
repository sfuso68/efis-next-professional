#!/usr/bin/env python3
"""Package operator-confirmed product evidence without approving its use."""

from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import tempfile


ROOT = Path(__file__).resolve().parents[2]
DIGEST = re.compile(r"[0-9a-f]{64}")
ROOT_KEYS = {
    "schema", "domain", "status", "operator_confirmed", "operator",
    "captured_utc", "intake_path", "intake_sha256", "review_gate_path",
    "review_gate_sha256", "subjects", "global_required_artifacts",
    "global_artifacts", "limitations", "hardware_evidence_claimed",
    "flight_approval", "decision",
}
SUBJECT_KEYS = {"id", "required_artifacts", "artifacts"}
DOMAIN_SPECS = {
    "engine": {
        "intake": "config/product/engine-source-intake-v1.json",
        "gate": "config/product/engine-source-review-gate-v1.json",
        "subjects": ["ROTAX_912_UL", "ROTAX_912_ULS"],
        "artifacts": ["data-plate-photo", "applicable-manual", "revision-page", "limit-pages", "review-signoff"],
        "global": [],
    },
    "sku": {
        "intake": "config/product/sku-approval-intake-v1.json",
        "gate": "config/product/sku-release-review-gate-v1.json",
        "subjects": ["STANDARD", "PROFESSIONAL"],
        "artifacts": ["capability-review", "node-allocation-review", "release-manifest", "operator-signoff"],
        "global": [],
    },
    "fpv": {
        "intake": "config/video/fpv-benchmark-intake-v1.json",
        "gate": "config/video/fpv-benchmark-review-gate-v1.json",
        "subjects": ["CAM-01", "CAM-02", "CAM-03", "CAM-04"],
        "artifacts": ["feed-capture", "feed-metadata"],
        "global": ["aggregate-bitrate", "decode-cpu", "decode-temperature", "end-to-end-latency", "dropped-frames", "four-feed-mosaic", "feed-loss-recovery", "telemetry-sidecar"],
    },
    "voice": {
        "intake": "config/voice/phraseology-approval-intake-v1.json",
        "gate": "config/voice/voice-cabin-review-gate-v1.json",
        "subjects": ["it-IT", "en-GB"],
        "artifacts": ["authoritative-source", "phrase-review", "fixed-critical-clips", "offline-tts-assets", "intelligibility-report"],
        "global": [],
    },
    "hil": {
        "intake": "config/hil/hil-run-intake-v1.json",
        "gate": "config/hil/hil-execution-review-gate-v1.json",
        "subjects": ["HIL-SENSOR-STALE", "HIL-SENSOR-LOST", "HIL-NETWORK-GAP", "HIL-STORAGE-FULL", "HIL-DISPLAY-LINK", "HIL-TERRAIN-MISSING"],
        "artifacts": ["input-recording", "execution-log", "expected-actual-report", "replay-report"],
        "global": [],
    },
}


def _error(errors: list[str], control: int, message: str) -> None:
    errors.append(f"[PRODUCT-PKG-{control:02d}] {message}")


def _utc(value: object) -> bool:
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        return parsed.utcoffset() is not None and parsed.utcoffset().total_seconds() == 0
    except (TypeError, ValueError):
        return False


def _repo_file(root: Path, raw: object) -> Path | None:
    if not isinstance(raw, str):
        return None
    relative = PurePosixPath(raw)
    if relative.is_absolute() or ".." in relative.parts:
        return None
    target = root.resolve().joinpath(*relative.parts)
    resolved = target.resolve()
    if root.resolve() not in resolved.parents or target.is_symlink() or not resolved.is_file():
        return None
    return resolved


def _artifact(base: Path, raw: object) -> Path | None:
    if not isinstance(raw, str) or not raw.strip():
        return None
    relative = Path(raw)
    if relative.is_absolute() or ".." in relative.parts:
        return None
    candidate = base / relative
    resolved = candidate.resolve()
    if base.resolve() not in resolved.parents or candidate.is_symlink() or not resolved.is_file() or resolved.stat().st_size == 0:
        return None
    return resolved


def load_results(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("results root must be an object")
    return value


def validate_results(data: dict, root: Path, results_dir: Path, allow_not_run: bool = False) -> list[str]:
    errors: list[str] = []
    if data.get("schema") != "avionix-next.product-review-results.v1":
        _error(errors, 1, "results schema is invalid")
    if set(data) != ROOT_KEYS:
        _error(errors, 2, "results root properties differ from the closed schema")
    domain = data.get("domain")
    spec = DOMAIN_SPECS.get(domain)
    if spec is None:
        _error(errors, 3, "domain is unknown")
        return errors
    if allow_not_run and data.get("status") == "NOT_RUN":
        if data.get("operator_confirmed") is not False or data.get("operator") is not None or data.get("captured_utc") is not None:
            _error(errors, 4, "NOT_RUN results must remain unconfirmed and unattributed")
    elif data.get("status") != "READY_FOR_PACKAGING" or data.get("operator_confirmed") is not True:
        _error(errors, 4, "results are not explicitly ready and operator confirmed")
    if data.get("status") != "NOT_RUN":
        if not isinstance(data.get("operator"), str) or not data["operator"].strip() or not _utc(data.get("captured_utc")):
            _error(errors, 5, "operator or UTC capture timestamp is invalid")
    for key, expected, digest_key in (
        ("intake_path", spec["intake"], "intake_sha256"),
        ("review_gate_path", spec["gate"], "review_gate_sha256"),
    ):
        target = _repo_file(root, data.get(key))
        if data.get(key) != expected or target is None:
            _error(errors, 6, f"{key} is missing, unsafe or unexpected")
        elif not DIGEST.fullmatch(str(data.get(digest_key, ""))) or hashlib.sha256(target.read_bytes()).hexdigest() != data[digest_key]:
            _error(errors, 7, f"{digest_key} is malformed or stale")
    subjects = data.get("subjects")
    if not isinstance(subjects, list) or [row.get("id") for row in subjects if isinstance(row, dict)] != spec["subjects"]:
        _error(errors, 8, "subjects differ from the frozen domain set")
        return errors
    for subject in subjects:
        if not isinstance(subject, dict) or set(subject) != SUBJECT_KEYS:
            _error(errors, 9, "subject properties differ from the closed schema")
            continue
        if subject.get("required_artifacts") != spec["artifacts"]:
            _error(errors, 10, f"{subject.get('id')} required artifacts changed")
            continue
        artifacts = subject.get("artifacts")
        expected = set(spec["artifacts"])
        if data.get("status") == "NOT_RUN" and allow_not_run:
            if artifacts != {}:
                _error(errors, 11, f"{subject['id']} NOT_RUN artifacts must be empty")
            continue
        if not isinstance(artifacts, dict) or set(artifacts) != expected:
            _error(errors, 11, f"{subject['id']} artifact map is incomplete")
            continue
        for kind, raw in artifacts.items():
            if _artifact(results_dir, raw) is None:
                _error(errors, 12, f"{subject['id']}/{kind} artifact is missing, empty or unsafe")
    if data.get("global_required_artifacts") != spec["global"]:
        _error(errors, 13, "global required artifacts changed")
    global_artifacts = data.get("global_artifacts")
    if data.get("status") == "NOT_RUN" and allow_not_run:
        if global_artifacts != {}:
            _error(errors, 14, "NOT_RUN global artifacts must be empty")
    elif not isinstance(global_artifacts, dict) or set(global_artifacts) != set(spec["global"]):
        _error(errors, 14, "global artifact map is incomplete")
    else:
        for kind, raw in global_artifacts.items():
            if _artifact(results_dir, raw) is None:
                _error(errors, 15, f"global/{kind} artifact is missing, empty or unsafe")
    if not isinstance(data.get("limitations"), list) or any(not isinstance(item, str) or not item.strip() for item in data.get("limitations", [])):
        _error(errors, 16, "limitations must be a list of non-empty strings")
    if data.get("hardware_evidence_claimed") is not False or data.get("flight_approval") is not False:
        _error(errors, 17, "results must not claim hardware verification or flight approval")
    expected_decision = "NO_GO_NOT_RUN" if data.get("status") == "NOT_RUN" else "NO_GO_PENDING_INDEPENDENT_REVIEW"
    if data.get("decision") != expected_decision:
        _error(errors, 18, "decision is not fail-closed")
    return errors


def _pack_artifacts(artifacts: dict, base: Path, output: Path) -> list[dict]:
    packed = []
    for kind, raw in artifacts.items():
        path = (base / raw).resolve()
        packed.append({
            "kind": kind,
            "file": os.path.relpath(path, output.parent).replace("\\", "/"),
            "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        })
    return packed


def assemble(root: Path, results_path: Path, output: Path) -> list[str]:
    data = load_results(results_path)
    base = results_path.resolve().parent
    errors = validate_results(data, root, base)
    if output.exists():
        _error(errors, 19, "output already exists; evidence bundles are immutable")
    if errors:
        return errors
    manifest = {
        "schema": "avionix-next.product-evidence-bundle.v1",
        "domain": data["domain"],
        "status": "PACKAGED_PENDING_REVIEW",
        "operator": data["operator"],
        "captured_utc": data["captured_utc"],
        "source": {
            "intake_path": data["intake_path"],
            "intake_sha256": data["intake_sha256"],
            "review_gate_path": data["review_gate_path"],
            "review_gate_sha256": data["review_gate_sha256"],
        },
        "subjects": [
            {"id": row["id"], "artifacts": _pack_artifacts(row["artifacts"], base, output)}
            for row in data["subjects"]
        ],
        "global_artifacts": _pack_artifacts(data["global_artifacts"], base, output),
        "limitations": data["limitations"],
        "review_status": "PENDING",
        "reviewers": [],
        "hardware_verified": False,
        "deployment_allowed": False,
        "flight_approval": False,
        "decision": "NO_GO_PENDING_INDEPENDENT_REVIEW",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=output.parent, delete=False) as handle:
        temporary = Path(handle.name)
        json.dump(manifest, handle, indent=2)
        handle.write("\n")
    temporary.replace(output)
    return []


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--results", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        errors = assemble(args.root, args.results, args.output)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        errors = [f"[PRODUCT-PKG-01] unable to package results: {exc}"]
    if errors:
        print("AVIONIX product evidence packaging: FAIL")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"AVIONIX product evidence packaged pending review: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
