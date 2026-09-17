#!/usr/bin/env python3
"""Validate and independently assess an EDP passive-capture report."""

from __future__ import annotations

import argparse
import hashlib
import ipaddress
import json
import math
from pathlib import Path
import re
import sys
from datetime import datetime, timezone


sys.path.insert(0, str(Path(__file__).resolve().parent))
import capture_edp_observer as capture  # noqa: E402


SCHEMA = "efis-next.edp-capture-assessment.v1"
SOURCE_SCHEMA = "efis-next.edp-passive-capture.v1"
MAX_REPORT_BYTES = 1_048_576
REQUIRED_CHECKS = frozenset((
    "duration_covered", "minimum_datagram_rate", "frames_present",
    "malformed_within_limit", "identity_rejected_zero",
    "sequence_gap_within_limit", "duplicates_within_limit",
    "reversed_within_limit", "publisher_restarts_within_limit",
    "timing_samples_present", "maximum_interarrival_within_limit",
    "p95_interarrival_within_limit", "socket_errors_zero",
))
GROUPS = {
    "integrity": (
        "duration_covered", "frames_present", "malformed_within_limit",
        "socket_errors_zero",
    ),
    "authority": (
        "identity_rejected_zero", "publisher_restarts_within_limit",
    ),
    "continuity": (
        "minimum_datagram_rate", "sequence_gap_within_limit",
        "duplicates_within_limit", "reversed_within_limit",
    ),
    "uniformity": (
        "timing_samples_present", "maximum_interarrival_within_limit",
        "p95_interarrival_within_limit",
    ),
}
COUNTERS = (
    "packets_seen", "packets_ignored", "frames", "bytes", "malformed",
    "identity_rejected", "sequence_gaps", "duplicates", "reversed",
    "publisher_restarts", "simulated_frames", "real_frames",
    "mixed_provenance_frames", "simulated_samples", "total_samples",
    "socket_errors",
)
LIMITS = (
    "minimum_hz", "maximum_gap_percent", "maximum_malformed",
    "maximum_duplicates", "maximum_reversed", "maximum_publisher_restarts",
    "maximum_interarrival_ms", "maximum_p95_interarrival_ms",
)
INTEGER_LIMITS = frozenset((
    "maximum_malformed", "maximum_duplicates", "maximum_reversed",
    "maximum_publisher_restarts",
))


class ReportError(ValueError):
    pass


def finite_number(value: object, name: str, *, minimum: float = 0.0) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ReportError(f"{name} must be numeric")
    number = float(value)
    if not math.isfinite(number) or number < minimum:
        raise ReportError(f"{name} is outside the accepted range")
    return number


def counter(document: dict[str, object], name: str) -> int:
    value = document.get(name)
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ReportError(f"{name} must be a non-negative integer")
    return value


def load_report(path: Path) -> tuple[dict[str, object], str]:
    raw = path.read_bytes()
    if not raw or len(raw) > MAX_REPORT_BYTES:
        raise ReportError("report size is outside the accepted range")
    try:
        document = json.loads(raw)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ReportError(f"invalid JSON report: {error}") from error
    if not isinstance(document, dict):
        raise ReportError("report root must be an object")
    validate_report(document)
    return document, hashlib.sha256(raw).hexdigest()


def validate_endpoint(document: dict[str, object]) -> None:
    endpoint = document.get("endpoint")
    if not isinstance(endpoint, dict):
        raise ReportError("endpoint must be an object")
    try:
        source = ipaddress.ip_address(endpoint.get("source"))
        destination = ipaddress.ip_address(endpoint.get("destination"))
    except ValueError as error:
        raise ReportError("endpoint addresses must be valid IP addresses") from error
    if source.version != 4 or destination.version != 4:
        raise ReportError("EDP observer endpoint must use IPv4")
    port = endpoint.get("port")
    if isinstance(port, bool) or not isinstance(port, int) or not 1 <= port <= 65535:
        raise ReportError("endpoint port is invalid")


