"""Pinned MFD bench activation; stdlib only, executable with Python -I.

--check-only is read-only. --activate --bench-window requires root and stops
the MFD display three times. No reboot, network or firmware edits. Canonical-path drop-in restored on failure.
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
STAGED = Path("/home/pi/efis-next-release-mfd-a0be9a5-v1")
PREVIOUS = Path("/home/pi/efis-next-release-mfd-3432402-v1")
SEALED_PARENT = Path("/opt/efis-next-mfd-releases")
SEALED = SEALED_PARENT / "mfd-a0be9a5-main-v1"
CURRENT = Path("/home/pi/efis-next-current")
ARCHIVE = Path("/home/pi/efis-release-mfd-a0be9a5-stage/efis-next-a0be9a5-arm64.tar.gz")
VALIDATOR = Path("/usr/local/lib/efis-next/validate_release_manifest.py")
SERVICE = "efis-next-mfd-direct.service"
MANIFEST_SHA = "252c6f374da0a1cf5feba3477fc7419d65000ba8480ffff0cce149775c385eaa"
PREVIOUS_MANIFEST_SHA = "4a259e2f5c23fc3d876f3c08181fad67bbec69b00aad403c782232cf1956a1fe"
BINARY_SHA = "065ba0bda94cc30b03c4c79868be82546c5d02ddb58f89befb5f938dd606fad8"
PREVIOUS_BINARY_SHA = "5c3c1a42ad1f068fa742f2f3bb2f5225a9ba58a97bde6bf6145baf5b5abed64e"
ARCHIVE_SHA = "0ab89831f0887b1729aea5837feb98d329edb252bfe27e3dcad0b913b8334c15"
VALIDATOR_SHA = "cbe02dd613140352ddea334d46de498708110436fc2d0a037aa741a4571fd368"
PROFILE_SHA = "3d67d76a83ee9ea89ab7515097042d3c39aca9f77db4e6d3719bf139a7da7154"
BASE_UNIT = Path("/etc/systemd/system/efis-next-mfd-direct.service")
DROPIN = Path("/etc/systemd/system/efis-next-mfd-direct.service.d/release-v1.conf")
BASE_SHA = "6c68a4a161e84739b63cf9b657be571d80cba6f48550bafcef9ea2bf9832ceb7"
OLD_DROPIN_SHA = "551f8fd46d74c90072336c439907923949d6aecbc7939d75938f19e47d730210"
KMS = Path("/etc/efis/eglfs-kms.json")
KMS_SHA = "4d47595f20e57c99759c9be78a3fef2c2c89754eba5a6639cdf316563af24f10"
# Only this append is permitted. No build alias is created below the new release.
DROPIN_APPEND = b"\n[Unit]\nConditionPathExists=\nConditionPathExists=/home/pi/efis-next-current/bin/efis-pfd-qt\n"
POWER_APPROVAL = Path("/var/lib/efis-next/qualification/mfd-power-approval.json")
ENVIRONMENT = {
    "QT_QPA_PLATFORM": "eglfs", "QT_QPA_EGLFS_KMS_CONFIG": str(KMS),
    "EFIS_KIOSK": "1", "QSG_RENDER_LOOP": "threaded",
    "EFIS_START_PAGE": "CANVAS", "EFIS_NODE_ID": "MFD-01",
    "EFIS_NODE_PLATFORM": "RPi5", "EFIS_NODE_ROLE": "MULTIFUNCTION DISPLAY",
    "EFIS_SYSTEM_PROFILE": "PROFESSIONAL", "EFIS_BLACKBOX_STATUS_MODE": "DDP_LINK",
    "EFIS_DDP_BIND_ADDRESS": "0.0.0.0", "EFIS_DDP_LISTEN_PORT": "49002",
    "EFIS_DDP_SOURCE_NODE": "47", "EFIS_SIMULATION_ROLE": "FALLBACK",
    "EFIS_TERRAIN_GPU_ROOT": "/home/pi/efis-data/terrain/gpu",
    "EFIS_IMAGERY_TEXTURE_ROOT": "/home/pi/efis-data/imagery/active-textures",
    "EFIS_IMAGERY_ACTIVE_PACKAGE": "/home/pi/efis-data/imagery/active-package.json",
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


def approved_dropin(raw):
    digest = hashlib.sha256(raw).hexdigest()
    if digest == OLD_DROPIN_SHA:
        return True
    return raw.endswith(DROPIN_APPEND) and hashlib.sha256(raw[:-len(DROPIN_APPEND)]).hexdigest() == OLD_DROPIN_SHA


def config_gate():
    trusted_file(BASE_UNIT, BASE_SHA)
    trusted_file(VALIDATOR, VALIDATOR_SHA)
    trusted_file(KMS, KMS_SHA)
    raw = DROPIN.read_bytes()
    trusted_file(DROPIN, hashlib.sha256(raw).hexdigest())
    require(approved_dropin(raw), "unexpected MFD release drop-in")
    props = properties()
    require(props.get("User") == "pi", "unexpected service user")
    actual = set(props.get("DropInPaths", "").split()) | {props.get("FragmentPath", "")}
    require(actual == {str(BASE_UNIT), str(DROPIN)}, "unexpected unit or extra drop-in")


def power_gate():
    require(run(["/usr/bin/vcgencmd", "get_throttled"]).strip() == "throttled=0x0", "MFD power history gate failed")
    require(POWER_APPROVAL.is_file() and not POWER_APPROVAL.is_symlink(), "qualified power approval missing")
    raw = POWER_APPROVAL.read_bytes()
    trusted_file(POWER_APPROVAL, hashlib.sha256(raw).hexdigest())
    record = json.loads(raw)
    validate_power_approval(record, Path("/proc/sys/kernel/random/boot_id").read_text().strip())


def validate_power_approval(record, boot_id):
    require(isinstance(record, dict), "invalid power approval object")
    require(set(record) == {"schema", "node", "boot_id", "chain_identified", "representative_load_pass", "approved_by", "evidence_sha256"}, "invalid power approval fields")
    require(record["schema"] == "efis-next.qualified-power.v1" and record["node"] == "MFD", "wrong power approval identity")
    require(record["boot_id"] == boot_id, "power approval is not for this boot")
    require(record["chain_identified"] is True and record["representative_load_pass"] is True, "physical qualification incomplete")
    require(isinstance(record["approved_by"], str) and bool(record["approved_by"].strip()), "missing power approver")
    require(isinstance(record["evidence_sha256"], str) and re.fullmatch(r"[0-9a-f]{64}", record["evidence_sha256"]), "missing power evidence digest")


def validate_release(root, manifest_sha, binary_sha):
    require(root.is_dir() and not root.is_symlink(), f"invalid root: {root}")
    require(root.resolve() == root, f"ancestor symlink refused: {root}")
    manifest = json.loads(read_pinned(root / "release-manifest.json", manifest_sha))
    require(manifest.get("roles") == ["MFD"] and manifest.get("platform") == "RPI5_AARCH64", "wrong role/platform")
    require(manifest.get("flight_approval") is False and manifest.get("simulation_mode") is True, "SIM safety markers changed")
    if manifest_sha == MANIFEST_SHA:
        require(manifest.get("source_commit") == SOURCE, "wrong candidate source")
    read_pinned(root / "bin/efis-pfd-qt", binary_sha)
    read_pinned(root / "config/active/aircraft-profile.json", PROFILE_SHA)
    # A separately pinned, root-owned, standalone stdlib validator is the only
    # Python helper executed. Never import code from the writable candidate.
    run(["/usr/bin/python3", "-I", str(VALIDATOR), "--manifest", str(root / "release-manifest.json"),
         "--release-root", str(root), "--expected-role", "MFD", "--expected-platform", "RPI5_AARCH64", "--check-files"])


def runtime_identity(root):
    props = properties()
    require(props.get("ActiveState") == "active", "MFD not active")
    pid = props.get("MainPID", "")
    require(re.fullmatch(r"[1-9][0-9]*", pid), "missing main PID")
    require(Path(f"/proc/{pid}/exe").resolve() == root / "bin/efis-pfd-qt", "unexpected running executable")
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
    require(socket.gethostname() == "pi5" and platform.machine() == "aarch64", "wrong MFD host/architecture")
    require(Path("/proc/device-tree/model").read_text().startswith("Raspberry Pi 5 "), "wrong physical model")
    require(CURRENT.is_symlink() and CURRENT.resolve() == PREVIOUS, "current is not the pinned previous release")
    require(not SEALED.exists() and not SEALED.is_symlink(), "sealed target already exists; operator review required")
    require(shutil.disk_usage("/opt").free >= 8 * 1024 ** 3, "less than 8 GiB free")
    if SEALED_PARENT.exists():
        info = SEALED_PARENT.stat()
        require(not SEALED_PARENT.is_symlink() and info.st_uid == 0 and not info.st_mode & 0o022, "unsafe sealed parent")
    config_gate()
    require(hashlib.sha256(DROPIN.read_bytes()).hexdigest() == OLD_DROPIN_SHA, "current drop-in is not original")
    power_gate()
    for key in ("EFIS_TERRAIN_GPU_ROOT", "EFIS_IMAGERY_TEXTURE_ROOT", "EFIS_IMAGERY_ACTIVE_PACKAGE"):
        require(Path(ENVIRONMENT[key]).exists(), "missing external data: " + key)
    validate_release(PREVIOUS, PREVIOUS_MANIFEST_SHA, PREVIOUS_BINARY_SHA)
    validate_release(STAGED, MANIFEST_SHA, BINARY_SHA)
    with tarfile.open(fileobj=io.BytesIO(read_pinned(ARCHIVE, ARCHIVE_SHA)), mode="r:gz") as archive:
        validate_archive_members(archive)
    pid = runtime_identity(PREVIOUS)
    sockets = run(["/usr/bin/ss", "-lunp"])
    require(re.search(r'0\.0\.0\.0:49002\s.*pid=' + pid + r'[,)]', sockets), "DDP listener missing or wrong owner")
    for service in ["efis.service", "lightdm.service", "fuso-sv3d.service", "fuso-sv3d-sim.service", "efis-next-pfd-direct.service"]:
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
        self.original_dropin = None

    def prepare(self):
        # Everything that may execute was read once and verified before use.
        archive = read_pinned(ARCHIVE, ARCHIVE_SHA)
        binary = read_pinned(STAGED / "bin/efis-pfd-qt", BINARY_SHA)
        manifest = read_pinned(STAGED / "release-manifest.json", MANIFEST_SHA)
        evidence_parent = Path("/var/lib/efis-next/activation-evidence")
        evidence_parent.mkdir(parents=True, exist_ok=True)
        require(evidence_parent.resolve() == evidence_parent, "evidence ancestor symlink")
        require(evidence_parent.stat().st_uid == 0 and not evidence_parent.stat().st_mode & 0o022, "unsafe evidence parent")
        self.evidence = evidence_parent / ("mfd-a0be9a5-" + dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ"))
        self.evidence.mkdir(mode=0o700)
        print("EVIDENCE=" + str(self.evidence), flush=True)
        self.record("preflight.json", preflight())
        self.original_dropin = trusted_file(DROPIN, OLD_DROPIN_SHA)
        SEALED_PARENT.mkdir(mode=0o755, exist_ok=True)
        SEALED.mkdir(mode=0o700)
        print("PHASE=SEALING_VERIFIED_SOURCE", flush=True)
        safe_extract(archive, SEALED)
        (SEALED / "bin").mkdir(exist_ok=True)
        (SEALED / "bin/efis-pfd-qt").write_bytes(binary)
        (SEALED / "bin/efis-pfd-qt").chmod(0o555)
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
        self.install_dropin(self.original_dropin + DROPIN_APPEND)
        self.point(target)
        run(["/usr/bin/systemctl", "start", SERVICE])
        self.expected = target

    def install_dropin(self, raw):
        require(self.original_dropin is not None and approved_dropin(raw), "unapproved drop-in bytes")
        # Fixed root-owned directory; replace only our UUID temporary regular file.
        require(DROPIN.parent.stat().st_uid == 0 and not DROPIN.parent.stat().st_mode & 0o022 and not DROPIN.parent.is_symlink(), "unsafe drop-in directory")
        require(DROPIN.is_file() and not DROPIN.is_symlink(), "unsafe current drop-in")
        temporary = DROPIN.parent / (".mfd-dropin-" + uuid.uuid4().hex)
        descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o644)
        try:
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(raw)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, DROPIN)
            directory = os.open(DROPIN.parent, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
        finally:
            if temporary.is_file():
                temporary.unlink()
        run(["/usr/bin/systemctl", "daemon-reload"])

    def point(self, target):
        require(target in (PREVIOUS, SEALED), "target outside pinned release set")
        require(CURRENT.is_symlink() and CURRENT.resolve() in (PREVIOUS, SEALED), "refusing to replace unexpected current")
        temporary = CURRENT.parent / (".mfd-activation-" + str(os.getpid()) + "-" + uuid.uuid4().hex)
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
        require(re.search(r'0\.0\.0\.0:49002\s.*pid=' + pid + r'[,)]', sockets), "DDP listener does not belong to MFD")
        journal = run(["/usr/bin/journalctl", "-u", SERVICE, "--after-cursor", self.cursor, "--no-pager", "-o", "cat"])
        require("event=startup product=efis-next component=pfd" in journal, "missing fresh startup")
        require(re.search(r'event=render_health .*ddp_state= LIVE .*ddp_accepted= [1-9][0-9]* .*ddp_rejected= 0 .*ddp_socket_errors= 0', journal), "no fresh DDP live heartbeat")
        rates = re.findall(r'event=render_health presentation_hz=\s*([0-9.]+)', journal)
        require(rates and all(float(rate) > 0 for rate in rates), "no positive renderer heartbeat")
        require(not re.search(r'engine_runtime_(save_failed|rejected)|segmentation fault|core dumped|failed to create display', journal, re.IGNORECASE), "runtime/render fault")
        suffix = "previous" if target == PREVIOUS else "candidate"
        (self.evidence / (suffix + "-journal-" + str(pid) + ".txt")).write_text(journal)
        self.record(suffix + "-runtime.json", {"pid": pid, "executable": str(target / "bin/efis-pfd-qt"), "power": "0x0"})

    def recover(self):
        # No power gate on recovery: a candidate failure must not prevent
        # restoration. Never change runtime data or network; restore the pinned original drop-in.
        try:
            run(["/usr/bin/systemctl", "stop", SERVICE])
        except Exception as error:
            self.record("recovery-stop-error.json", {"error": str(error)})
        restore_error = None
        try:
            self.install_dropin(self.original_dropin)
        except Exception as error:
            restore_error = error
            self.record("recovery-dropin-error.json", {"error": str(error)})
        self.point(PREVIOUS)
        run(["/usr/bin/systemctl", "start", SERVICE])
        time.sleep(8)
        runtime_identity(PREVIOUS)
        require(restore_error is None, "previous started but drop-in restoration failed: " + str(restore_error))
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
        lock = os.open("/run/lock/efis-mfd-a0be9a5.lock", os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
        info = os.fstat(lock)
        require(stat.S_ISREG(info.st_mode) and info.st_uid == 0 and not info.st_mode & 0o022, "unsafe activation lock")
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            preflight()
            signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(InterruptedError("SIGTERM")))
            operations = Operations()
            transaction(operations)
            print("MFD_A0BE9A5_ACTIVATION=PASS\nROLLBACK_DRILL=PASS\nNEXT_REQUIRED=REBOOT_AND_VISUAL_ACCEPTANCE", flush=True)
            print("EVIDENCE=" + str(operations.evidence), flush=True)
        finally:
            os.close(lock)
        return 0
    except BaseException as error:
        print("MFD_A0BE9A5_ACTIVATION=FAIL: " + str(error), file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

