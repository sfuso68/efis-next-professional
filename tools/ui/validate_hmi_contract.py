#!/usr/bin/env python3
"""Fail-closed validation for the AVIONIX SkyView/EMS HMI contract."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / "config/ui/avionix-hmi-v1.json"
COMPONENTS = (
    "qml/HmiTokens.qml",
    "qml/HmiDataStateBadge.qml",
    "qml/HmiArcGaugeV4.qml",
    "qml/SkyViewCompassArc.qml",
    "qml/HmiComponentShowcase.qml",
)


def validate(data: dict) -> list[str]:
    errors: list[str] = []
    if data.get("schema") != "avionix.hmi-contract.v1":
        errors.append("unexpected schema")
    if data.get("status") != "DESIGN_BASELINE_NOT_FLIGHT_APPROVED":
        errors.append("status must remain DESIGN_BASELINE_NOT_FLIGHT_APPROVED")
    if data.get("logical_canvas") != {"width": 1024, "height": 600}:
        errors.append("logical canvas must be 1024x600")

    targets = {item.get("role"): item for item in data.get("targets", [])}
    if set(targets) != {"SKYVIEW", "EMS"}:
        errors.append("targets must contain exactly SKYVIEW and EMS")
    elif targets["SKYVIEW"].get("platform") != "RPi5" or targets["EMS"].get("platform") != "RPi4":
        errors.append("platform allocation violates ADR-013")

    tokens = data.get("design_tokens", {})
    if tokens.get("minimum_touch_px", 0) < 44:
        errors.append("touch target below 44 px")
    if tokens.get("minimum_primary_text_px", 0) < 34:
        errors.append("primary text below 34 px")
    if tokens.get("minimum_secondary_text_px", 0) < 18:
        errors.append("secondary text below 18 px")

    required_states = {"LIVE", "STALE", "LOST", "DEGRADED", "FAIL", "SIM"}
    if set(data.get("data_states", [])) != required_states:
        errors.append("data state set is incomplete or contains unknown states")
    if data.get("freshness_ms") != {"live_max": 250, "stale_max": 1000}:
        errors.append("freshness must match DDP/EDP 250/1000 ms")

    skyview = data.get("skyview", {})
    required_speeds = {"VS0", "VS1", "VX", "VY", "VFE", "VNO", "VNE"}
    if set(skyview.get("v_speed_labels", [])) != required_speeds:
        errors.append("required V-speed labels are incomplete")
    if skyview.get("hsi_visible_half_angle_deg") != 35:
        errors.append("HSI visible half-angle must be 35 degrees")
    if skyview.get("minimum_fps", 0) < 40:
        errors.append("SkyView minimum FPS below 40")

    ems = data.get("ems", {})
    if ems.get("primary_gauges") != ["RPM", "MAP"]:
        errors.append("EMS primary gauges must be RPM and MAP")
    if ems.get("cylinders") != 4 or set(ems.get("cylinder_channels", [])) != {"CHT", "EGT"}:
        errors.append("EMS cylinder matrix must expose four CHT/EGT channels")
    if ems.get("flap_detents_deg") != [0, 10, 15, 20, 30]:
        errors.append("unexpected flap detents")
    if set(ems.get("gear_channels", [])) != {"NOSE", "LEFT", "RIGHT"}:
        errors.append("landing gear channels are incomplete")
    if ems.get("minimum_fps", 0) < 40:
        errors.append("EMS minimum FPS below 40")

    degraded = data.get("degraded_mode", {})
    invariants = ("colour_is_not_only_cue", "last_value_must_show_age", "sim_always_visible", "no_control_authority")
    for key in invariants:
        if degraded.get(key) is not True:
            errors.append(f"degraded-mode safety invariant disabled: {key}")
    return errors


def validate_repository(root: Path = ROOT) -> list[str]:
    errors: list[str] = []
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    for relative in COMPONENTS:
        if not (root / relative).is_file():
            errors.append(f"missing HMI component: {relative}")
        if relative not in cmake:
            errors.append(f"HMI component not registered in CMake: {relative}")
    ems = (root / "qml/AdaptiveEmsView.qml").read_text(encoding="utf-8")
    if ems.count("HmiArcGaugeV4 {") != 5:
        errors.append("AdaptiveEmsView must use exactly five HmiArcGaugeV4 instances")
    if "dataState: controller.dataSourceState" not in ems:
        errors.append("EMS V4 gauges do not expose controller data state")
    compass = (root / "qml/SkyViewCompassArc.qml").read_text(encoding="utf-8")
    if "readonly property real halfAngle: 35" not in compass:
        errors.append("SkyView compass implementation diverges from the 35 degree contract")
    return errors


def main() -> int:
    data = json.loads(CONTRACT.read_text(encoding="utf-8"))
    errors = validate(data) + validate_repository()
    if errors:
        print("AVIONIX HMI contract FAILED")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("AVIONIX HMI contract PASS: 2 targets, 10 gates, fail-closed invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
