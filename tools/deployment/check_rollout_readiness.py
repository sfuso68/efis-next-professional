"""Read-only, pinned preflight for the a0be9a5 bench candidates.

No activation capability: a passing report is never an authorization.
Run as a module from the checker checkout, not from an immutable release.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import socket
import subprocess

from tools.deployment.validate_release_manifest import validate

SOURCE = "a0be9a52494ea7ccb5a3890e94d03b671162383b"
NODES = {
    "MFD": {
        "host": "pi5", "model": "Raspberry Pi 5", "platform": "RPI5_AARCH64",
        "current": "/home/pi/efis-next-current",
        "previous": "/home/pi/efis-next-release-mfd-3432402-v1",
        "candidate": "/home/pi/efis-next-release-mfd-a0be9a5-v1",
        "digest": "252c6f374da0a1cf5feba3477fc7419d65000ba8480ffff0cce149775c385eaa",
        "previous_digest": "4a259e2f5c23fc3d876f3c08181fad67bbec69b00aad403c782232cf1956a1fe",
        "roles": ["MFD"], "services": ["efis-next-mfd-direct.service"],
    },
    "PFD": {
        "host": "efis-pfd-01", "model": "Raspberry Pi 5", "platform": "RPI5_AARCH64",
        "current": "/home/efis/efis-next-current",
        "previous": "/home/efis/efis-next-releases/pfdrec-7051a47-retention-v1",
        "candidate": "/home/efis/efis-release-pfdrec-a0be9a5-stage/pfdrec-a0be9a5-main-v1",
        "digest": "8e3d3a9a929f27261eb70805bf437d4a0588890c16b3ca142ba3b8cbfb64ef26",
        "previous_digest": "92fdb5178bd91a7fd7abfc8909e892bfaeaa9c122243fc27f596afc87b48f644",
        "roles": ["PFD", "RECORDER"], "services": ["efis-pfd.service", "efis-recorder.service"],
    },
    "EMS": {
        "host": "efis", "model": "Raspberry Pi 4", "platform": "RPI4_AARCH64",
        "current": "/home/pi/efis-next-current",
        "previous": "/home/pi/efis-next-releases/ems-62ba90b-hmi-cas-v1",
        "candidate": "/home/pi/efis-next-releases/ems-a0be9a5-main-v1",
        "digest": "9bd369facf5a348ec83d9dd6f9eaa388f7dc90f9b839861a62cd67528095e625",
        "previous_digest": "cb0378c580e4b76bd81749a36e9dc005974c8a9e523f07a23aa5390cac68e66f",
        "roles": ["EMS"], "services": ["efis-ems.service"],
    },
    "BLACKBOX": {
        "host": "efisblackbox", "model": "Raspberry Pi 5", "platform": "RPI5_AARCH64",
        "current": "/opt/efis-blackbox-current",
        "previous": "/opt/efis-blackbox-releases/blackbox-7655799-v1",
        "candidate": "/home/pi/efis-release-blackbox-a0be9a5-stage/blackbox-a0be9a5-main-v1",
        "digest": "37c96d54c4e884161fd25c71350af71f6a69aca73e3f5af1217bdbf1ede71cc4",
        "previous_digest": "7ada1c350f69a426876b8163c8b817d01795f122f85f14034445c362c9f71d05",
        "roles": ["BLACKBOX"], "services": ["efis-blackbox-broker.service"],
    },
}


def command(args: list[str]) -> str | None:
    """Allowlisted callers supply argv, never shell text; bounded execution."""
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=8, check=False)
        return result.stdout.strip() if result.returncode == 0 else None
    except (OSError, subprocess.SubprocessError):
        return None


def check_release(root: Path, spec: dict, *, candidate: bool) -> dict:
    try:
        if root.is_symlink() or not root.is_dir():
            raise ValueError("release root must be a regular directory")
        manifest_path = root / "release-manifest.json"
        if manifest_path.is_symlink():
            raise ValueError("manifest symlink refused")
        raw = manifest_path.read_bytes()
        manifest = json.loads(raw)
        # The existing CLI accepts a single --expected-role. Check the complete
        # set here: a PFD+RECORDER report must never check only its last role.
        validate(manifest, root=root, check_files=True)
        if set(manifest["roles"]) != set(spec["roles"]):
            raise ValueError("role set mismatch")
        if manifest["platform"] != spec["platform"]:
            raise ValueError("platform mismatch")
        digest = hashlib.sha256(raw).hexdigest()
        if candidate:
            if digest != spec["digest"] or manifest["source_commit"] != SOURCE:
                raise ValueError("candidate manifest or source changed")
            if manifest["flight_approval"] is not False or manifest["simulation_mode"] is not True:
                raise ValueError("candidate safety markers changed")
        elif digest != spec["previous_digest"]:
            raise ValueError("previous manifest changed")
        return {"pass": True, "manifest_sha256": digest, "release_id": manifest["release_id"]}
    except (OSError, ValueError, KeyError, TypeError) as error:
        return {"pass": False, "reason": str(error)}


def assess(node: str, observed: dict) -> dict:
    spec = NODES[node]
    services = observed.get("services", {})
    binary = "efis-blackbox-broker" if node == "BLACKBOX" else "efis-ems-qt" if node == "EMS" else "efis-pfd-qt"
    checks = {
        "hostname": observed.get("host") == spec["host"],
        "architecture": observed.get("arch") == "aarch64",
        "physical_model": str(observed.get("model", "")).startswith(spec["model"] + " "),
        "current_is_previous": observed.get("current") == spec["previous"],
        "power_history_clear": observed.get("throttled") == "throttled=0x0",
        "candidate_verified": observed.get("candidate", {}).get("pass") is True,
        "previous_verified": observed.get("previous", {}).get("pass") is True,
        "services_active": all(services.get(s, {}).get("ActiveState") == "active"
                               and isinstance(services.get(s, {}).get("MainPID"), str)
                               and re.fullmatch(r"[1-9][0-9]*", services.get(s, {}).get("MainPID", ""))
                               for s in spec["services"]),
        "process_paths_current": all(
            services.get(s, {}).get("executable") == spec["previous"] + "/bin/" +
            ("efis-recorder" if s == "efis-recorder.service" else binary)
            for s in spec["services"]),
        "noninteractive_privilege": observed.get("sudo_available") is True,
    }
    blocked = [key for key, passed in checks.items() if not passed]
    return {
        "schema": "efis-next.activation-preflight.v1", "node": node, "source_commit": SOURCE,
        "timestamp_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "status": "BLOCKED" if blocked else "AUTOMATIC_PREFLIGHT_PASS",
        "checks": checks, "blocking_checks": blocked, "observed": observed,
        "activation_authorized": False, "flight_approval": False,
        "remaining_manual_gates": ["qualified power evidence", "authorized bench window",
                                   "pinned transactional activator and rollback", "reboot and visual acceptance"]
                                  + (["MFD canary accepted"] if node == "PFD" else []),
    }


def process_executable(pid: str) -> str | None:
    if not re.fullmatch(r"[1-9][0-9]*", pid):
        return None
    try:
        return os.readlink(f"/proc/{pid}/exe")
    except PermissionError:
        # Some broker processes have a different owner. Read just this link;
        # never run a service action or dump the process environment as root.
        return command(["sudo", "-n", "readlink", "-f", f"/proc/{pid}/exe"])
    except OSError:
        return None


def collect(node: str) -> dict:
    spec = NODES[node]
    try:
        model = Path("/proc/device-tree/model").read_text().rstrip("\0\n")
    except OSError:
        model = "UNAVAILABLE"
    services = {}
    for service in spec["services"]:
        args = ["systemctl"] + (["--user"] if node == "PFD" else [])
        result = command(args + ["show", service, "-p", "ActiveState", "-p", "MainPID", "-p", "NRestarts"])
        services[service] = dict(line.split("=", 1) for line in (result or "").splitlines() if "=" in line)
        pid = services[service].get("MainPID", "")
        services[service]["executable"] = process_executable(pid)
    return {
        "host": socket.gethostname(), "arch": platform.machine(), "model": model,
        "current": str(Path(spec["current"]).resolve()),
        "throttled": command(["vcgencmd", "get_throttled"]),
        "temperature": command(["vcgencmd", "measure_temp"]),
        "services": services,
        "sudo_available": (hasattr(os, "geteuid") and os.geteuid() == 0)
                          or command(["sudo", "-n", "true"]) is not None,
        "candidate": check_release(Path(spec["candidate"]), spec, candidate=True),
        "previous": check_release(Path(spec["previous"]), spec, candidate=False),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", choices=sorted(NODES), required=True)
    args = parser.parse_args()
    report = assess(args.node, collect(args.node))
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if not report["blocking_checks"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
