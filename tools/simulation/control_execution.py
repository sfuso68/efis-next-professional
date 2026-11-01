"""Opt-in local SIL executor. Trusted injected FDM only, no endpoints/actuators.

Sidecar fsyncs intent/result synchronously. This is input-property readback,
not an independent aerodynamic oracle or supported live replay archive.
"""
import copy
import hashlib
import os
from pathlib import Path
import threading

from tools.simulation import session_contract as c
from tools.simulation.command_queue import CommandQueue
from tools.simulation.pilot_fixture import INPUTS
from tools.simulation.session_engine import encode

SCHEMA = "fuso-avionics.sil-control-audit.v1"


class SyncControlAudit:
    MAX_RECORDS = 1024
    MAX_BYTES = 1048576
    MAX_RECORD_BYTES = 65536

    def __init__(self, path, binding):
        self.path = Path(path)
        self.binding = copy.deepcopy(binding)
        self.owner = threading.get_ident()
        self.stream = self.path.open("xb", buffering=0)
        self.records = self.bytes = self.tick = 0
        self.digest = hashlib.sha256()
        self.closed = False
        try:
            self.record("BOUND", 0, binding=self.binding)
        except Exception:
            self.close()
            raise

    def record(self, event, tick, **fields):
        c.require(threading.get_ident() == self.owner, "audit owner")
        c.integer(tick, self.tick)
        c.require(event in ("BOUND", "INTENT", "APPLIED", "REJECTED", "RELEASED"), "audit event")
        m = self.binding["manifest"]
        row = dict(schema=SCHEMA, kind="CONTROL_AUDIT", index=self.records, event=event,
                   session_id=m["session_id"], epoch=m["epoch"], tick=tick,
                   simulated=True, flight_approval=False, **fields)
        data = encode(row)
        c.require(not self.closed and self.records < self.MAX_RECORDS
                  and len(data) <= self.MAX_RECORD_BYTES
                  and self.bytes+len(data) <= self.MAX_BYTES, "audit capacity/closed")
        if self.stream.write(data) != len(data):raise OSError("short control audit write")
        os.fsync(self.stream.fileno())
        self.digest.update(data)
        self.bytes += len(data)
        self.records += 1
        self.tick = tick

    def close(self):
        if not self.closed:
            self.closed = True
            self.stream.close()

    def metadata(self):
        c.require(self.closed, "control audit still open")
        return dict(scope="SYNC_SIDECAR_NOT_REPLAY_ARCHIVABLE", file=self.path.name,
                    sha256=self.digest.hexdigest(), records=self.records, bytes=self.bytes)


class ControlExecutor:
    def __init__(self, admission, backend, audit_path):
        for method in ("set_test_control", "clear_test_controls", "read"):
            c.require(callable(getattr(backend, method, None)), "SIL backend capability")
        c.require(hasattr(backend, "catalog"), "SIL property catalogue")
        self.admission, self.backend = admission, backend
        self.audit = SyncControlAudit(audit_path, admission.commands.binding)
        self.inflight = ()
        self.released = False
        self.tick = 0
        self.applied = 0
        self.started = False
        self.uncertain = []

    def physical_step_started(self):
        self.started = True

    def invalidate_inflight(self):
        if self.started:
            for request in self.inflight:
                if self.admission.commands.mark_uncertain(request):
                    self.uncertain.append(request["command_id"])
            self.started = False

    def release(self):
        """Remove overrides before I/O; NOT neutralization/property restoration."""
        if self.released:return
        try:
            self.backend.clear_test_controls()
            self.released = True
            self.audit.record("RELEASED", self.tick, scope="OVERRIDE_MAP_ONLY",
                              uncertain_commands=list(self.uncertain))
        finally:
            self.audit.close()

    def before_step(self, tick, guard):
        if self.released:return
        self.tick = tick
        c.require(not self.inflight, "previous execution incomplete")
        queue = self.admission.commands
        due = [c.decode(raw) for raw in queue.take_due(current_tick=tick)]
        if queue.closed:
            self.release()
            return
        accepted, rejected, controls = [], [], set()
        # Validate the entire batch before any FDM input writes.
        for request in due:
            control = request["args"].get("control")
            if (request["action"] != "SET_CONTROL" or control not in INPUTS
                    or INPUTS[control] not in self.backend.catalog):
                rejected.append((request, "UNSUPPORTED"))
            elif control in controls:
                rejected.append((request, "INVALID"))
            else:
                controls.add(control)
                accepted.append(request)
        for request, reason in rejected:
            ack = CommandQueue._ack(request, reason)
            self.audit.record("REJECTED", tick, request=request, ack=ack)
            queue.record_outcome(request, ack, current_tick=tick)
        self.inflight = tuple(accepted)
        for request in accepted:
            self.audit.record("INTENT", tick, request=request)
            guard()  # INTENT fsync also counts against physical/writer deadlines.
            c.require(self.admission.inbox.authorize_dispatch(queue, request, current_tick=tick),
                      "execution lease revoked")
            self.backend.set_test_control(**request["args"])

    def authorize_step(self, tick):
        """Last authority check after writer/deadline guard, before physics."""
        if self.released:return True
        self.admission.commands.maintain(current_tick=tick)
        if self.admission.commands.closed or self.admission.inbox.revoked:
            uncertain_batch = bool(self.inflight)
            self.admission.revoke()
            c.require(not uncertain_batch, "execution revoked before physical step")
            return True
        return False

    def after_step(self, tick):
        if self.released:return
        c.require(tick == self.tick, "execution step tick")
        # Validate all readbacks before asserting any APPLIED outcome.
        values = []
        for request in self.inflight:
            prop = INPUTS[request["args"]["control"]]
            value = self.backend.read(prop)
            c.number(value)
            c.require(abs(value-request["args"]["value"]) <= 1e-12, "SIL input readback mismatch")
            values.append((request, prop, value))
        for request, prop, value in values:
            ack = CommandQueue._ack(request)
            ack.update(status="APPLIED", applied_tick=tick)
            self.audit.record("APPLIED", tick, request=request, ack=ack, property=prop, readback=value)
            self.admission.commands.record_outcome(request, ack, current_tick=tick)
            self.applied += 1
        self.inflight = ()
        self.started = False
        # Do not revoke dispatched work between completed physics and recording
        # its known outcome, even if result fsync crosses the lease deadline.
        self.admission.commands.maintain(current_tick=tick)
        if self.admission.commands.closed:
            self.release()
