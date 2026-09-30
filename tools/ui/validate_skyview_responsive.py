#!/usr/bin/env python3
"""Fail-closed validation for the AVIONIX SkyView responsive layout."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / "config/ui/avionix-skyview-responsive-v1.json"
ROOT_KEYS = {
    "schema", "status", "resolutions", "compact_max_width_px",
    "fma_height_logical_px", "tape_vertical_margin_logical_px",
    "weather_right_margin_logical_px", "compact_weather", "regular_weather",
    "minimum_heading_clearance_logical_px", "attribution_visible",
    "no_data_text", "state_labels", "operational_touch_enabled",
    "golden_interactive", "overlay_z", "no_control_authority",
}


def _error(errors: list[str], control: int, message: str) -> None:
    errors.append(f"[HMI-R20-{control:02d}] {message}")


def validate(data: object) -> list[str]:
    errors: list[str] = []
    if not isinstance(data, dict):
        return ["[HMI-R20-01] contract root must be an object"]
    if (set(data) != ROOT_KEYS or
            data.get("schema") != "avionix.skyview-responsive.v1" or
            data.get("status") != "DESIGN_BASELINE_NOT_FLIGHT_APPROVED"):
        _error(errors, 1, "root/schema/status differ from the closed baseline")
    if data.get("resolutions") != [[1024, 600], [1280, 800], [1920, 1080]]:
        _error(errors, 2, "SkyView resolution matrix differs from the frozen order")
    if data.get("compact_max_width_px") != 1100:
        _error(errors, 3, "compact breakpoint must be exactly 1100 px")
    if data.get("fma_height_logical_px") != 58:
        _error(errors, 4, "FMA reserved height must be 58 logical px")
    if data.get("tape_vertical_margin_logical_px") != 12:
        _error(errors, 5, "tape vertical clearance must be 12 logical px")
    if data.get("weather_right_margin_logical_px") != 8:
        _error(errors, 6, "weather/tape clearance must be 8 logical px")

    compact = data.get("compact_weather")
    if not isinstance(compact, dict):
        compact = {}
    if compact.get("width_logical_px") != 154 or compact.get("height_logical_px") != 64:
        _error(errors, 7, "compact weather dimensions must be 154x64")
    if compact.get("top_margin_logical_px") != 62:
        _error(errors, 8, "compact weather must clear the heading ribbon by 62 px")
    if compact.get("badge_height_logical_px") != 44:
        _error(errors, 9, "compact status badge must preserve 44 px")
    if compact.get("legend_visible") is not False:
        _error(errors, 10, "compact legend must be suppressed")
    if compact.get("no_data_integrated") is not True or set(compact) != {
            "width_logical_px", "height_logical_px", "top_margin_logical_px",
            "badge_height_logical_px", "legend_visible", "no_data_integrated"}:
        _error(errors, 11, "compact no-data state must be integrated in a closed object")

    regular = data.get("regular_weather")
    if not isinstance(regular, dict):
        regular = {}
    if regular.get("width_logical_px") != 178 or regular.get("height_logical_px") != 103:
        _error(errors, 12, "regular weather dimensions must be 178x103")
    if regular.get("top_margin_logical_px") != 62:
        _error(errors, 13, "regular weather must clear the heading ribbon by 62 px")
    if regular.get("badge_height_logical_px") != 34:
        _error(errors, 14, "regular status badge must be 34 px")
    if regular.get("legend_visible") is not True:
        _error(errors, 15, "regular legend must remain available")
    if regular.get("no_data_gap_logical_px") != 6 or set(regular) != {
            "width_logical_px", "height_logical_px", "top_margin_logical_px",
            "badge_height_logical_px", "legend_visible", "no_data_gap_logical_px"}:
        _error(errors, 16, "regular no-data banner requires a closed 6 px gap")
    if data.get("minimum_heading_clearance_logical_px") != 4:
        _error(errors, 17, "minimum heading clearance must be 4 logical px")
    if data.get("attribution_visible") is not True or data.get("no_data_text") != "NO DATA • CHECK HOTSPOT":
        _error(errors, 18, "weather attribution/no-data semantics are incomplete")
    if (data.get("state_labels") != ["CURRENT", "CACHED", "STALE", "EXPIRED", "SIM"] or
            data.get("operational_touch_enabled") is not True or
            data.get("golden_interactive") is not False):
        _error(errors, 19, "state labels or interaction policy differ from baseline")
    if data.get("overlay_z") != 3 or data.get("no_control_authority") is not True:
        _error(errors, 20, "overlay priority or no-authority invariant is invalid")
    return errors


def validate_repository(root: Path = ROOT) -> list[str]:
    errors: list[str] = []
    pfd = (root / "qml/AdaptivePfd.qml").read_text(encoding="utf-8")
    weather = (root / "qml/WeatherRadarOverlay.qml").read_text(encoding="utf-8")
    markers = {
        "readonly property bool compactSkyView: root.width <= 1100": "breakpoint",
        "topMargin: 62*root.uiScale": "top clearance",
        "width: (compactSkyView ? 154 : 178)*root.uiScale": "responsive width",
        "height: (compactSkyView ? 64 : 103)*root.uiScale": "responsive height",
        "compactLayout: compactSkyView": "responsive propagation",
        "interactive: !root.goldenMode": "golden interaction lock",
    }
    for marker, label in markers.items():
        if marker not in pfd:
            errors.append(f"AdaptivePfd missing {label}: {marker}")
    weather_markers = {
        "property bool compactLayout: false": "compact property",
        "property bool radarEnabled: true": "non-shadowing radar state",
        "readonly property bool trustedDisplayState:": "fail-closed weather state allowlist",
        "root.weatherProvider.radarState === \"CURRENT\"": "current weather state",
        "root.weatherProvider.radarState === \"STALE\"": "stale weather state",
        "root.weatherProvider.radarState === \"SIMULATED\"": "simulation weather state",
        "radar.status === Image.Ready && root.trustedDisplayState": "fail-closed image availability",
        "!root.available && root.compactLayout ? \"NO DATA • CHECK HOTSPOT\"": "integrated no-data",
        "!root.loading && !root.compactLayout": "separate banner guard",
        "topMargin: 6*root.uiScale": "regular banner gap",
        "!root.compactLayout && root.width > 300*root.uiScale": "legend guard",
        "SUPPLEMENTARY • © RADAR-DPC CC-BY-SA": "attribution",
    }
    for marker, label in weather_markers.items():
        if marker not in weather:
            errors.append(f"WeatherRadarOverlay missing {label}: {marker}")
    return errors


def main() -> int:
    data = json.loads(CONTRACT.read_text(encoding="utf-8"))
    errors = validate(data) + validate_repository()
    if errors:
        print("AVIONIX SkyView responsive contract FAILED")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("AVIONIX SkyView responsive contract PASS: 20 fail-closed controls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
