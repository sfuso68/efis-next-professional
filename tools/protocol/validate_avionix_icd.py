#!/usr/bin/env python3
"""Fail-closed validator and independent vector builder for AVIONIX ICD V1."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SPEC = ROOT / "config/protocols/avionix-data-icd-v1.json"
DEFAULT_VECTORS = ROOT / "tests/data/protocols/avionix-data-icd-v1-vectors.json"
DEFAULT_EVIDENCE_SCHEMA = ROOT / "config/protocols/avionix-evidence-bundle-v1.schema.json"

FROZEN_ENUMS = {
    "source_id": {"unknown": 0, "simulation": 1, "pixhawk_primary": 2,
                  "esp32_legacy": 3, "dlvr_legacy": 4, "nav_rf": 5,
                  "nav_synthetic": 6, "pilot_setting": 7, "blackbox_broker": 8},
    "quality": {"unknown": 0, "poor": 1, "nominal": 2, "high": 3},
    "validity": {"valid": 0, "stale": 1, "unavailable": 2,
                 "out_of_range": 3, "disagree": 4, "simulated": 5, "degraded": 6},
    "unit": {"unknown": 0, "none": 1, "boolean": 2, "degree": 3, "knot": 4,
             "foot": 5, "foot_per_minute": 6, "dot": 7,
             "revolutions_per_minute": 8, "inch_mercury": 9,
             "pound_per_square_inch": 10, "celsius": 11, "gallon": 12,
             "gallon_per_hour": 13, "volt": 14, "ampere": 15,
             "hectopascal": 16, "degree_per_second": 17, "gravity": 18,
             "pascal": 19},
}


def crc32c(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for octet in data:
        crc ^= octet
        for _ in range(8):
            crc = (crc >> 1) ^ (0x82F63B78 if crc & 1 else 0)
    return crc ^ 0xFFFFFFFF


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path}: root must be an object")
    return value


def _layout_errors(name: str, fields: Any, expected_size: int) -> list[str]:
    errors: list[str] = []
    if not isinstance(fields, list) or not fields:
        return [f"{name}: layout must be a non-empty array"]
    cursor = 0
    names: set[str] = set()
    allowed = {"bytes", "u8", "u16", "u32", "u64", "f64"}
    for index, field in enumerate(fields):
        if not isinstance(field, list) or len(field) != 4:
            errors.append(f"{name}[{index}]: expected [name, offset, size, type]")
            continue
        field_name, offset, size, kind = field
        if field_name in names:
            errors.append(f"{name}: duplicate field {field_name}")
        names.add(field_name)
        if kind not in allowed:
            errors.append(f"{name}.{field_name}: unsupported type {kind}")
        if not isinstance(offset, int) or not isinstance(size, int) or size <= 0:
            errors.append(f"{name}.{field_name}: invalid offset/size")
            continue
        if offset != cursor:
            errors.append(f"{name}.{field_name}: offset {offset}, expected {cursor}")
        cursor = offset + size
    if cursor != expected_size:
        errors.append(f"{name}: size {cursor}, expected {expected_size}")
    return errors


def validate_spec(spec: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if spec.get("schema") != "avionix-next.data-icd.v1":
        errors.append("spec: unsupported schema")
    if spec.get("revision") != "1.0.0" or spec.get("status") != "FROZEN-SW":
        errors.append("spec: revision/status must be 1.0.0/FROZEN-SW")
    authority = spec.get("authority", {})
    if authority.get("default_source_node") != 47 or authority.get("flight_approval") is not False:
        errors.append("spec: authority must pin node 47 and flight_approval=false")
    freshness = spec.get("freshness", {})
    if freshness.get("stale_after_ms") != 250 or freshness.get("lost_after_ms") != 1000:
        errors.append("spec: frozen freshness must be 250/1000 ms")
    if crc32c(b"123456789") != 0xE3069283:
        errors.append("validator: CRC32C implementation failed known vector")
    if spec.get("enumerations") != FROZEN_ENUMS:
        errors.append("spec: common enumerations differ from frozen values")

    protocols = spec.get("protocols", {})
    ddp = protocols.get("DDP_V2", {})
    edp = protocols.get("EDP_V1", {})
    if (ddp.get("magic_ascii"), ddp.get("schema_version"), ddp.get("default_udp_port")) != ("EFN2", 2, 49002):
        errors.append("DDP_V2: identity/endpoint mismatch")
    if (edp.get("magic_ascii"), edp.get("schema_version"), edp.get("default_udp_port")) != ("EDP1", 1, 49003):
        errors.append("EDP_V1: identity/endpoint mismatch")
    if ddp.get("profiles") != {"standard": 1, "professional": 2, "degraded": 3}:
        errors.append("DDP_V2: profiles differ from frozen values")
    if ddp.get("payload_types") != {"snapshot": 1, "alert_reserved": 2, "heartbeat_reserved": 3, "config_reserved": 4}:
        errors.append("DDP_V2: payload types differ from frozen values")
    if ddp.get("health") != {"normal": 0, "advisory": 1, "caution": 2, "warning": 3}:
        errors.append("DDP_V2: health enum differs from frozen values")
    if edp.get("health") != {"normal": 0, "caution": 1, "warning": 2}:
        errors.append("EDP_V1: health enum differs from frozen values")
    if edp.get("injected_fault") != {"none": 0, "low_oil_pressure": 1, "high_cht": 2,
                                      "alternator_failure": 3, "low_fuel_pressure": 4}:
        errors.append("EDP_V1: injected fault enum differs from frozen values")
    for label, protocol in (("DDP_V2", ddp), ("EDP_V1", edp)):
        errors += _layout_errors(f"{label}.envelope", protocol.get("envelope"), protocol.get("envelope_size", -1))
        errors += _layout_errors(f"{label}.payload_header", protocol.get("payload_header"), protocol.get("payload_header_size", -1))
        errors += _layout_errors(f"{label}.sample", protocol.get("sample"), protocol.get("sample_size", -1))
        signals = protocol.get("signals")
        if not isinstance(signals, list) or len(signals) != protocol.get("sample_count"):
            errors.append(f"{label}: signal count mismatch")
        elif len({row[1] for row in signals if isinstance(row, list) and len(row) == 3}) != len(signals):
            errors.append(f"{label}: duplicate or malformed signal rows")
    if ddp.get("payload_header_size", 0) + ddp.get("sample_count", 0) * ddp.get("sample_size", 0) > ddp.get("payload_max_bytes", 0):
        errors.append("DDP_V2: canonical payload exceeds maximum")
    expected_edp = edp.get("payload_header_size", 0) + edp.get("sample_count", 0) * edp.get("sample_size", 0)
    if expected_edp != edp.get("payload_size"):
        errors.append("EDP_V1: payload size is not structural size")
    return errors


def _sample_values(vectors: dict[str, Any]) -> tuple[int, int, int, int, int, int]:
    contract = vectors["sample_contract"]
    return (contract["source"], contract["validity"], contract["quality"],
            contract["age_ms"], contract["simulated"], contract["origin_node"])


def build_vectors(spec: dict[str, Any], vectors: dict[str, Any]) -> dict[str, dict[str, Any]]:
    identity = vectors["canonical_identity"]
    source, validity, quality, age_ms, simulated, origin_node = _sample_values(vectors)
    protocols = spec["protocols"]

    ddp = protocols["DDP_V2"]
    ddp_payload = struct.pack(">BIIIIIHH", 0, 0, 0, 0, 0, 0, 0, ddp["sample_count"])
    for _, signal, unit in ddp["signals"]:
        value = 1.0 if signal in (400, 502) else float(signal)
        ddp_payload += struct.pack(">HdHBBBIB", signal, value, source, validity, quality, unit, age_ms, simulated)
    ddp_crc = crc32c(ddp_payload)
    ddp_wire = struct.pack(
        ">4sHBBIQQQII", b"EFN2", 2, vectors["vectors"]["DDP_V2"]["profile"],
        vectors["vectors"]["DDP_V2"]["payload_type"], identity["source_node"],
        identity["publisher_instance"], identity["sequence"], identity["monotonic_us"],
        len(ddp_payload), ddp_crc) + ddp_payload

    edp = protocols["EDP_V1"]
    edp_vector = vectors["vectors"]["EDP_V1"]
    edp_payload = struct.pack(">BBH", edp_vector["health"], edp_vector["injected_fault"], edp["sample_count"])
    for channel, (_, signal, unit) in enumerate(edp["signals"], start=1):
        edp_payload += struct.pack(
            ">HdHBBBIBHHId", signal, float(signal), source, validity, quality, unit,
            age_ms, simulated, origin_node, channel,
            vectors["sample_contract"]["calibration_revision"], float(signal))
    edp_crc = crc32c(edp_payload)
    edp_wire = struct.pack(
        ">4sHIQQQII", b"EDP1", 1, identity["source_node"], identity["publisher_instance"],
        identity["sequence"], identity["monotonic_us"], len(edp_payload), edp_crc) + edp_payload

    def result(wire: bytes, payload_crc: int) -> dict[str, Any]:
        return {"payload_crc32c": f"{payload_crc:08x}", "wire_sha256": hashlib.sha256(wire).hexdigest(), "wire_size": len(wire)}
    return {"DDP_V2": result(ddp_wire, ddp_crc), "EDP_V1": result(edp_wire, edp_crc)}


def validate_vectors(spec: dict[str, Any], vectors: dict[str, Any]) -> tuple[list[str], dict[str, dict[str, Any]]]:
    errors: list[str] = []
    if vectors.get("schema") != "avionix-next.data-icd-vectors.v1":
        errors.append("vectors: unsupported schema")
    try:
        actual = build_vectors(spec, vectors)
    except (KeyError, TypeError, ValueError, struct.error) as exc:
        return [f"vectors: cannot build canonical frames: {exc}"], {}
    expected = vectors.get("vectors", {})
    for protocol, measured in actual.items():
        declared = expected.get(protocol, {})
        for key, value in measured.items():
            if declared.get(key) != value:
                errors.append(f"vectors.{protocol}.{key}: {declared.get(key)!r}, expected {value!r}")
    return errors, actual


def validate_evidence_schema(schema: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    required = schema.get("required", [])
    must_have = {"schema", "evidence_id", "requirement_ids", "gate", "level", "status", "created_utc", "producer", "environment", "artifacts", "checks", "limitations", "flight_approval"}
    if schema.get("$id") != "urn:avionix-next:evidence-bundle:v1":
        errors.append("evidence schema: incorrect $id")
    if set(required) != must_have:
        errors.append("evidence schema: required properties differ from frozen set")
    properties = schema.get("properties", {})
    if properties.get("flight_approval", {}).get("const") is not False:
        errors.append("evidence schema: flight_approval must be const false")
    levels = properties.get("level", {}).get("enum", [])
    if levels != ["VERIFIED_SW", "EVIDENCE_HW_HISTORICAL", "VERIFY_HW", "BLOCKED"]:
        errors.append("evidence schema: evidence levels differ from frozen order")
    if schema.get("additionalProperties") is not False:
        errors.append("evidence schema: root must reject unknown properties")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--spec", type=Path, default=DEFAULT_SPEC)
    parser.add_argument("--vectors", type=Path, default=DEFAULT_VECTORS)
    parser.add_argument("--evidence-schema", type=Path, default=DEFAULT_EVIDENCE_SCHEMA)
    parser.add_argument("--print-vector-digests", action="store_true")
    args = parser.parse_args()
    try:
        spec = load_json(args.spec)
        vectors = load_json(args.vectors)
        evidence = load_json(args.evidence_schema)
        errors = validate_spec(spec)
        vector_errors, actual = validate_vectors(spec, vectors)
        errors += vector_errors
        errors += validate_evidence_schema(evidence)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 2
    if args.print_vector_digests:
        print(json.dumps(actual, indent=2, sort_keys=True))
    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print("AVIONIX ICD V1: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