def validate_streams(document: dict[str, object]) -> None:
    streams = document.get("streams")
    if not isinstance(streams, list):
        raise ReportError("streams must be an array")
    expected_node = counter(document, "expected_source_node")
    identities: set[tuple[int, int]] = set()
    sums = {name: 0 for name in ("frames", "gaps", "duplicates", "reversed")}
    current = 0
    for item in streams:
        if not isinstance(item, dict):
            raise ReportError("stream entries must be objects")
        node = item.get("source_node")
        publisher = item.get("publisher_instance")
        if (isinstance(node, bool) or not isinstance(node, int) or
                isinstance(publisher, bool) or not isinstance(publisher, int) or
                node != expected_node or publisher <= 0):
            raise ReportError("stream authority is invalid")
        identity = (node, publisher)
        if identity in identities:
            raise ReportError("duplicate stream identity")
        identities.add(identity)
        for field in sums:
            value = item.get(field)
            if isinstance(value, bool) or not isinstance(value, int) or value < 0:
                raise ReportError(f"stream {field} is invalid")
            sums[field] += value
        is_current = item.get("current")
        if not isinstance(is_current, bool):
            raise ReportError("stream current flag must be boolean")
        current += is_current
        first, last = item.get("first_sequence"), item.get("last_sequence")
        if not isinstance(first, int) or not isinstance(last, int) or first <= 0 or last <= 0:
            raise ReportError("stream sequence bounds are invalid")
    frames = counter(document, "frames")
    if sums["frames"] != frames:
        raise ReportError("stream frames do not reconcile with report")
    for report_name, stream_name in (("sequence_gaps", "gaps"),
                                     ("duplicates", "duplicates"),
                                     ("reversed", "reversed")):
        if sums[stream_name] != counter(document, report_name):
            raise ReportError(f"stream {stream_name} do not reconcile with report")
    if (frames == 0 and (streams or current)) or (frames > 0 and current != 1):
        raise ReportError("exactly one populated stream must be current")
    if counter(document, "publisher_restarts") != max(0, len(streams) - 1):
        raise ReportError("publisher restart count does not reconcile with streams")
    for item in streams:
        assert isinstance(item, dict)
        forward = int(item["frames"]) - 1 - int(item["duplicates"]) - int(item["reversed"])
        expected_span = max(0, forward) + int(item["gaps"])
        if int(item["last_sequence"]) - int(item["first_sequence"]) != expected_span:
            raise ReportError("stream sequence bounds do not reconcile")


def recompute_checks(document: dict[str, object]) -> dict[str, bool]:
    limits = document["acceptance_limits"]
    assert isinstance(limits, dict)
    namespace = argparse.Namespace(
        duration_seconds=finite_number(document.get("requested_duration_seconds"),
                                       "requested_duration_seconds", minimum=0.001),
        minimum_hz=float(limits["minimum_hz"]),
        max_gap_percent=float(limits["maximum_gap_percent"]),
        max_malformed=int(limits["maximum_malformed"]),
        max_duplicates=int(limits["maximum_duplicates"]),
        max_reversed=int(limits["maximum_reversed"]),
        max_publisher_restarts=int(limits["maximum_publisher_restarts"]),
        max_interarrival_ms=float(limits["maximum_interarrival_ms"]),
        max_p95_interarrival_ms=float(limits["maximum_p95_interarrival_ms"]),
    )
    clone = dict(document)
    return capture.evaluate(clone, namespace)


