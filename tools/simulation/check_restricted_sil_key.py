"""Read-only negative probes for the specifically authorized serverha SIL key."""
import json
import os
from pathlib import Path
import subprocess
from tools.simulation.jsbsim_session import write_json


def run(directory):
    if os.name != "posix" or os.uname().nodename != "serverha" or os.getuid() != 1000:
        raise ValueError("pinned serverha user required")
    directory = Path(directory)
    directory.mkdir(parents=False, exist_ok=False)
    common = ["ssh", "-i", "/home/sfuso/.ssh/efis_sil_pfd_ed25519", "-o", "BatchMode=yes",
              "-o", "StrictHostKeyChecking=yes", "-o", "ConnectTimeout=3"]
    cases = []
    for command in ("", "id", "SIL_CHECK_V1; id", "SIL_CHECK_V1 extra", "internal-sftp"):
        cases.append(("command:"+command, ["-T", "efis@192.168.1.45", command], b"", "SIL_ACCESS_DENIED"))
    cases.extend([
        ("PTY", ["-tt", "efis@192.168.1.45", "id"], b"", "PTY allocation request failed"),
        ("TCP_FORWARD", ["-W", "127.0.0.1:22", "efis@192.168.1.45"], b"", "administratively prohibited"),
        ("REMOTE_FORWARD", ["-o", "ExitOnForwardFailure=yes", "-R", "0:127.0.0.1:22", "-N", "efis@192.168.1.45"],
         b"", "remote port forwarding failed")])
    results = []
    for name, arguments, data, expected in cases:
        result = subprocess.run(common+arguments, input=data, capture_output=True, timeout=6)
        stderr = result.stderr.decode(errors="replace")
        results.append({"case": name, "returncode": result.returncode,
                        "pass": result.returncode != 0 and expected in stderr and not result.stdout,
                        "stderr": stderr})
    report = {"status": "PASS" if all(r["pass"] for r in results) else "FAIL",
              "cases": results, "private_key_transferred": False, "configuration_changed": False}
    write_json(directory / "restricted-key-report.json", report)
    return report


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    result = run(args.directory)
    print(json.dumps(result))
    raise SystemExit(0 if result["status"] == "PASS" else 1)
