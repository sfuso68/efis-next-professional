"""Specific serverha/PFD SIL bench over two authenticated SSH pipes. Not a router."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import queue
import re
import shlex
import subprocess
import threading
import time
from tools.simulation.jsbsim_session import strict_json, write_json

SCHEMA = "fuso-avionics.sil-live.v1"
CONFIG_SHA = "f9296942c9bab6f8738e3b0b3c6c55b49908b0a122805b5103155ede47dcf9bb"
BASE = "/home/sfuso/avionix-sil-r114-20261025"
IMAGE = "sha256:db3ff2e1800a8581e2c48a27c3995339d47bdf046da21c7627accd3d51053a93"
TOOL = "/home/efis/efis-sil-panel-r117/r119-build-v4/efis-sil-panel-replay"


def commands(session, seconds, send):
    if not re.fullmatch(r"[a-zA-Z0-9_-]{1,64}", session) or type(seconds) is not int or not 1 <= seconds <= 120:
        raise ValueError("invalid bounded session")
    server = ["docker", "run", "-i", "--rm", "--pull", "never", "--network", "none", "--read-only",
              "--tmpfs", "/tmp:rw,noexec,nosuid,size=64m", "--cap-drop=ALL", "--security-opt", "no-new-privileges",
              "--cpus=2", "--memory=768m", "--pids-limit=64", "-u", "1000:1000", "-e", "PYTHONPATH=/runtime:/source",
              "-e", "JSBSIM_DEBUG=0", "-v", BASE+"/runtime:/runtime:ro", "-v", BASE+"/r119-source-v3:/source:ro",
              "-v", BASE+"/runs:/runs", "-w", "/source", IMAGE, "python", "-B", "-m", "tools.simulation.jsbsim_live",
              "--world", "/source/config/simulation/treviso-belluno-world-reference.json",
              "--directory", "/runs/"+session, "--seconds", str(seconds), "--scenario", "turn"]
    panel = [TOOL, CONFIG_SHA, "--live-check-stdio"]
    if send:
        panel = ["sudo", "-n", "systemd-run", "--quiet", "--pipe", "--wait", "--collect",
                 "--unit=efis-sil-live-feed", "--uid=efis", "--property=PrivateNetwork=yes",
                 "--property=JoinsNamespaceOf=efis-sil-panel-screen.service", "--property=RuntimeMaxSec=140",
                 "--property=TimeoutStopSec=3",
                 "--property=ExecStopPost=+/usr/bin/systemctl stop efis-sil-panel-screen.service",
                 TOOL, CONFIG_SHA, "--live-loopback-stdio"]
    prefix = ('test "$(sha256sum '+TOOL+' | cut -d\' \' -f1)" = '
              '7d1c5880e6e06d69f71b6c3fe26a8f67860a65fe2259f661c49ad960066f6167 && ')
    if send:
        prefix += ('sudo -n systemctl is-active --quiet efis-sil-panel-screen.service && '
                   'test "$(sudo -n systemctl show efis-sil-panel-screen.service -p PrivateNetwork --value)" = yes && ')
    return shlex.join(server), prefix+shlex.join(panel)


def reader(stream, messages):
    try:
        while True:
            line = stream.readline(65537)
            if not line:
                messages.put(None, timeout=2)
                return
            if len(line) > 65536:
                raise ValueError("oversize SSH line")
            messages.put(strict_json(line), timeout=2)
    except Exception as exc:
        try:
            messages.put(exc, timeout=2)
        except queue.Full:
            pass


def receive(messages, seconds=3):
    try:
        value = messages.get(timeout=seconds)
    except queue.Empty:
        raise ValueError("SSH response timeout") from None
    if isinstance(value, Exception):
        raise ValueError("SSH stream invalid") from value
    if not isinstance(value, dict):
        raise ValueError("SSH stream EOF/nonobject")
    return value


def send(process, message):
    process.stdin.write(json.dumps(message, separators=(",", ":"), allow_nan=False).encode()+b"\n")
    process.stdin.flush()


def run(key, directory, session, seconds, loopback=False):
    source_command, panel_command = commands(session, seconds, loopback)
    key = Path(key).resolve(strict=True)
    directory = Path(directory)
    directory.mkdir(parents=False, exist_ok=False)
    processes, logs = [], []
    report = {"schema": SCHEMA, "status": "FAIL", "simulated": True, "flight_approval": False,
              "session_directory": session, "frames_forwarded": 0, "mode": "LOOPBACK" if loopback else "CHECK_ONLY"}
    started = time.monotonic()
    try:
        for name, endpoint, command in (("source", "sfuso@192.168.1.6", source_command),
                                        ("panel", "efis@192.168.1.45", panel_command)):
            log = (directory / (name+"-stderr.txt")).open("xb")
            logs.append(log)
            process = subprocess.Popen(["ssh", "-T", "-i", str(key), "-o", "BatchMode=yes",
                                        "-o", "ConnectTimeout=6", "-o", "ServerAliveInterval=2",
                                        "-o", "ServerAliveCountMax=2", endpoint, command],
                                       stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log)
            processes.append(process)
        source, panel = processes
        source_messages, panel_messages = queue.Queue(2), queue.Queue(2)
        for process, messages in ((source, source_messages), (panel, panel_messages)):
            threading.Thread(target=reader, args=(process.stdout, messages), daemon=True).start()
        hello = receive(source_messages, 10)
        if (hello.get("schema") != SCHEMA or hello.get("kind") != "HELLO" or hello.get("seconds") != seconds
                or hello.get("simulated") is not True or hello.get("flight_approval") is not False
                or hello.get("world_config_sha256") != CONFIG_SHA):
            raise ValueError("source hello contract")
        send(panel, hello)
        for index in range(seconds * 20 + 1):
            request = receive(panel_messages)
            if (request.get("schema") != SCHEMA or type(request.get("sequence")) is not int
                    or request["sequence"] != index or not re.fullmatch(r"[a-f0-9]{32}", str(request.get("nonce")))):
                raise ValueError("panel challenge contract")
            send(source, request)
            response = receive(source_messages)
            if (response.get("schema") != SCHEMA or response.get("kind") != "FRAME"
                    or response.get("nonce") != request["nonce"]):
                raise ValueError("source challenge mismatch")
            send(panel, response)
            report["frames_forwarded"] += 1
        result = receive(panel_messages)
        if (result.get("kind") != "COMPLETE" or result.get("frames") != seconds * 20 + 1
                or result.get("schema") != SCHEMA or result.get("simulated") is not True
                or result.get("flight_approval") is not False
                or result.get("mode") != ("LIVE_LOOPBACK" if loopback else "LIVE_CHECK_ONLY")):
            raise ValueError("panel completion contract")
        for process in processes:
            if process.wait(timeout=10) != 0:
                raise ValueError("SSH child failed")
        report.update(status="PASS", panel_result=result, source_session_id=hello["session_id"])
    except BaseException as exc:
        report.update(error_type=type(exc).__name__, reason=str(exc))
        raise
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=2)
            for stream in (process.stdin, process.stdout):
                try:
                    stream.close()
                except (OSError, BrokenPipeError):
                    pass
        for log in logs:
            log.close()
        report["elapsed_seconds"] = time.monotonic()-started
        write_json(directory / "bridge-report.json", report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ssh-key", required=True, type=Path)
    parser.add_argument("--directory", required=True, type=Path)
    parser.add_argument("--session", required=True)
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--loopback", action="store_true", help="Requires isolated screen bench already active")
    args = parser.parse_args()
    try:
        print(json.dumps(run(args.ssh_key, args.directory, args.session, args.seconds, args.loopback)))
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        parser.exit(2, f"SIL bridge failed: {exc}\n")


if __name__ == "__main__":
    main()
