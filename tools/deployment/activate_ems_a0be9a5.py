"""Pinned EMS bench activation; stdlib only, executable with Python -I.

--check-only is read-only. --activate --bench-window requires root and stops
the EMS display three times. No reboot, network, firmware, or unit edits.
"""
from __future__ import annotations

import argparse
import datetime as dt
try:
    import fcntl
except ImportError:  # orchestration tests may run on Windows; activation cannot
    fcntl = None
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import signal
import socket
import stat
import subprocess
import sys
import tarfile
import time
import uuid

SOURCE = "a0be9a52494ea7ccb5a3890e94d03b671162383b"
STAGED = Path("/home/pi/efis-next-releases/ems-a0be9a5-main-v1")
PREVIOUS = Path("/home/pi/efis-next-releases/ems-62ba90b-hmi-cas-v1")
SEALED_PARENT = Path("/opt/efis-next-ems-releases")
SEALED = SEALED_PARENT / "ems-a0be9a5-main-v1"
CURRENT = Path("/home/pi/efis-next-current")
ARCHIVE = Path("/home/pi/efis-release-ems-a0be9a5-stage/efis-next-a0be9a5-arm64.tar.gz")
VALIDATOR = Path("/usr/local/lib/efis-next/validate_release_manifest.py")
SERVICE = "efis-ems.service"
MANIFEST_SHA = "9bd369facf5a348ec83d9dd6f9eaa388f7dc90f9b839861a62cd67528095e625"
PREVIOUS_MANIFEST_SHA = "cb0378c580e4b76bd81749a36e9dc005974c8a9e523f07a23aa5390cac68e66f"
BINARY_SHA = "8256045bf8707a7c269562799dafdb79f6ab4bee21341946aeafcf0dcfce33f6"
PREVIOUS_BINARY_SHA = "4ebab1d5a7a10387987be49eb4d36554dccfb67e5ccd08d6e461b484b96073f7"
ARCHIVE_SHA = "0ab89831f0887b1729aea5837feb98d329edb252bfe27e3dcad0b913b8334c15"
VALIDATOR_SHA = "cbe02dd613140352ddea334d46de498708110436fc2d0a037aa741a4571fd368"
PROFILE_SHA = "3d67d76a83ee9ea89ab7515097042d3c39aca9f77db4e6d3719bf139a7da7154"
UNIT_HASHES = {
    "/etc/systemd/system/efis-ems.service": "fa766cfca9b3537f042b74ab32b4d3506ee11fd9f94cb449ce49790fef20d638",
    "/etc/systemd/system/efis-ems.service.d/release-v1.conf": "67cc913dd0b9bde6191eb6c70ee529c4e93d51e0212dad7e67eef07e6b41f116",
    "/etc/systemd/system/efis-ems.service.d/performance.conf": "d2f14045c25bcdb2ab77d266c31ccc0737be95f7912bad7ae1eecd39538f415f",
}
ENVIRONMENT = {
    "QT_QPA_PLATFORM": "eglfs", "EFIS_KIOSK": "1", "QSG_RENDER_LOOP": "threaded",
    "EFIS_EDP_BIND": "10.77.10.12", "EFIS_EDP_PORT": "49003", "EFIS_EDP_SOURCE_NODE": "47",
    "EFIS_ENGINE_RUNTIME_STATE": "/home/pi/.local/state/efis-next/ems/engine-runtime.state",
    "EFIS_BLACKBOX_FUEL_EVENTS": "/home/pi/.local/state/efis-next/ems/blackbox-fuel-events.jsonl",
    "EFIS_CAPTURE_DIR": "/home/pi/.local/state/efis-next/ems/captures",
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def read_pinned(path, expected):
    require(not path.is_symlink() and path.is_file(), f"not a regular file: {path}")
    data = path.read_bytes()
    require(hashlib.sha256(data).hexdigest() == expected, f"SHA256 mismatch: {path}")
    return data


def trusted_file(path, expected):
    data = read_pinned(path, expected)
    info = path.stat()
    require(info.st_uid == 0 and not info.st_mode & 0o022, f"not root-protected: {path}")
    return data


def run(args, timeout=20):
    result = subprocess.run(args, text=True, capture_output=True, timeout=timeout, check=False)
    require(result.returncode == 0, f"command failed: {args[0]} {args[1:3]}: {result.stderr.strip()}")
    return result.stdout


def properties():
    text = run(["/usr/bin/systemctl", "show", SERVICE, "-p", "ActiveState", "-p", "MainPID",
                "-p", "NRestarts", "-p", "User", "-p", "DropInPaths", "-p", "FragmentPath"])
    return dict(line.split("=", 1) for line in text.splitlines() if "=" in line)


def config_gate():
    for name, digest in UNIT_HASHES.items():
        trusted_file(Path(name), digest)
    trusted_file(VALIDATOR, VALIDATOR_SHA)
    props = properties()
    require(props.get("User") == "pi", "unexpected service user")
    actual = set(props.get("DropInPaths", "").split()) | {props.get("FragmentPath", "")}
    require(actual == set(UNIT_HASHES), "unexpected unit or extra drop-in")


def power_gate():
    require(run(["/usr/bin/vcgencmd", "get_throttled"]).strip() == "throttled=0x0", "EMS power gate failed")


def validate_release(root, manifest_sha, binary_sha):
    require(root.is_dir() and not root.is_symlink(), f"invalid root: {root}")
    require(root.resolve() == root, f"ancestor symlink refused: {root}")
    manifest = json.loads(read_pinned(root / "release-manifest.json", manifest_sha))
    require(manifest.get("roles") == ["EMS"] and manifest.get("platform") == "RPI4_AARCH64", "wrong role/platform")
    require(manifest.get("flight_approval") is False and manifest.get("simulation_mode") is True, "SIM safety markers changed")
    if manifest_sha == MANIFEST_SHA:
        require(manifest.get("source_commit") == SOURCE, "wrong candidate source")
    read_pinned(root / "bin/efis-ems-qt", binary_sha)
    read_pinned(root / "config/active/aircraft-profile.json", PROFILE_SHA)
    # A separately pinned, root-owned, standalone stdlib validator is the only
    # Python helper executed. Never import code from the writable candidate.
    run(["/usr/bin/python3", "-I", str(VALIDATOR), "--manifest", str(root / "release-manifest.json"),
         "--release-root", str(root), "--expected-role", "EMS", "--expected-platform", "RPI4_AARCH64", "--check-files"])


def runtime_identity(root):
    props = properties()
    require(props.get("ActiveState") == "active", "EMS not active")
    pid = props.get("MainPID", "")
    require(re.fullmatch(r"[1-9][0-9]*", pid), "missing main PID")
    require(Path(f"/proc/{pid}/exe").resolve() == root / "bin/efis-ems-qt", "unexpected running executable")
    require(props.get("NRestarts") == "0", "automatic restart observed")
    # Read only the whitelist, never print or persist the whole environment.
    environment = dict(item.split("=", 1) for item in Path(f"/proc/{pid}/environ").read_bytes().decode().split("\0") if "=" in item)
    require(all(environment.get(k) == v for k, v in ENVIRONMENT.items()), "runtime environment drift")
    require(not environment.get("EFIS_CAPTURE"), "unexpected startup capture mode")
    require(environment.get("EFIS_AIRCRAFT_PROFILE", "config/active/aircraft-profile.json") in
            ("config/active/aircraft-profile.json", str(root / "config/active/aircraft-profile.json")),
            "external aircraft profile requires separate approval")
    return pid


def preflight():
    require(socket.gethostname() == "efis" and platform.machine() == "aarch64", "wrong EMS host/architecture")
    require(Path("/proc/device-tree/model").read_text().startswith("Raspberry Pi 4 "), "wrong physical model")
    require(CURRENT.is_symlink() and CURRENT.resolve() == PREVIOUS, "current is not the pinned previous release")
    require(not SEALED.exists() and not SEALED.is_symlink(), "sealed target already exists; operator review required")
    require(shutil.disk_usage("/opt").free >= 8 * 1024 ** 3, "less than 8 GiB free")
    if SEALED_PARENT.exists():
        info = SEALED_PARENT.stat()
        require(not SEALED_PARENT.is_symlink() and info.st_uid == 0 and not info.st_mode & 0o022, "unsafe sealed parent")
    config_gate()
    power_gate()
    validate_release(PREVIOUS, PREVIOUS_MANIFEST_SHA, PREVIOUS_BINARY_SHA)
    validate_release(STAGED, MANIFEST_SHA, BINARY_SHA)
    with tarfile.open(fileobj=io.BytesIO(read_pinned(ARCHIVE, ARCHIVE_SHA)), mode="r:gz") as archive:
        validate_archive_members(archive)
    pid = runtime_identity(PREVIOUS)
    sockets = run(["/usr/bin/ss", "-lunp"])
    require(re.search(r'10\.77\.10\.12:49003\s.*pid=' + pid + r'[,)]', sockets), "EDP listener missing or wrong owner")
    for service in ["efis.service", "lightdm.service", "efis-next-mfd-direct.service"]:
        result = subprocess.run(["/usr/bin/systemctl", "is-active", "--quiet", service], timeout=8, check=False)
        require(result.returncode != 0, f"conflicting display service: {service}")
    return {"preflight": "PASS", "source_commit": SOURCE, "current": str(PREVIOUS),
            "candidate": str(STAGED), "activation_executed": False, "flight_approval": False}


def validate_archive_members(archive):
    members = archive.getmembers()
    for member in members:
        path = PurePosixPath(member.name)
        require(not path.is_absolute() and ".." not in path.parts and path.parts[:1] == ("efis-next",), "unsafe archive path")
        require(member.isdir() or member.isfile(), "archive links/special entries refused")
    return members


def safe_extract(archive_bytes, destination):
    """Extract only ordinary git archive members with the expected prefix."""
    with tarfile.open(fileobj=io.BytesIO(archive_bytes), mode="r:gz") as archive:
        members = validate_archive_members(archive)
        for member in members:
            relative = PurePosixPath(member.name).parts[1:]
            if not relative:
                continue
            target = destination.joinpath(*relative)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open("xb") as output:
                    source = archive.extractfile(member)
                    require(source is not None, "missing archive member")
                    with source:
                        shutil.copyfileobj(source, output)
                    output.flush()
                    os.fsync(output.fileno())
                target.chmod(0o555 if member.mode & 0o111 else 0o444)


class Operations:
    def __init__(self):
        self.evidence = None
        self.expected = PREVIOUS
        self.cursor = None

    def prepare(self):
        # Everything that may execute was read once and verified before use.
        archive = read_pinned(ARCHIVE, ARCHIVE_SHA)
        binary = read_pinned(STAGED / "bin/efis-ems-qt", BINARY_SHA)
        manifest = read_pinned(STAGED / "release-manifest.json", MANIFEST_SHA)
        evidence_parent = Path("/var/lib/efis-next/activation-evidence")
        evidence_parent.mkdir(parents=True, exist_ok=True)
        require(evidence_parent.resolve() == evidence_parent, "evidence ancestor symlink")
        require(evidence_parent.stat().st_uid == 0 and not evidence_parent.stat().st_mode & 0o022, "unsafe evidence parent")
        self.evidence = evidence_parent / ("ems-a0be9a5-" + dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ"))
        self.evidence.mkdir(mode=0o700)
        print("EVIDENCE=" + str(self.evidence), flush=True)
        self.record("preflight.json", preflight())
        state = Path(ENVIRONMENT["EFIS_ENGINE_RUNTIME_STATE"])
        if state.is_file():
            require(not state.is_symlink() and state.stat().st_size < 1024 ** 2, "unsafe runtime snapshot")
            (self.evidence / "engine-runtime-before.state").write_bytes(state.read_bytes())
        SEALED_PARENT.mkdir(mode=0o755, exist_ok=True)
        SEALED.mkdir(mode=0o700)
        print("PHASE=SEALING_VERIFIED_SOURCE", flush=True)
        safe_extract(archive, SEALED)
        (SEALED / "bin").mkdir(exist_ok=True)
        (SEALED / "bin/efis-ems-qt").write_bytes(binary)
        (SEALED / "bin/efis-ems-qt").chmod(0o555)
        (SEALED / "release-manifest.json").write_bytes(manifest)
        (SEALED / "SOURCE_COMMIT").write_text(SOURCE + "\n")
        for item in SEALED.rglob("*"):
            require(not item.is_symlink(), "sealed payload symlink")
            if item.is_dir():
                item.chmod(0o555)
            elif not item.stat().st_mode & 0o111:
                item.chmod(0o444)
        SEALED.chmod(0o555)
        for item in [p for p in SEALED.rglob("*") if p.is_file()]:
            with item.open("rb") as file:
                os.fsync(file.fileno())
        for item in [p for p in SEALED.rglob("*") if p.is_dir()] + [SEALED, SEALED_PARENT]:
            directory = os.open(item, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
        validate_release(SEALED, MANIFEST_SHA, BINARY_SHA)
        (self.evidence / "unit-before.txt").write_text(run(["/usr/bin/systemctl", "cat", SERVICE]))

    def record(self, filename, data):
        if self.evidence is not None:
            (self.evidence / filename).write_text(json.dumps(data, indent=2) + "\n")

    def ready(self, target):
        power_gate()
        config_gate()
        require(CURRENT.resolve() == self.expected, "current changed during activation")
        validate_release(target, MANIFEST_SHA if target == SEALED else PREVIOUS_MANIFEST_SHA,
                         BINARY_SHA if target == SEALED else PREVIOUS_BINARY_SHA)
        text = run(["/usr/bin/journalctl", "-u", SERVICE, "-n", "1", "--show-cursor", "--no-pager", "-o", "cat"])
        match = re.search(r"^-- cursor: (.+)$", text, re.MULTILINE)
        require(match is not None, "journal cursor unavailable")
        self.cursor = match.group(1)

    def change(self, target):
        # Recovery is armed by transaction() immediately before this first
        # mutating service command, never during preflight or sealing.
        run(["/usr/bin/systemctl", "stop", SERVICE])
        self.point(target)
        run(["/usr/bin/systemctl", "start", SERVICE])
        self.expected = target

    def point(self, target):
        require(target in (PREVIOUS, SEALED), "target outside pinned release set")
        require(CURRENT.is_symlink() and CURRENT.resolve() in (PREVIOUS, SEALED), "refusing to replace unexpected current")
        temporary = CURRENT.parent / (".ems-activation-" + str(os.getpid()) + "-" + uuid.uuid4().hex)
        require(not temporary.exists() and not temporary.is_symlink(), "switch path exists")
        temporary.symlink_to(target)
        try:
            os.replace(temporary, CURRENT)
        finally:
            if temporary.is_symlink():
                temporary.unlink()
        directory = os.open(CURRENT.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)

    def verify(self, target):
        # Observe a stable process and fresh startup/link/render events.
        time.sleep(12)
        pid = runtime_identity(target)
        power_gate()
        config_gate()
        sockets = run(["/usr/bin/ss", "-lunp"])
        require(re.search(r'10\.77\.10\.12:49003\s.*pid=' + pid + r'[,)]', sockets), "EDP listener does not belong to EMS")
        journal = run(["/usr/bin/journalctl", "-u", SERVICE, "--after-cursor", self.cursor, "--no-pager", "-o", "cat"])
        require("event=startup product=efis-next component=ems" in journal, "missing fresh startup")
        require(re.search(r'event=ems_edp_link_transition .*engine_state= LIVE .*accepted= [1-9][0-9]* .*node= BLACKBOX 47', journal), "no fresh EDP link acceptance")
        rates = re.findall(r'event=render_health node=ems presentation_hz=\s*([0-9.]+)', journal)
        require(rates and all(float(rate) > 0 for rate in rates), "no positive renderer heartbeat")
        require(not re.search(r'engine_runtime_(save_failed|rejected)|segmentation fault|core dumped|failed to create display', journal, re.IGNORECASE), "runtime/render fault")
        suffix = "previous" if target == PREVIOUS else "candidate"
        (self.evidence / (suffix + "-journal-" + str(pid) + ".txt")).write_text(journal)
        self.record(suffix + "-runtime.json", {"pid": pid, "executable": str(target / "bin/efis-ems-qt"), "power": "0x0"})

    def recover(self):
        # No power gate on recovery: a candidate failure must not prevent
        # restoration. Never change runtime data, unit files or network.
        try:
            run(["/usr/bin/systemctl", "stop", SERVICE])
        except Exception as error:
            self.record("recovery-stop-error.json", {"error": str(error)})
        self.point(PREVIOUS)
        run(["/usr/bin/systemctl", "start", SERVICE])
        time.sleep(8)
        runtime_identity(PREVIOUS)
        self.record("recovery.json", {"status": "PASS", "current": str(PREVIOUS)})
        print("RECOVERY=PASS", flush=True)

    def complete(self):
        self.record("result.json", {"status": "PASS", "current": str(SEALED), "rollback_drill": "PASS",
                                  "reboot_acceptance": "NOT_RUN", "visual_acceptance": "NOT_RUN", "flight_approval": False})


def transaction(operations):
    """Injectable orchestration; no test mode or alternate paths in the CLI."""
    armed = False
    try:
        operations.prepare()
        for target in (SEALED, PREVIOUS, SEALED):
            operations.ready(target)
            print("PHASE=" + ("ROLLBACK_DRILL" if target == PREVIOUS else "CANDIDATE_START"), flush=True)
            armed = True
            operations.change(target)
            operations.verify(target)
        operations.complete()
    except BaseException as failure:
        if armed:
            try:
                operations.recover()
            except BaseException as recovery_failure:
                raise RuntimeError(f"ACTIVATION_FAILED_AND_RECOVERY_FAILED: {recovery_failure}") from failure
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--check-only", action="store_true")
    group.add_argument("--activate", action="store_true")
    parser.add_argument("--bench-window", action="store_true")
    args = parser.parse_args()
    try:
        require(args.check_only or (args.bench_window and fcntl is not None and os.geteuid() == 0), "activation requires Linux sudo and --bench-window")
        print(json.dumps(preflight()), flush=True)
        if args.check_only:
            return 0
        # Lock after readonly checks, then repeat them under lock. O_NOFOLLOW
        # refuses attacker-controlled lock symlinks; owner must be root.
        lock = os.open("/run/lock/efis-ems-a0be9a5.lock", os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
        info = os.fstat(lock)
        require(stat.S_ISREG(info.st_mode) and info.st_uid == 0 and not info.st_mode & 0o022, "unsafe activation lock")
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            preflight()
            signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(InterruptedError("SIGTERM")))
            operations = Operations()
            transaction(operations)
            print("EMS_A0BE9A5_ACTIVATION=PASS\nROLLBACK_DRILL=PASS\nNEXT_REQUIRED=REBOOT_AND_VISUAL_ACCEPTANCE", flush=True)
            print("EVIDENCE=" + str(operations.evidence), flush=True)
        finally:
            os.close(lock)
        return 0
    except BaseException as error:
        print("EMS_A0BE9A5_ACTIVATION=FAIL: " + str(error), file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
