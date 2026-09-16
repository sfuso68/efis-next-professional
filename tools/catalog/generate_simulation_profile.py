#!/usr/bin/env python3
"""Generate the complete, explicitly synthetic EFIS operational profile.

The output exists only to exercise the simulator and UI.  Every dictionary
field is materialized as a simulated, indicative VALUE so that no generated
number can be mistaken for approved aircraft data.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


SIMULATION_NOTE = "Synthetic/analogue value for software simulation only; not applicable to flight."


def definitions(dictionary: dict) -> list[dict]:
    result: list[dict] = []
    for domain in dictionary["domains"]:
        for parameter in domain["parameters"]:
            item = dict(domain.get("defaults", {}))
            item.update(parameter)
            item["key"] = f"{domain['id']}.{parameter['id']}"
            result.append(item)
    return result


def procedure(name: str) -> dict:
    return {
        "mode": "SIMULATION",
        "title": name,
        "steps": ["SIMULATED PROCEDURE — NOT FOR FLIGHT"],
    }


OVERRIDES: dict[str, object] = {
    "identity.manufacturer": "Vidor",
    "identity.model": "Asso V Jolly",
    "identity.variant": "Pioneer 300 analogue — simulation only",
    "identity.construction_number": "SIM-0001",
    "identity.registration": "I-SIM",
    "identity.category": "EXPERIMENTAL",
    "identity.icao_type": "ZZZZ",
    "identity.construction_year": 2026,
    "identity.seats": 2,
    "identity.build_standard": "Synthetic software test configuration V1",
    "identity.configuration_id": "SIM-ASSO5J-912UL-V1",
    "identity.base_airport": "LIPH Treviso",
    "identity.owner_operator": "EFIS NEXT simulator",
    "identity.external_markings": "SIMULATION ONLY",
    "regulatory.state": "Italy — simulated",
    "regulatory.authority": "No authority; synthetic dataset",
    "regulatory.certificate_type": "SIMULATION ONLY",
    "regulatory.certificate_number": "SIM-NOT-A-CERTIFICATE",
    "regulatory.approval_basis": "No approval basis; software test data",
    "regulatory.operating_categories": {"day_vfr": True, "simulation_only": True},
    "regulatory.day_vfr_allowed": True,
    "regulatory.night_vfr_allowed": False,
    "regulatory.ifr_allowed": False,
    "regulatory.aerobatic_allowed": False,
    "regulatory.minimum_crew": 1,
    "regulatory.maximum_occupants": 2,
    "geometry.length": 6.15,
    "geometry.height": 2.2,
    "geometry.wingspan": 8.1,
    "geometry.wing_area": 10.5,
    "geometry.mean_aerodynamic_chord": 1.3,
    "geometry.aspect_ratio": 6.25,
    "geometry.airfoil": "Synthetic light-aircraft analogue",
    "geometry.wing_incidence": 1.5,
    "geometry.wing_dihedral": 3.0,
    "geometry.wheelbase": 1.55,
    "geometry.wheel_track": 1.65,
    "geometry.propeller_ground_clearance": 0.25,
    "geometry.reference_datum": "Synthetic firewall datum",
    "geometry.leveling_reference": "Synthetic cabin-sill reference",
    "mass_balance.empty_mass": 300.0,
    "mass_balance.basic_empty_mass": 310.0,
    "mass_balance.minimum_flight_mass": 350.0,
    "mass_balance.maximum_takeoff_mass": 472.5,
    "mass_balance.maximum_landing_mass": 472.5,
    "mass_balance.maximum_zero_fuel_mass": 450.0,
    "mass_balance.maximum_baggage_mass": 20.0,
    "mass_balance.minimum_seat_mass": 55.0,
    "mass_balance.maximum_seat_mass": 100.0,
    "mass_balance.fuel_density_reference": 0.72,
    "mass_balance.empty_cg": 300.0,
    "mass_balance.weighing_date": "2026-09-09",
    "mass_balance.weighing_report_reference": "SIMULATED — no weighing report",
    "structural_limits.positive_load_factor_clean": 4.0,
    "structural_limits.negative_load_factor_clean": -2.0,
    "structural_limits.positive_load_factor_flaps": 2.0,
    "structural_limits.negative_load_factor_flaps": 0.0,
    "structural_limits.maximum_operating_altitude": 12000.0,
    "structural_limits.maximum_demonstrated_crosswind": 15.0,
    "structural_limits.maximum_tailwind": 5.0,
    "structural_limits.maximum_bank_normal": 60.0,
    "speeds.vso": 41.0,
    "speeds.vs1": 45.0,
    "speeds.vx": 58.0,
    "speeds.vy": 65.0,
    "speeds.va_by_mass": [{"mass_kg": 472.5, "speed_kt": 100.0, "mode": "SIMULATION"}],
    "speeds.vno": 110.0,
    "speeds.vne": 145.0,
    "speeds.vra": 100.0,
    "speeds.vle": 90.0,
    "speeds.vlo_extend": 90.0,
    "speeds.vlo_retract": 90.0,
    "speeds.best_glide": 65.0,
    "speeds.minimum_sink": 55.0,
    "speeds.vref": 55.0,
    "speeds.rotation_speed": 50.0,
    "speeds.climb_speed": 65.0,
    "speeds.cruise_speed": 135.0,
    "speeds.maximum_door_window_open_speed": 90.0,
    "performance.service_ceiling": 12000.0,
    "performance.absolute_ceiling": 14000.0,
    "performance.glide_ratio": 11.0,
    "engine.manufacturer": "Rotax",
    "engine.model": "912 UL",
    "engine.variant": "UL — synthetic installation",
    "engine.serial_number": "SIM-912-0001",
    "engine.cylinders": 4,
    "engine.displacement": 1211.2,
    "engine.compression_ratio": 9.0,
    "engine.rated_power": 80.0,
    "engine.takeoff_power": 80.0,
    "engine.takeoff_rpm": 5800.0,
    "engine.takeoff_time_limit": 300.0,
    "engine.maximum_continuous_power": 78.9,
    "engine.maximum_continuous_rpm": 5500.0,
    "engine.tbo": 2000.0,
    "engine.calendar_tbo": 180.0,
    "fluids.oil_capacity_min": 2.5,
    "fluids.oil_capacity_max": 3.0,
    "fluids.maximum_oil_consumption": 0.06,
    "fluids.coolant_capacity": 1.5,
    "fluids.brake_fluid": "Simulation brake fluid",
    "propeller.manufacturer": "Simulation propeller",
    "propeller.model": "Three-blade analogue",
    "propeller.serial_number": "SIM-PROP-0001",
    "propeller.blade_count": 3,
    "propeller.diameter": 1.72,
    "propeller.pitch_control_type": "GROUND_ADJUSTABLE",
    "propeller.reverse_available": False,
    "propeller.feather_available": False,
    "propeller.inertia": 0.3,
    "fuel_system.total_capacity": 60.0,
    "fuel_system.total_usable": 58.0,
    "fuel_system.total_unusable": 2.0,
    "electrical.nominal_voltage": 12.0,
    "electrical.alternator_current_limit": 40.0,
    "electrical.essential_endurance": 30.0,
    "flaps.installed": True,
    "flaps.actuation_type": "ELECTRIC",
    "flaps.detents": [0, 10, 15, 20, 30],
    "landing_gear.type": "RETRACTABLE_TRICYCLE",
    "landing_gear.actuation_type": "ELECTRIC",
    "landing_gear.extension_time": 7.0,
    "landing_gear.retraction_time": 7.0,
    "autopilot.installed": True,
    "autopilot.controller_identity": {"controller": "Pixhawk 6X", "firmware": "ArduPlane 4.7.0", "simulation_only": True},
    "operating_units.airspeed": "kt",
    "operating_units.altitude": "ft",
    "operating_units.vertical_speed": "ft/min",
    "operating_units.pressure": "hPa",
    "operating_units.temperature": "degC",
    "operating_units.fuel_volume": "L",
    "operating_units.fuel_flow": "L/h",
    "operating_units.mass": "kg",
    "operating_units.distance": "NM",
    "operating_units.engine_pressure": "bar",
    "operating_units.language": "it-IT",
    "operating_units.time_format": "LOCAL_24H",
    "data_governance.owner_approval": {"approved": False, "reason": "SIMULATION ONLY"},
    "data_governance.technical_review": {"verified": False, "reason": "SIMULATION ONLY"},
    "data_governance.release_record": {"released": False, "reason": "SIMULATION ONLY"},
    "data_governance.unmapped_source_fields": [],
}


def generic_value(definition: dict) -> object:
    key = definition["key"]
    kind = definition["type"]
    unit = definition.get("unit")
    if key.startswith("normal_procedures.") or key.startswith("emergency_procedures."):
        return procedure(definition["label"])
    if kind == "TEXT":
        return f"SIMULATED — {definition['label']}"
    if kind == "INTEGER":
        return 1
    if kind == "BOOLEAN":
        return False
    if kind == "ENUM":
        return definition["enum"][0]
    if kind == "DATE":
        return "2026-09-09"
    if kind == "NUMBER":
        return {
            "kt": 90.0, "kg": 20.0, "kg/L": 0.72, "kg*m2": 0.3,
            "m": 1.0, "m2": 10.5, "mm": 300.0, "ft": 10000.0,
            "deg": 0.0, "g": 1.0, "1": 1.0, "rpm": 5000.0,
            "hp": 80.0, "cm3": 1211.2, "L": 30.0, "L/h": 16.0,
            "V": 14.2, "A": 20.0,
        }.get(unit, 1.0)
    if kind == "DURATION":
        return {"h": 2000.0, "month": 12.0, "min": 30.0, "s": 5.0}.get(unit, 1.0)
    if kind == "CURVE":
        return [{"x": 0, "y": 0}, {"x": 1, "y": 1, "mode": "SIMULATION"}]
    if kind == "TABLE":
        return [{"mode": "SIMULATION", "parameter": key, "note": "Synthetic placeholder — not for flight"}]
    return {"mode": "SIMULATION", "parameter": key, "status": "INDICATIVE"}


def build_profile(dictionary: dict) -> dict:
    values: dict[str, dict] = {}
    for definition in definitions(dictionary):
        key = definition["key"]
        entry = {
            "disposition": "VALUE",
            "value": OVERRIDES.get(key, generic_value(definition)),
            "simulated": True,
            "approval_status": "INDICATIVE",
            "source": "simulation-analogue-2026-09-09",
            "source_document": "EFIS NEXT Simulation Dataset V1",
            "source_revision": "1",
            "source_page": "GENERATED",
            "rationale": SIMULATION_NOTE,
        }
        if definition.get("unit"):
            entry["unit"] = definition["unit"]
        values[key] = entry
    return {
        "schema": "efis-next.operational-profile.v2",
        "profile_key": "simulation.asso-v-jolly-912-ul",
        "simulation_only": True,
        "parent_profile": "model.asso-v-jolly-912-ul",
        "scope_type": "INSTALLED_AIRCRAFT",
        "scope_ref": "SIMULATION-ONLY|ASSO-V-JOLLY-912-UL",
        "title": "SIMULATION ONLY — Asso V Jolly / Rotax 912 UL analogue scenario",
        "revision": 1,
        "lifecycle_state": "SIMULATION",
        "created_at": "2026-09-09T00:00:00Z",
        "updated_at": "2026-09-09T00:00:00Z",
        "default_disposition": "UNKNOWN",
        "default_approval_status": "INDICATIVE",
        "notes": "Complete synthetic profile for simulator/UI testing. Never valid for flight or aircraft configuration.",
        "values": values,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dictionary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    dictionary = json.loads(args.dictionary.read_text(encoding="utf-8"))
    profile = build_profile(dictionary)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(profile, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), "fields": len(profile["values"]), "simulated": True}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
