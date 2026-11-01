"""Prepare a hash-bound EMS review; never change or authorize a golden baseline."""
from __future__ import annotations

import argparse
from datetime import datetime, timedelta
import hashlib
import json
from pathlib import Path
import re
import stat
import tempfile
import zipfile

from tools.ui.capture_ems_responsive_state_matrix import SIZES, STATES, filename
from tools.ui.capture_hmi_golden import png_pixels, png_size, sha256
from tools.ui.compare_hmi_golden import difference

EXPECTED = [(w, h, state, age) for w, h in SIZES for state, age in STATES]
NAMES = [filename(w, h, state) for w, h, state, _ in EXPECTED]
ROOT_KEYS = {"schema", "status", "generated_utc", "platform", "role", "fixture", "images"}
IMAGE_KEYS = {"state", "age_ms", "width", "height", "file", "bytes", "sha256", "pixel_sha256"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def strict_json(text):
    def object_pairs(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "duplicate JSON field")
            result[key] = value
        return result
    return json.loads(text, object_pairs_hook=object_pairs)


def manifest(directory):
    path = directory / "manifest.json"
    require(not path.is_symlink() and path.stat().st_size <= 131072, "unsafe manifest")
    value = strict_json(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict) and set(value) == ROOT_KEYS, "manifest fields differ")
    require(value["schema"] == "avionix.ems-responsive-state-golden.v1", "manifest schema differs")
    require(value["status"] == "SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE", "not a software reference")
    require(value["platform"] == "offscreen" and value["role"] == "ems"
            and value["fixture"] == "HmiGoldenMain.qml", "wrong render platform/fixture")
    timestamp = datetime.fromisoformat(value["generated_utc"])
    require(timestamp.utcoffset() == timedelta(0), "manifest timestamp must be UTC")
    rows = value["images"]
    require(isinstance(rows, list) and len(rows) == 18, "expected exactly 18 captures")
    for row, (w, h, state, age) in zip(rows, EXPECTED):
        require(isinstance(row, dict) and set(row) == IMAGE_KEYS, "image fields differ")
        require(all(type(row[k]) is int for k in ("width", "height", "age_ms", "bytes")), "non-integer image metadata")
        require((row["width"], row["height"], row["state"], row["age_ms"]) == (w, h, state, age), "wrong state/resolution/age/order")
        require(row["file"] == filename(w, h, state), "unsafe or wrong image name")
        for key in ("sha256", "pixel_sha256"):
            require(isinstance(row[key], str) and re.fullmatch(r"[0-9a-f]{64}", row[key]), "invalid image digest")
    return value


def image_pixels(directory, row):
    path = directory / row["file"]
    require(not path.is_symlink() and path.is_file(), "image missing or symlink")
    require(0 < path.stat().st_size == row["bytes"] <= 8388608, "image size differs/exceeds limit")
    require(sha256(path) == row["sha256"], "encoded image digest differs")
    require(png_size(path) == (row["width"], row["height"]), "PNG dimensions differ")
    pixels = png_pixels(path)
    require(hashlib.sha256(pixels).hexdigest() == row["pixel_sha256"], "decoded pixel digest differs")
    return pixels


def read_review(path, archive_digest, source_commit, rows, reference_digest):
    if path is None:
        return [{"file": row["file"], "pixel_sha256": row["pixel_sha256"],
                 "decision": "NOT_REVIEWED", "reviewer": "", "reason": ""} for row in rows]
    require(not path.is_symlink() and path.stat().st_size <= 131072, "unsafe review file")
    review = strict_json(path.read_text(encoding="utf-8"))
    require(isinstance(review, dict) and set(review) == {"schema", "archive_sha256", "source_commit", "reference_manifest_sha256", "decisions"}, "review fields differ")
    require(review["schema"] == "avionix.ems-golden-review-decisions.v1", "review schema differs")
    require(review["archive_sha256"] == archive_digest and review["source_commit"] == source_commit, "review source binding differs")
    require(review["reference_manifest_sha256"] == reference_digest, "review reference binding differs")
    decisions = review["decisions"]
    require(isinstance(decisions, list) and len(decisions) == 18, "incomplete review")
    for decision, row in zip(decisions, rows):
        require(isinstance(decision, dict) and set(decision) == {"file", "pixel_sha256", "decision", "reviewer", "reason"}, "decision fields differ")
        require(decision["file"] == row["file"] and decision["pixel_sha256"] == row["pixel_sha256"], "decision pixel binding differs")
        require(decision["decision"] in ("NOT_REVIEWED", "ACCEPT", "CHANGES_REQUIRED"), "unknown review decision")
        require(all(isinstance(decision[k], str) and len(decision[k]) <= 2000 for k in ("reviewer", "reason")), "invalid reviewer/reason")
        if decision["decision"] != "NOT_REVIEWED":
            require(bool(decision["reviewer"].strip()) and bool(decision["reason"].strip()), "decision needs reviewer and reason")
    return decisions


