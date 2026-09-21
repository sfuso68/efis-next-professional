#!/usr/bin/env python3
"""Validate the frozen AVIONIX DDP/EDP contract against source and network plan."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys


class ContractError(ValueError):
    pass


ROOT = Path(__file__).resolve().parents[2]


def _restricted_integer_expression(expression: str) -> int:
    """Evaluate integers, multiplication and grouping without executing code."""
    pieces = re.findall(r"[0-9]+|[()*]|\s+", expression)
    if not expression or "".join(pieces) != expression:
        raise ContractError(f"unsupported C++ integer expression: {expression}")
    tokens = [piece for piece in pieces if not piece.isspace()]
    if not tokens:
        raise ContractError(f"unsupported C++ integer expression: {expression}")

    position = 0

    def factor() -> int:
        nonlocal position
        if position >= len(tokens):
            raise ContractError(f"incomplete C++ integer expression: {expression}")
        token = tokens[position]
        if token.isdigit():
            position += 1
            return int(token)
        if token == "(":
            position += 1
            value = product()
            if position >= len(tokens) or tokens[position] != ")":
                raise ContractError(f"unbalanced C++ integer expression: {expression}")
            position += 1
            return value
        raise ContractError(f"unsupported C++ integer expression: {expression}")

    def product() -> int:
        nonlocal position
        value = factor()
        while position < len(tokens) and tokens[position] == "*":
            position += 1
            value *= factor()
        return value

    result = product()
    if position != len(tokens):
        raise ContractError(f"unsupported C++ integer expression: {expression}")
    return result


def _integer(source: str, name: str) -> int:
    match = re.search(rf"\b{name}\s*(?:=\s*([^;]+)|\{{\s*([0-9]+)\s*\}})\s*;", source)
    if not match:
        raise ContractError(f"C++ constant not found: {name}")
    expression = (match.group(1) or match.group(2)).strip().replace("U", "")
    try:
        return _restricted_integer_expression(expression)
    except ContractError as error:
        raise ContractError(
            f"unsupported C++ integer expression for {name}: {expression}"
        ) from error


def _freshness_milliseconds(source: str, name: str) -> int:
    """Resolve a FreshnessBudget member expressed as tick or tick * N."""
    tick_match = re.search(
        r"\btick\s*=\s*std::chrono::milliseconds\{\s*([0-9]+)\s*\}\s*;",
        source,
    )
    if not tick_match:
        raise ContractError("FreshnessBudget tick not found")
    member_match = re.search(
        rf"\b{name}\s*=\s*tick(?:\s*\*\s*([0-9]+))?\s*;",
        source,
    )
    if not member_match:
        raise ContractError(f"FreshnessBudget member not found: {name}")
    multiplier = int(member_match.group(1) or "1")
    return int(tick_match.group(1)) * multiplier


def _contract(document: dict, contract_id: str) -> dict:
    matches = [item for item in document.get("contracts", []) if item.get("id") == contract_id]
    if len(matches) != 1:
        raise ContractError(f"expected exactly one {contract_id} contract")
    return matches[0]


def validate(root: Path, manifest_path: Path) -> dict:
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    if document.get("schema") != "avionix-next.data-contract.v1":
        raise ContractError("unsupported data-contract schema")
    if document.get("status") != "BENCH_NOT_FLIGHT_APPROVED":
        raise ContractError("contract must remain explicitly not flight approved")
    if document.get("publisher") != {"node_id": 47, "logical_node": "blackbox"}:
        raise ContractError("publisher identity must remain Blackbox node 47")

    ddp = _contract(document, "ddp-v2")
    edp = _contract(document, "edp-v1")
    headers = {
        "ddp-v2": (root / "include/efis/protocol/ddp_v2.hpp").read_text(encoding="utf-8"),
        "edp-v1": (root / "include/efis/protocol/edp_v1.hpp").read_text(encoding="utf-8"),
    }
    freshness_budget = (root / "include/efis/core/freshness_budget.hpp").read_text(
        encoding="utf-8"
    )
    transport_stale_ms = _freshness_milliseconds(
        freshness_budget, "transport_stale"
    )
    transport_lost_ms = _freshness_milliseconds(
        freshness_budget, "transport_lost"
    )

    expected = {
        "ddp-v2": {"schema_version": _integer(headers["ddp-v2"], "schema_version"),
                   "envelope_bytes": _integer(headers["ddp-v2"], "envelope_size"),
                   "maximum_payload_bytes": _integer(headers["ddp-v2"], "max_payload_size"),
                   "stale_after_ms": transport_stale_ms,
                   "lost_after_ms": transport_lost_ms},
        "edp-v1": {"schema_version": _integer(headers["edp-v1"], "schema_version"),
                   "envelope_bytes": _integer(headers["edp-v1"], "envelope_size"),
                   "sample_count": _integer(headers["edp-v1"], "engine_sample_count"),
                   "stale_after_ms": transport_stale_ms,
                   "lost_after_ms": transport_lost_ms},
    }
    for contract, actual in ((ddp, expected["ddp-v2"]), (edp, expected["edp-v1"])):
        for key, value in actual.items():
            if contract.get(key) != value:
                raise ContractError(f"{contract['id']}: {key}={contract.get(key)!r}, source={value!r}")
        if contract.get("transport") != "udp-unicast-ipv4" or contract.get("command_capability") is not False:
            raise ContractError(f"{contract['id']}: transport must be read-only UDP unicast IPv4")
        if contract.get("stale_after_ms", 0) <= 0 or contract.get("lost_after_ms", 0) <= contract.get("stale_after_ms", 0):
            raise ContractError(f"{contract['id']}: invalid freshness thresholds")

    network = json.loads((root / "config/network/avionics-network-v1.json").read_text(encoding="utf-8"))
    flows = {(item["source"], item["destination"], item["port"], item["segment"])
             for item in network.get("flows", [])}
    required = {
        *(('blackbox', consumer, ddp['port'], 'flight-data') for consumer in ddp['consumers']),
        *(('blackbox', consumer, edp['port'], 'flight-data') for consumer in edp['consumers']),
    }
    missing = required - flows
    if missing:
        raise ContractError(f"network plan missing contract flows: {sorted(missing)}")

    protocol_docs = {
        "ddp-v2": (root / "docs/protocols/DISPLAY_DATA_PROTOCOL_V2.md").read_text(encoding="utf-8"),
        "edp-v1": (root / "docs/protocols/ENGINE_DATA_PROTOCOL_V1.md").read_text(encoding="utf-8"),
    }
    for contract in (ddp, edp):
        text = protocol_docs[contract["id"]]
        for token in (contract["magic"], str(contract["schema_version"]), str(contract["port"]),
                      str(contract["sample_count"]), str(contract["stale_after_ms"]),
                      str(contract["lost_after_ms"])):
            if token not in text:
                raise ContractError(f"{contract['id']}: documentation token missing: {token}")

    return {"schema": document["schema"], "contracts": 2, "flows_checked": len(required),
            "source_constants_checked": sum(len(item) for item in expected.values()), "status": "PASS"}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args(argv)
    manifest = args.manifest or args.root / "config/protocol/avionix-data-contract-v1.json"
    try:
        print(json.dumps(validate(args.root, manifest), sort_keys=True))
    except (OSError, json.JSONDecodeError, ContractError, ValueError) as error:
        print(f"data contract invalid: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
