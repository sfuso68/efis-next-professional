#!/usr/bin/env python3
"""Fail-closed validation for the AVIONIX SkyView/EMS HMI contract."""

from __future__ import annotations

import json
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / "config/ui/avionix-hmi-v1.json"
COMPONENTS = (
    "qml/HmiTokens.qml",
    "qml/HmiDataStateBadge.qml",
    "qml/HmiArcGaugeV4.qml",
    "qml/HmiCylinderMatrixV2.qml",
    "qml/HmiAirframePanel.qml",
    "qml/HmiFuelStatus.qml",
    "qml/HmiElectricalSynoptic.qml",
    "qml/SkyViewCompassArc.qml",
    "qml/HmiComponentShowcase.qml",
    "qml/HmiGoldenController.qml",
    "qml/HmiGoldenMain.qml",
    "qml/SkyViewGoldenMain.qml",
)

CONTROL_IDS = tuple(f"HMI-B20-{index:02d}" for index in range(1, 21))
ROOT_KEYS = {"schema", "status", "logical_canvas", "targets", "design_tokens",
             "data_states", "freshness_ms", "skyview", "ems", "degraded_mode"}
PALETTE = {"background", "foreground", "cyan", "normal", "caution", "warning",
           "navigation", "invalid"}
SKYVIEW_ZONES = ["status_rail", "synthetic_vision", "airspeed_tape", "altitude_tape",
                 "heading_ribbon", "hsi_arc", "context_panel", "engine_strip"]
V_SPEEDS = ["VS0", "VS1", "VX", "VY", "VFE", "VNO", "VNE"]
EMS_PAGES = ["ENGINE", "LEAN", "FUEL", "TREND", "CHECKLIST", "MAINTENANCE", "SETTINGS"]
ELECTRICAL = ["BATTERY", "ALTERNATOR", "MAIN_BUS", "ESS_BUS", "FUEL_PUMP",
              "IGNITION_A", "IGNITION_B"]


def _error(errors: list[str], control: int, message: str) -> None:
    errors.append(f"[HMI-B20-{control:02d}] {message}")


def _object(value: object) -> dict:
    return value if isinstance(value, dict) else {}


def _number_at_least(value: object, minimum: float) -> bool:
    return (isinstance(value, (int, float)) and not isinstance(value, bool) and
            math.isfinite(value) and value >= minimum)