def prepare(archive, expected_sha256, reference, source_commit, run_id, artifact_id, review=None):
    require(isinstance(expected_sha256, str) and re.fullmatch(r"[0-9a-f]{64}", expected_sha256), "invalid archive digest")
    require(isinstance(source_commit, str) and re.fullmatch(r"[0-9a-f]{40}", source_commit), "full source commit required")
    require(type(run_id) is int and run_id > 0 and type(artifact_id) is int and artifact_id > 0, "invalid CI identifiers")
    require(not archive.is_symlink() and archive.stat().st_size <= 16777216, "unsafe/oversized archive")
    require(sha256(archive) == expected_sha256, "archive digest differs")
    require(not reference.is_symlink(), "reference directory must not be a symlink")
    baseline = manifest(reference)
    with tempfile.TemporaryDirectory(prefix="efis-golden-review-") as temporary:
        candidate = Path(temporary)
        with zipfile.ZipFile(archive) as bundle:
            entries = bundle.infolist()
            require(len(entries) == 19 and {entry.filename for entry in entries} == set(NAMES + ["manifest.json"]), "unexpected/missing/duplicate archive paths")
            require(sum(entry.file_size for entry in entries) <= 67108864, "expanded archive exceeds limit")
            for entry in entries:
                mode = (entry.external_attr >> 16) & 0xFFFF
                require(not stat.S_ISLNK(mode) and not entry.is_dir() and not (entry.flag_bits & 1), "unsupported archive entry")
                limit = 131072 if entry.filename == "manifest.json" else 8388608
                require(0 < entry.file_size <= limit, "archive entry exceeds limit")
                with bundle.open(entry) as stream:
                    data = stream.read(limit + 1)
                require(len(data) == entry.file_size, "archive entry length differs")
                (candidate / entry.filename).write_bytes(data)
        rendered = manifest(candidate)
        decisions = read_review(review, expected_sha256, source_commit, rendered["images"], sha256(reference / "manifest.json"))
        images = []
        for old, new, decision in zip(baseline["images"], rendered["images"], decisions):
            old_pixels, new_pixels = image_pixels(reference, old), image_pixels(candidate, new)
            changed = old["pixel_sha256"] != new["pixel_sha256"]
            peak, mean, fraction = difference(old_pixels, new_pixels) if changed else (0, 0., 0.)
            images.append({"file": new["file"], "state": new["state"], "width": new["width"], "height": new["height"],
                "candidate_sha256": new["sha256"], "candidate_pixel_sha256": new["pixel_sha256"],
                "reference_pixel_sha256": old["pixel_sha256"], "pixels_changed": changed,
                "max_channel_delta": peak, "mean_channel_delta": round(mean, 6),
                "changed_channel_fraction": round(fraction, 6), "review": decision})
    state = ("CHANGES_REQUIRED" if any(d["decision"] == "CHANGES_REQUIRED" for d in decisions) else
             "REVIEW_COMPLETE_NEEDS_AUTHORIZATION" if all(d["decision"] == "ACCEPT" for d in decisions) else "PENDING_REVIEW")
    return {"schema": "avionix.ems-golden-review.v1", "status": state,
        "source_commit": source_commit, "repository": "sfuso68/efis-next-professional",
        "ci_run_id": run_id, "ci_artifact_id": artifact_id, "archive_sha256": expected_sha256,
        "provenance": "CALLER_DECLARED_CI_IDS_WITH_EXTERNAL_ARCHIVE_DIGEST_NOT_SIGNATURE",
        "reference_manifest_sha256": sha256(reference / "manifest.json"),
        "baseline_update_authorized": False, "flight_approval": False,
        "images": images}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--run-id", required=True, type=int)
    parser.add_argument("--artifact-id", required=True, type=int)
    parser.add_argument("--review", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        # Fail before expensive validation; exclusive create below also closes races.
        require(not args.output.exists() and not args.output.is_symlink(), "output already exists")
        require(not args.output.resolve().is_relative_to(args.reference.resolve()), "output must be outside reference directory")
        result = prepare(args.archive, args.sha256, args.reference, args.source_commit,
                         args.run_id, args.artifact_id, args.review)
        with args.output.open("x", encoding="utf-8") as stream:
            stream.write(json.dumps(result, indent=2) + "\n")
    except (OSError, ValueError, KeyError, TypeError, zipfile.BadZipFile) as error:
        print(json.dumps({"status": "INVALID", "error": str(error), "baseline_update_authorized": False}))
        return 1
    print(json.dumps({"status": result["status"], "images": len(result["images"]), "baseline_update_authorized": False}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
