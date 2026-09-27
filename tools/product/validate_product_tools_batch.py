#!/usr/bin/env python3
"""Validate the five fail-closed product readiness registries."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PATHS = {
    "contract": ROOT / "config/product/avionix-product-contracts-batch-v1.json",
    "engine": ROOT / "config/product/engine-source-registry-v1.json",
    "sku": ROOT / "config/product/sku-capability-manifest-v1.json",
    "fpv": ROOT / "config/video/fpv-capacity-plan-v1.json",
    "video": ROOT / "config/video/efis-cam-lab.json",
    "voice": ROOT / "config/voice/phraseology-pack-registry-v1.json",
    "voice_config": ROOT / "config/voice/voice-assist-v1.json",
    "hil": ROOT / "config/hil/avionix-hil-scenarios-v1.json",
}


class RegistryError(ValueError):
    """A readiness registry claims authority or weakens a safety boundary."""


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise RegistryError(f"{path}: root must be an object")
    return value


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise RegistryError(f"{control}: {message}")


def validate(paths: dict[str, Path] = DEFAULT_PATHS) -> dict:
    documents = {name: load(path) for name, path in paths.items()}
    contract = documents["contract"]
    require(contract.get("execution_authorized") is False, "PT-ROOT-01", "contract execution became authorized")
    require(contract.get("flight_approval") is False, "PT-ROOT-01", "contract flight approval changed")

    engine = documents["engine"]
    require(engine.get("schema") == "avionix-next.engine-source-registry.v1", "PT-EMS-01", "wrong schema")
    require(engine.get("activation_allowed") is False, "PT-EMS-01", "unreviewed engine registry is activatable")
    require(engine.get("decision") == "NO_GO_UNTIL_APPLICABLE_SOURCES_REVIEWED", "PT-EMS-01", "engine decision changed")
    engines = engine.get("engines")
    require(isinstance(engines, list) and [item.get("id") for item in engines] == ["ROTAX_912_UL", "ROTAX_912_ULS"], "PT-EMS-02", "engine registry changed")
    for item in engines:
        require(item.get("serial_number") == "UNRESOLVED", "PT-EMS-03", "serial asserted without review")
        require(item.get("manual_document_id") == "UNRESOLVED" and item.get("manual_revision") == "UNRESOLVED", "PT-EMS-04", "manual asserted without review")
        require(item.get("limit_page_references") == [] and item.get("limits") == [], "PT-EMS-05", "limits asserted without sources")
        require(item.get("review_state") == "NOT_REVIEWED" and item.get("operational_use_allowed") is False, "PT-EMS-06", "unreviewed engine became operational")

    sku = documents["sku"]
    require(sku.get("schema") == "avionix-next.sku-capability-manifest.v1", "PT-SKU-01", "wrong schema")
    require(sku.get("single_codebase") is True, "PT-SKU-02", "profiles diverged from one codebase")
    require(sku.get("inflight_profile_switch_allowed") is False and sku.get("unknown_capability_policy") == "REJECT", "PT-SKU-03", "profile boundary weakened")
    profiles = sku.get("profiles")
    require(isinstance(profiles, list) and [item.get("id") for item in profiles] == ["STANDARD", "PROFESSIONAL"], "PT-SKU-04", "profile registry changed")
    required_shared = {"HEALTH_CAS", "LOGGING", "SETUP"}
    for profile in profiles:
        require(required_shared <= set(profile.get("capabilities", [])), "PT-SKU-05", "shared safety capabilities missing")
        require(profile.get("release_manifest") == "UNRESOLVED" and profile.get("approved") is False, "PT-SKU-06", "unreviewed profile became deployable")
    allocations = {item["id"]: set(item.get("required_nodes", [])) for item in profiles}
    require({"PFD_RPI5", "EMS_RPI4", "BLACKBOX_RPI5"} <= allocations["STANDARD"], "PT-SKU-07", "Standard allocation incomplete")
    require({"MFD_RPI5", "FPV_RPI4"} <= allocations["PROFESSIONAL"], "PT-SKU-07", "Professional allocation incomplete")

    fpv = documents["fpv"]
    video = documents["video"]
    require(fpv.get("schema") == "avionix-next.fpv-capacity-plan.v1", "PT-FPV-01", "wrong schema")
    require(fpv.get("execution_authorized") is False and fpv.get("decoder_platform") == "RPI4", "PT-FPV-02", "FPV execution/platform changed")
    require(fpv.get("network_bridge_allowed") is False and fpv.get("operational_wifi_allowed") is False, "PT-FPV-03", "FPV isolation weakened")
    feeds = fpv.get("feeds")
    require(isinstance(feeds, list) and len(feeds) == 4, "PT-FPV-04", "exactly four feeds required")
    require([item.get("id") for item in feeds] == [item.get("id") for item in video.get("cameras", [])], "PT-FPV-04", "feed IDs drifted from video config")
    for feed in feeds:
        require(feed.get("codec") == "UNRESOLVED" and feed.get("resolution") == "UNRESOLVED" and feed.get("fps") == "UNRESOLVED", "PT-FPV-05", "unmeasured feed parameters asserted")
        require(feed.get("measured_bitrate_mbps") == "NOT_RUN", "PT-FPV-06", "bitrate claimed without measurement")
    require(set(fpv.get("measurements", {}).values()) == {"NOT_RUN"}, "PT-FPV-07", "measurements claimed without hardware run")

    voice = documents["voice"]
    voice_config = documents["voice_config"]
    require(voice.get("schema") == "avionix-next.phraseology-pack-registry.v1", "PT-VOI-01", "wrong schema")
    require(voice.get("offline_only") is True and voice.get("pilot_review_required") is True, "PT-VOI-02", "offline/review boundary weakened")
    require(voice.get("automatic_transmission_allowed") is False, "PT-VOI-03", "automatic transmission enabled")
    require(voice.get("approved_packs") == [] and voice.get("natural_voice_assets") == [], "PT-VOI-04", "unreviewed voice assets asserted")
    require(voice.get("supported_locales") == voice_config.get("languages"), "PT-VOI-05", "locale registry drifted")
    require(len(voice.get("required_pack_fields", [])) == 8, "PT-VOI-06", "phraseology provenance fields incomplete")

    hil = documents["hil"]
    require(hil.get("schema") == "avionix-next.hil-scenarios.v1", "PT-HIL-01", "wrong schema")
    require(hil.get("execution_authorized") is False and hil.get("fixture") == "UNRESOLVED", "PT-HIL-02", "HIL became executable")
    for key in ("production_network_allowed", "production_storage_allowed", "actuator_output_allowed", "flight_control_command_allowed"):
        require(hil.get(key) is False, "PT-HIL-03", f"unsafe HIL boundary: {key}")
    scenarios = hil.get("scenarios")
    require(isinstance(scenarios, list) and len(scenarios) == 6, "PT-HIL-04", "scenario catalog changed")
    require(len({item.get("id") for item in scenarios}) == 6 and len({item.get("seed") for item in scenarios}) == 6, "PT-HIL-05", "scenario IDs/seeds must be unique")
    for item in scenarios:
        require(isinstance(item.get("seed"), int) and item["seed"] > 0, "PT-HIL-06", "invalid deterministic seed")
        require(item.get("input_sha256") == "UNRESOLVED", "PT-HIL-07", "input digest asserted before fixture approval")
        require(re.fullmatch(r"[A-Z][A-Z0-9_]*", str(item.get("expected_state", ""))) is not None, "PT-HIL-08", "invalid expected state")
    require(hil.get("same_replay_all_consumers") is True and hil.get("operator_and_utc_required") is True, "PT-HIL-09", "evidence boundary weakened")

    return {
        "schema": "avionix-next.product-tools-validation.v1",
        "status": "PASS",
        "execution_status": "NOT_RUN",
        "execution_authorized": False,
        "flight_approval": False,
        "registries": 5,
        "engines": len(engines),
        "profiles": len(profiles),
        "fpv_feeds": len(feeds),
        "approved_phraseology_packs": len(voice["approved_packs"]),
        "hil_scenarios": len(scenarios),
        "decision": "NO_GO_PENDING_INTERACTIVE_EVIDENCE",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    try:
        report = validate()
    except (RegistryError, KeyError, TypeError, json.JSONDecodeError, OSError) as exc:
        print(f"AVIONIX product tools: FAIL: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
