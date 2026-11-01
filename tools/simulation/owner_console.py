"""Finite local stdio JSBSim SIL console. No network listeners or real hardware.

Run only from an existing trusted local/SSH OS session. EOF, overlong stdin,
I/O failure, STOP or wall cap revoke authority; no automatic rearm. Replies
are volatile latest-only diagnostics. Run files, not stdout, are evidence.
"""
import argparse
import copy
import hashlib
import os
from pathlib import Path
import threading
import time

from tools.simulation import session_contract as c
from tools.simulation.async_journal import AsyncTruthJournal
from tools.simulation.control_audit import FILE, execution_policy
from tools.simulation.local_console import LocalConsole
from tools.simulation.session_control import SessionAdmission
from tools.simulation.session_engine import SessionEngine, TruthJournal, encode
from tools.simulation.session_jsbsim import Backend, PhysicalAdapter, VERSION
from tools.simulation.sil_world import config_digest


class StdioChannel:
    """Bounded FD ingress and one active + one latest immutable output packet.

    Direct os.read/write avoids buffered-stdio daemon shutdown locks. A blocked
    OS write may outlive the finite join: it must never block the physics owner.
    No claim of reliable response delivery; use retained command-ID LOOKUP.
    """
    def __init__(self, console, input_fd, output_fd):
        self.console, self.input_fd, self.output_fd = console, input_fd, output_fd
        self._lock = threading.Lock()
        self._end, self._ready = threading.Event(), threading.Event()
        self._packet = None
        self.overwritten = 0
        self.ingress = (0, "NONE")
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.writer = threading.Thread(target=self._write, daemon=True)

    def start(self):
        self.writer.start();self.reader.start()

    def _read(self):
        pending = b""
        try:
            while not self._end.is_set():
                chunk = os.read(self.input_fd, min(1024, LocalConsole.MAX_LINE+1-len(pending)))
                if not chunk:
                    self.console.request_stop();return
                pending += chunk
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    status = self.console.offer(line)
                    self.ingress = (min(self.ingress[0]+1, 2**53-1), status)
                    if status.startswith("STOP"):return
                if len(pending) > LocalConsole.MAX_LINE:
                    self.console.request_stop();return
        except OSError:self.console.request_stop(failed=True)

    def publish(self, packet):
        c.require(type(packet) is bytes and len(packet) <= 65536, "bounded output")
        # Owner never waits for an output consumer or for its writer lock.
        if not self._lock.acquire(blocking=False):
            self.overwritten += 1;return
        try:
            if self._packet is not None:self.overwritten += 1
            self._packet = packet
            self._ready.set()
        finally:self._lock.release()

    def _write(self):
        try:
            while True:
                self._ready.wait(.05)
                with self._lock:
                    packet, self._packet = self._packet, None
                    self._ready.clear()
                if packet is not None:
                    view = memoryview(packet)
                    while view:
                        size = os.write(self.output_fd, view)
                        if size <= 0:raise OSError("short console write")
                        view = view[size:]
                if self._end.is_set():return
        except OSError:self.console.request_stop(failed=True)

    def close(self):
        self._end.set();self._ready.set()
        if self.writer.ident is not None:self.writer.join(.05)
        # Input may still be blocked waiting for the existing SSH session.
        if self.reader.ident is not None:self.reader.join(.05)


def publish_snapshot(channel, packet, last_generation):
    """One slot update: ingress metadata must not overwrite its owner reply."""
    generation, ingress = channel.ingress
    if packet != b"[]\n" or generation != last_generation:
        channel.publish(encode(dict(console="SNAPSHOT", replies=c.decode(packet),
            ingress=dict(generation=generation, status=ingress),
            output_overwritten=channel.overwritten, simulated=True, flight_approval=False)))
    return generation


