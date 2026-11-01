"""Specific PFD installer, explicit owner authorization required. No sshd restart."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import time

ROOT = Path("/usr/local/lib/efis-next/sil-r120")
AUTH = Path("/home/efis/.ssh/authorized_keys")
SUDOERS = Path("/etc/sudoers.d/efis-sil-r120")
TOOL = Path("/home/efis/efis-sil-panel-r117/r119-build-v4/efis-sil-panel-replay")
BIN_SHA = "7d1c5880e6e06d69f71b6c3fe26a8f67860a65fe2259f661c49ad960066f6167"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def restricted_entry(text):
    public = text.strip().split()
    if len(public) != 3 or public[0] != "ssh-ed25519" or public[2] != "efis-sil-serverha-to-pfd-r120":
        raise ValueError("unexpected public key")
    blob = base64.b64decode(public[1], validate=True)
    if (len(blob) != 51 or blob[:4] != b"\0\0\0\x0b" or blob[4:15] != b"ssh-ed25519"
            or blob[15:19] != b"\0\0\0\x20"):
        raise ValueError("invalid ed25519 public blob")
    return ('restrict,from="192.168.1.6",command="' + (ROOT / "dispatch.sh").as_posix() + '" '
            + " ".join(public) + "\n").encode()


def trusted_directory(path):
    info = path.lstat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != 0 or info.st_mode & 0o022:
        raise ValueError("untrusted root directory: " + str(path))


def install_new(path, data, mode):
    if path.exists() or path.is_symlink():
        raise ValueError("refusing overwrite: " + str(path))
    with path.open("xb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    os.chown(path, 0, 0)
    os.chmod(path, mode)


def install(stage, dispatch_sha, runner_sha):
    import pwd
    if os.geteuid() != 0 or os.uname().nodename != "efis-pfd-01":
        raise ValueError("root on pinned PFD required")
    stage = Path(stage)
    dispatch = (stage / "sil-ssh-dispatch-r120.sh").read_bytes()
    runner = (stage / "sil-run-adapter-r120.sh").read_bytes()
    binary = TOOL.read_bytes()
    if (digest(dispatch) != dispatch_sha or digest(runner) != runner_sha
            or len(binary) > 32 * 1024 * 1024 or digest(binary) != BIN_SHA):
        raise ValueError("source digest mismatch")
    public_text = (stage / "sil-key.pub").read_text()
    entry = restricted_entry(public_text)
    public = public_text.strip().split()
    if AUTH.is_symlink() or not AUTH.is_file():
        raise ValueError("authorized_keys must be a regular existing file")
    original = AUTH.read_bytes()
    if public[1].encode() in original:
        raise ValueError("public key already present; audit instead of duplicate")
    for parent in (Path("/usr"), Path("/usr/local"), Path("/usr/local/lib"), ROOT.parent):
        trusted_directory(parent)
    if ROOT.exists() or SUDOERS.exists():
        raise ValueError("installation already present; do not replace blindly")
    backup_root = Path("/var/lib/efis-next")
    trusted_directory(backup_root.parent)
    backup_root.mkdir(mode=0o700, exist_ok=True)
    trusted_directory(backup_root)
    backup = backup_root / ("sil-ssh-backup-r120-" + str(time.time_ns()))
    backup.mkdir(mode=0o700)
    install_new(backup / "authorized_keys.before", original, 0o600)
    ROOT.mkdir(mode=0o755)
    install_new(ROOT / "dispatch.sh", dispatch, 0o555)
    install_new(ROOT / "run-adapter.sh", runner, 0o555)
    install_new(ROOT / "adapter", binary, 0o555)
    sudoers = ("efis ALL=(root) NOPASSWD: " + str(ROOT) + "/run-adapter.sh check, "
               + str(ROOT) + "/run-adapter.sh feed\n").encode()
    install_new(ROOT / "sudoers.candidate", sudoers, 0o440)
    subprocess.run(["/usr/sbin/visudo", "-cf", str(ROOT / "sudoers.candidate")], check=True)
    install_new(SUDOERS, sudoers, 0o440)
    owner = pwd.getpwnam("efis")
    temporary = None
    try:
        fd, temporary = tempfile.mkstemp(prefix=".sil-r120-", dir=AUTH.parent)
        with os.fdopen(fd, "wb") as stream:
            stream.write(original + (b"" if original.endswith(b"\n") else b"\n") + entry)
            stream.flush()
            os.fsync(stream.fileno())
        os.chown(temporary, owner.pw_uid, owner.pw_gid)
        os.chmod(temporary, 0o600)
        if AUTH.read_bytes() != original:
            raise ValueError("authorized_keys changed concurrently")
        os.replace(temporary, AUTH)
        temporary = None
    finally:
        if temporary is not None:
            os.unlink(temporary)
    subprocess.run(["/usr/sbin/sshd", "-t"], check=True)
    return {"status": "PASS", "backup": str(backup), "existing_keys_preserved": True,
            "authorized_keys_before_sha256": digest(original), "entry_sha256": digest(entry),
            "private_key_transferred": False, "sshd_restarted": False,
            "adapter_sha256": digest(binary), "commands": ["SIL_CHECK_V1", "SIL_FEED_V1"]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", required=True)
    parser.add_argument("--dispatch-sha", required=True)
    parser.add_argument("--runner-sha", required=True)
    args = parser.parse_args()
    print(json.dumps(install(args.stage, args.dispatch_sha, args.runner_sha), sort_keys=True))