def validate(data: dict) -> list[str]:
    errors: list[str] = []
    if not isinstance(data, dict):
        return ["[HMI-B20-01] contract root must be an object"]
    if (set(data) != ROOT_KEYS or
            data.get("schema") != "avionix.hmi-contract.v1" or
            data.get("status") != "DESIGN_BASELINE_NOT_FLIGHT_APPROVED"):
        _error(errors, 1, "root/schema/status differ from the closed non-flight baseline")
    if data.get("logical_canvas") != {"width": 1024, "height": 600}:
        _error(errors, 2, "logical canvas must be exactly 1024x600")

    raw_targets = data.get("targets", [])
    target_rows_valid = (isinstance(raw_targets, list) and len(raw_targets) == 2 and
                         all(isinstance(item, dict) and
                             set(item) == {"role", "platform", "resolutions"}
                             for item in raw_targets))
    targets = {item.get("role"): item for item in raw_targets} if target_rows_valid else {}
    if set(targets) != {"SKYVIEW", "EMS"}:
        _error(errors, 3, "targets must contain exactly one SKYVIEW and one EMS role")
    elif targets["SKYVIEW"].get("platform") != "RPi5" or targets["EMS"].get("platform") != "RPi4":
        _error(errors, 3, "platform allocation violates ADR-013")
    expected_resolutions = {
        "SKYVIEW": [[1024, 600], [1280, 800], [1920, 1080]],
        "EMS": [[800, 480], [1024, 600], [1280, 800]],
    }
    if set(targets) == set(expected_resolutions) and any(
            targets[role].get("resolutions") != resolutions
            for role, resolutions in expected_resolutions.items()):
        _error(errors, 4, "target resolution matrices differ from the frozen order")
    elif set(targets) != set(expected_resolutions):
        _error(errors, 4, "target resolution matrices are unavailable")

    tokens = _object(data.get("design_tokens"))
    if (set(tokens) != {"minimum_touch_px", "minimum_primary_text_px",
                        "minimum_secondary_text_px", "minimum_stroke_px", "palette"} or
            not _number_at_least(tokens.get("minimum_touch_px"), 44) or
            not _number_at_least(tokens.get("minimum_primary_text_px"), 34) or
            not _number_at_least(tokens.get("minimum_secondary_text_px"), 18) or
            not _number_at_least(tokens.get("minimum_stroke_px"), 1.5)):
        _error(errors, 5, "touch, typography or stroke floor is incomplete")
    palette = _object(tokens.get("palette"))
    colours = list(palette.values())
    if (set(palette) != PALETTE or any(not isinstance(value, str) or
                                      not re.fullmatch(r"#[0-9A-F]{6}", value)
                                      for value in colours) or len(set(colours)) != len(colours)):
        _error(errors, 6, "palette must contain eight unique uppercase RGB semantic colours")

    if data.get("data_states") != ["LIVE", "STALE", "LOST", "DEGRADED", "FAIL", "SIM"]:
        _error(errors, 7, "data states differ from the frozen ordered set")
    if data.get("freshness_ms") != {"live_max": 250, "stale_max": 1000}:
        _error(errors, 8, "freshness must match DDP/EDP 250/1000 ms")

    skyview = _object(data.get("skyview"))
    if (set(skyview) != {"zones", "v_speed_labels", "hsi_visible_half_angle_deg",
                         "terrain", "overlays", "minimum_fps", "target_fps"} or
            skyview.get("zones") != SKYVIEW_ZONES):
        _error(errors, 9, "SkyView zones differ from the closed ordered layout")
    if skyview.get("v_speed_labels") != V_SPEEDS:
        _error(errors, 10, "required V-speed labels differ from the frozen order")
    if skyview.get("hsi_visible_half_angle_deg") != 35:
        _error(errors, 11, "HSI visible half-angle must be 35 degrees")
    if skyview.get("terrain") != {"source_required": True, "age_required": True,
                                   "runway_context_only": True}:
        _error(errors, 12, "terrain source/age/runway gates are incomplete")
    if skyview.get("overlays") != ["weather", "traffic", "obstacles", "route"]:
        _error(errors, 13, "SkyView overlays differ from the frozen order")

    ems = _object(data.get("ems"))
    if (not _number_at_least(skyview.get("minimum_fps"), 40) or
            not _number_at_least(skyview.get("target_fps"), 50) or
            skyview.get("target_fps", 0) < skyview.get("minimum_fps", 0) or
            not _number_at_least(ems.get("minimum_fps"), 40) or
            not _number_at_least(ems.get("target_fps"), 50) or
            ems.get("target_fps", 0) < ems.get("minimum_fps", 0)):
        _error(errors, 14, "minimum/target FPS policy is invalid")
    compact = _object(ems.get("compact_mode"))
    expected_compact = {
        "max_width_px": 820,
        "max_height_px": 500,
        "minimum_micro_text_px": 8,
        "minimum_action_text_px": 12,
        "minimum_touch_px": 44,
        "abbreviate_header_context": True,
    }
    if compact != expected_compact:
        _error(errors, 15, "EMS compact-mode legibility policy is incomplete")
    if (ems.get("primary_gauges") != ["RPM", "MAP"] or
            ems.get("secondary_gauges") != ["OIL_PRESS", "OIL_TEMP", "FUEL_FLOW"]):
        _error(errors, 16, "EMS primary/secondary gauge hierarchy differs from baseline")
    if ems.get("cylinders") != 4 or ems.get("cylinder_channels") != ["CHT", "EGT"]:
        _error(errors, 17, "EMS cylinder matrix must expose four ordered CHT/EGT channels")
    if (ems.get("flap_detents_deg") != [0, 10, 15, 20, 30] or
            ems.get("gear_channels") != ["NOSE", "LEFT", "RIGHT"]):
        _error(errors, 18, "flap detents or landing gear channels differ from baseline")
    expected_ems_keys = {"compact_mode", "primary_gauges", "secondary_gauges",
                         "cylinders", "cylinder_channels", "flap_detents_deg",
                         "gear_channels", "pages", "electrical_channels",
                         "minimum_fps", "target_fps"}
    if (set(ems) != expected_ems_keys or ems.get("pages") != EMS_PAGES or
            ems.get("electrical_channels") != ELECTRICAL):
        _error(errors, 19, "EMS pages/electrical channels differ from closed baseline")

    degraded = _object(data.get("degraded_mode"))
    expected_degraded = {
        "colour_is_not_only_cue": True,
        "last_value_may_remain_visible": True,
        "last_value_must_show_age": True,
        "sim_always_visible": True,
        "no_control_authority": True,
    }
    if degraded != expected_degraded:
        _error(errors, 20, "degraded-mode safety invariants differ from closed baseline")
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
    tokens_qml = (root / "qml/HmiTokens.qml").read_text(encoding="utf-8")
    if "readonly property bool compactLayout: width <= 820 || height <= 500" not in ems:
        errors.append("AdaptiveEmsView does not activate compact mode at 800x480")
    if "Math.max(hmiTokens.minimumTouch,48*root.uiScale)" not in ems:
        errors.append("EMS action controls do not preserve the 44 px touch minimum")
    if "compact:root.compactLayout" not in ems:
        errors.append("EMS components do not receive compact-mode state")
    for marker in ("minimumMicroText: 8", "minimumActionText: 12", "function legibleText"):
        if marker not in tokens_qml:
            errors.append(f"HMI typography token missing: {marker}")
    if ems.count("HmiArcGaugeV4 {") != 5:
        errors.append("AdaptiveEmsView must use exactly five HmiArcGaugeV4 instances")
    if "dataState: controller.dataSourceState" not in ems:
        errors.append("EMS V4 gauges do not expose controller data state")
    integrations = {
        "HmiCylinderMatrixV2 {": "cylinder matrix",
        "HmiAirframePanel {": "airframe panel",
        "HmiFuelStatus {": "fuel summary",
        "HmiElectricalSynoptic {": "electrical synoptic",
    }
    for marker, label in integrations.items():
        if ems.count(marker) != 1:
            errors.append(f"AdaptiveEmsView must integrate exactly one {label}")
    if "limitsVerified:false" not in ems:
        errors.append("EMS cylinder limits must remain explicitly unverified")
    compass = (root / "qml/SkyViewCompassArc.qml").read_text(encoding="utf-8")
    if "readonly property real halfAngle: 35" not in compass:
        errors.append("SkyView compass implementation diverges from the 35 degree contract")
    main_cpp = (root / "src/renderers/qt/main.cpp").read_text(encoding="utf-8")
    if ("--hmi-golden" not in main_cpp or "--skyview-golden" not in main_cpp or
            "EFIS_CAPTURE_SIZE" not in main_cpp):
        errors.append("PFD renderer does not expose deterministic multi-resolution golden mode")
    return errors


def main() -> int:
    data = json.loads(CONTRACT.read_text(encoding="utf-8"))
    errors = validate(data) + validate_repository()
    if errors:
        print("AVIONIX HMI contract FAILED")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("AVIONIX HMI contract PASS: 2 targets, 10 gates, 20 fail-closed controls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
