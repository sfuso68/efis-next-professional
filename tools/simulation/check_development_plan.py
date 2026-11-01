"""Lint the V1 planning baseline, never attest simulator or flight readiness.

This checker intentionally refuses completion or authority changes. A future
acceptance registry needs its own evidence intake, not a green planning lint.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "config/simulation/development-plan-v1.json"
DOMAINS = set("PFD MFD_CANVAS EMS NAV SVS_TAWS WEATHER BLACKBOX SETUP_PROFILES "
              "EMERGENCY VOICE_RADIO FPV ACCESSORIES HEALTH_MAINTENANCE STANDARD_PROFESSIONAL".split())
SCENARIOS = set("MISSION FLIGHT_WIND ENGINE_OUT ENGINE_THERMAL ELECTRICAL FUEL SENSORS "
                "RADIO_NAV WEATHER_PRODUCTS TERRAIN_GAPS AIRFRAME TRANSPORT_RESTART "
                "RECORD_RESTORE EMERGENCY_VOICE FOUR_VIDEO SETUP_SKU_ISOLATION".split())


def exact(obj, keys):
    if not isinstance(obj, dict) or set(obj) != set(keys.split()):
        raise ValueError("planning object fields mismatch")


def validate(plan):
    exact(plan, "schema specification parent_requirement flight_approval deployment_authorized "
          "completion model_validation hardware_acceptance packages coverage scenario_ids open_gates")
    if (plan["schema"] != "fuso-avionics.sil-development-plan.v1"
            or plan["specification"] != "docs/development/SIMULATOR_MASTER_SPEC_V1.md"
            or plan["parent_requirement"] != "AVX-G5-02"
            or plan["flight_approval"] is not False or plan["deployment_authorized"] is not False
            or plan["completion"] != "NOT_COMPLETE" or plan["model_validation"] != "OPEN"
            or plan["hardware_acceptance"] != "OPEN"):
        raise ValueError("planning baseline cannot grant readiness or authority")
    packages = plan["packages"]
    if not isinstance(packages, list) or len(packages) != 12:
        raise ValueError("twelve development packages required")
    seen = set()
    for index, item in enumerate(packages, 1):
        exact(item, "id title state depends_on exit")
        if item["id"] != f"P{index:02d}" or item["state"] not in ("TODO", "PARTIAL"):
            raise ValueError("invalid package identity or unsupported completion claim")
        if any(not isinstance(item[k], str) or not item[k].strip() for k in ("title", "exit")):
            raise ValueError("package needs title and exit criterion")
        deps = item["depends_on"]
        if (not isinstance(deps, list) or any(not isinstance(d, str) for d in deps)
                or len(deps) != len(set(deps)) or not set(deps) <= seen
                or (index > 1 and not deps)):
            raise ValueError("dependency must name earlier unique packages")
        seen.add(item["id"])
    coverage = plan["coverage"]
    if not isinstance(coverage, dict) or set(coverage) != DOMAINS:
        raise ValueError("module coverage incomplete")
    for owners in coverage.values():
        if (not isinstance(owners, list) or any(not isinstance(x, str) for x in owners)
                or len(owners) != len(set(owners)) or not set(owners) <= seen
                or "P12" not in owners or len(owners) < 2):
            raise ValueError("module requires implementation and integrated acceptance")
    scenarios = plan["scenario_ids"]
    if (not isinstance(scenarios, list) or any(not isinstance(x, str) for x in scenarios)
            or len(scenarios) != len(SCENARIOS) or set(scenarios) != SCENARIOS):
        raise ValueError("required scenario coverage incomplete")
    gates = plan["open_gates"]
    expected = set("INDEPENDENT_PRODUCER COMPLETE_CONSUMERS MODEL_REFERENCES PFD_TOUCH "
                   "MFD_BLACKBOX_POWER GOLDEN_RELEASE SIL_NAS_RESTORE ENDURANCE".split())
    if (not isinstance(gates, list) or any(not isinstance(x, str) for x in gates)
            or len(gates) != len(expected) or set(gates) != expected):
        raise ValueError("planning baseline cannot close unresolved gates")
    return {"planning_structure": "PASS", "simulator_acceptance": "NOT_COMPLETE",
            "flight_approval": False, "packages": len(packages), "domains": len(coverage),
            "required_scenarios": len(scenarios), "next_package": "P01"}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate planning key")
        result[key] = value
    return result


def load(path=PLAN):
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    try:
        print(json.dumps(validate(load()), sort_keys=True))
    except (ValueError, OSError, TypeError) as exc:
        parser.exit(2, f"SIL planning check FAILED: {exc}\n")


if __name__ == "__main__":
    main()