def validate_report(document: dict[str, object]) -> None:
    if document.get("schema") != SOURCE_SCHEMA:
        raise ReportError("unsupported passive capture schema")
    if document.get("mode") != capture.MODE:
        raise ReportError("capture mode is not passive AF_PACKET")
    if document.get("raw_payload_persisted") is not False:
        raise ReportError("raw payload persistence is forbidden")
    if document.get("flight_approval") is not False:
        raise ReportError("capture cannot grant flight approval")
    interface = document.get("interface")
    if not isinstance(interface, str) or not re.fullmatch(r"[A-Za-z0-9_.:-]{1,15}", interface):
        raise ReportError("interface is missing")
    timestamp = document.get("timestamp_utc")
    if not isinstance(timestamp, str):
        raise ReportError("timestamp_utc is missing")
    try:
        parsed_timestamp = datetime.fromisoformat(timestamp)
    except ValueError as error:
        raise ReportError("timestamp_utc is invalid") from error
    if parsed_timestamp.tzinfo is None or parsed_timestamp.utcoffset() != timezone.utc.utcoffset(None):
        raise ReportError("timestamp_utc must include UTC offset")
    validate_endpoint(document)
    for name in COUNTERS:
        counter(document, name)
    expected_node = counter(document, "expected_source_node")
    if not 1 <= expected_node <= 65535:
        raise ReportError("expected source node is outside the accepted range")
    frames = counter(document, "frames")
    if (counter(document, "simulated_frames") + counter(document, "real_frames") +
            counter(document, "mixed_provenance_frames") != frames):
        raise ReportError("feed provenance frame counts do not reconcile")
    total_samples = counter(document, "total_samples")
    simulated_samples = counter(document, "simulated_samples")
    if total_samples != frames * capture.SAMPLE_COUNT or simulated_samples > total_samples:
        raise ReportError("sample provenance counters do not reconcile")
    if counter(document, "simulated_frames") * capture.SAMPLE_COUNT > simulated_samples:
        raise ReportError("fully simulated frames exceed simulated samples")
    finite_number(document.get("requested_duration_seconds"),
                  "requested_duration_seconds", minimum=0.001)
    finite_number(document.get("actual_duration_seconds"),
                  "actual_duration_seconds")
    finite_number(document.get("datagram_hz"), "datagram_hz")
    gap_percent = finite_number(document.get("sequence_gap_percent"),
                                "sequence_gap_percent")
    denominator = frames + counter(document, "sequence_gaps")
    expected_percent = round(100.0 * counter(document, "sequence_gaps") /
                             denominator, 6) if denominator else 100.0
    if not math.isclose(gap_percent, expected_percent, abs_tol=0.0000005):
        raise ReportError("sequence gap percentage is inconsistent")
    errors = document.get("decode_errors")
    if not isinstance(errors, dict) or any(
        not isinstance(name, str) or isinstance(value, bool) or
        not isinstance(value, int) or value < 0 for name, value in errors.items()
    ):
        raise ReportError("decode_errors must contain non-negative counters")
    if sum(errors.values()) != counter(document, "malformed"):
        raise ReportError("decode error counters do not reconcile")
    if counter(document, "packets_seen") != (
        counter(document, "packets_ignored") + counter(document, "malformed") +
        counter(document, "identity_rejected") + frames
    ):
        raise ReportError("packet counters do not reconcile")
    if counter(document, "bytes") != frames * (capture.ENVELOPE_SIZE + capture.PAYLOAD_SIZE):
        raise ReportError("accepted byte count does not reconcile")
    timing = document.get("interarrival")
    if not isinstance(timing, dict):
        raise ReportError("interarrival must be an object")
    samples = timing.get("samples")
    if isinstance(samples, bool) or not isinstance(samples, int) or samples < 0:
        raise ReportError("interarrival sample count is invalid")
    if samples != max(0, frames - 1):
        raise ReportError("interarrival samples do not reconcile with frames")
    for name in ("average_ms", "maximum_ms", "p95_ms", "mdev_ms"):
        value = timing.get(name)
        if samples == 0:
            if value is not None:
                raise ReportError(f"{name} must be null without timing samples")
        else:
            finite_number(value, name)
    limits = document.get("acceptance_limits")
    if not isinstance(limits, dict) or set(limits) != set(LIMITS):
        raise ReportError("acceptance_limits are incomplete or unknown")
    for name in LIMITS:
        if (name in INTEGER_LIMITS and
                (isinstance(limits[name], bool) or
                 not isinstance(limits[name], int))):
            raise ReportError(f"{name} must be a non-negative integer")
        minimum = 0.0 if name.startswith("maximum_") else 0.000001
        finite_number(limits[name], name, minimum=minimum)
    if float(limits["maximum_gap_percent"]) > 100:
        raise ReportError("maximum gap percentage exceeds 100")
    if float(limits["maximum_p95_interarrival_ms"]) > float(limits["maximum_interarrival_ms"]):
        raise ReportError("p95 timing limit exceeds maximum timing limit")
    validate_streams(document)
    embedded = document.get("checks")
    if (not isinstance(embedded, dict) or set(embedded) != REQUIRED_CHECKS or
            any(not isinstance(value, bool) for value in embedded.values())):
        raise ReportError("checks are incomplete, unknown or non-boolean")
    recomputed = recompute_checks(document)
    if embedded != recomputed:
        raise ReportError("embedded checks do not match recomputed evidence")
    expected_status = "PASS" if all(recomputed.values()) else "FAIL"
    if document.get("status") != expected_status:
        raise ReportError("capture status does not match recomputed checks")


def classify(document: dict[str, object], digest: str) -> dict[str, object]:
    checks = recompute_checks(document)
    groups = {}
    for name, keys in GROUPS.items():
        failures = [key for key in keys if not checks[key]]
        groups[name] = {
            "status": "PASS" if not failures else "FAIL",
            "failed_checks": failures,
        }
    frames = int(document["frames"])
    simulated_samples = int(document["simulated_samples"])
    total_samples = int(document["total_samples"])
    feed = ("NO_DATA" if frames == 0 else
            "SIM_ONLY" if simulated_samples == total_samples else
            "REAL_ONLY" if simulated_samples == 0 else "MIXED")
    passes = all(group["status"] == "PASS" for group in groups.values())
    return {
        "schema": SCHEMA,
        "source_schema": SOURCE_SCHEMA,
        "source_sha256": digest,
        "evidence_status": "PASS" if passes else "FAIL",
        "feed_classification": feed,
        "qualification_status": (
            "BENCH_SIM_ONLY" if passes and feed == "SIM_ONLY" else
            "BENCH_MIXED" if passes and feed == "MIXED" else
            "BENCH_REAL_ONLY" if passes and feed == "REAL_ONLY" else
            "NOT_QUALIFIED"
        ),
        "groups": groups,
        "flight_approval": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        document, digest = load_report(args.report)
        result = classify(document, digest)
    except (OSError, ReportError) as error:
        print(f"assessment failed: {error}", file=sys.stderr)
        return 1
    if args.output is not None:
        capture.write_report(args.output, result)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["evidence_status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