def run(directory, world, *, session_id, source_commit, input_fd=0, output_fd=1,
        seconds=10, journal_mode="async-test", channel_factory=StdioChannel):
    """Finite cooperative owner loop. Kernel/fsync stalls are NOT preempted."""
    c.token(session_id);c.integer(seconds, 1, 60);config_digest(world)
    c.require(journal_mode in ("sync", "async-test"), "journal mode")
    backend = Backend(copy.deepcopy(world))
    lease_ticks = min(seconds*100, 500)
    configuration = dict(world=world, scenario="cruise", seconds=seconds, pause_tick=None,
        backend="JSBSim-"+VERSION, model="c172p",
        recording_policy=copy.deepcopy(AsyncTruthJournal.policy) if journal_mode=="async-test"
                         else dict(mode="SYNC_PER_RECORD"),
        control_policy=execution_policy("local-console", "local-console-lease", lease_ticks),
        console_policy=dict(mode="LOCAL_STDIO_V1", max_line=4096, max_messages=16,
                            max_per_service=4, max_history=256, max_history_bytes=524288,
                            wall_cap_seconds=seconds+60, lifecycle="STOP_ONLY", eof="STOP"))
    manifest = dict(schema=c.SCHEMA, kind="MANIFEST", session_id=session_id, epoch=0,
        simulated=True, flight_approval=False, mode="LIVE", epoch_utc=world["epoch_utc"],
        integration_hz=100, output_hz=20, source_node=65000,
        config_sha256=hashlib.sha256(encode(configuration)).hexdigest(),
        model_sha256=backend.model_digest["sha256"], source_commit=source_commit,
        profile_status="REFERENCE_UNVALIDATED", event_ids=[], station_ids=[], route_hashes=[])
    c.manifest(manifest)
    journal = (AsyncTruthJournal if journal_mode=="async-test" else TruthJournal)(directory)
    control = engine = channel = None
    try:
        journal.append(dict(kind="RUN_CONFIGURATION", configuration=configuration, manifest=manifest))
        adapter = PhysicalAdapter(backend, "cruise", journal)
        engine = SessionEngine(manifest, adapter, journal, max_ticks=seconds*100)
        control = SessionAdmission(manifest, principal="local-console", lease_id="local-console-lease",
            lease_until_tick=lease_ticks, lease_duration_ns=5_000_000_000)
        control.enable_execution(backend, Path(directory)/FILE)
        engine.attach_admission(control)
        engine.validate();engine.arm_software_test();engine.start()
        console = LocalConsole(engine, control)
        channel = channel_factory(console, input_fd, output_fd)
        channel.publish(console.latest());channel.start()
        started = time.monotonic();last_ingress = 0
        while engine.state in ("RUNNING", "PAUSED"):
            if time.monotonic()-started >= seconds+60:console.request_stop()
            advanced = console.pump()
            last_ingress = publish_snapshot(channel, console.latest(), last_ingress)
            if not advanced:time.sleep(.001)
        c.require(engine.state == "COMPLETE", "console run did not complete")
        summary = dict(status=engine.state, frames=engine.frames, total_ticks=engine.total_ticks,
            epoch=engine.manifest["epoch"], backend="JSBSim-"+VERSION,
            elapsed_s=time.monotonic()-started, maximum_lag_ms=engine.maximum_lag_ns/1e6,
            simulated=True, flight_approval=False, network_publisher=False,
            actual_display_connected=False, journal_mode=journal_mode,
            console_output_overwritten=channel.overwritten,
            console_ingress=channel.ingress,
            console_scope="LOCAL_STDIO_ONLY", model_status="REFERENCE_UNVALIDATED")
        with (Path(directory)/"probe-report.json").open("xb") as stream:
            stream.write(encode(summary));stream.flush();os.fsync(stream.fileno())
        return summary
    except BaseException:
        # Unexpected transport/runner failures and interruption are not a
        # successful STOP. Preserve incomplete evidence; never invent COMPLETE.
        if engine is not None and engine.state in ("RUNNING", "PAUSED", "ARMED_TEST"):
            engine._fail("CONSOLE_RUNNER_FAILURE")
        raise
    finally:
        try:
            if channel is not None:channel.close()
        finally:
            try:
                if control is not None and control.executor is not None:control.executor.audit.close()
            finally:journal.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--world", type=Path, required=True)
    parser.add_argument("--session-id", required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--seconds", type=int, default=10)
    parser.add_argument("--journal-mode", choices=("sync", "async-test"), default="async-test")
    args = parser.parse_args()
    try:
        c.require(args.world.is_file() and not args.world.is_symlink(), "regular world required")
        with args.world.open("rb") as stream:raw=stream.read(65537)
        c.require(len(raw)<=65536, "world size")
        run(args.directory, c.decode(raw), session_id=args.session_id, source_commit=args.source_commit,
            seconds=args.seconds, journal_mode=args.journal_mode)
    except (ValueError, OSError, RuntimeError, ImportError) as exc:
        parser.exit(2, f"Local SIL console FAILED: {exc}\n")
    except KeyboardInterrupt:parser.exit(130, "Local SIL console interrupted; inspect run evidence\n")


if __name__ == "__main__":main()
