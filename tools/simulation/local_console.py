"""Bounded local SIL console controller; no SSH server, sockets or hardware.

The owning runtime supplies the OS-authenticated process context. Producers
only enqueue bytes or latch STOP; only the engine owner mutates lifecycle/FDM.
This module is not a runnable console, authentication service or flight API.
"""
from collections import deque
import copy
import threading

from tools.simulation import session_contract as c
from tools.simulation.pilot_fixture import INPUTS
from tools.simulation.session_engine import encode


class LocalConsole:
    MAX_LINE = 4096
    MAX_MESSAGES = 16
    MAX_PER_SERVICE = 4
    MAX_HISTORY = 256
    MAX_HISTORY_BYTES = 524288

    def __init__(self, engine, admission, *, lifecycle=False):
        engine._owner()
        admission._owner_check()
        c.require(engine._control is admission and admission.executor is not None,
                  "console requires attached SIL executor")
        c.require(engine.state == "RUNNING", "console attachment requires running engine")
        c.require(type(lifecycle) is bool,"console lifecycle opt-in")
        self.engine, self.admission = engine, admission
        self.binding = admission.commands.binding
        self.lifecycle, self._lifecycle_index = lifecycle, 0
        self._owner = threading.get_ident()
        self._lock, self._stop, self._fault = threading.Lock(), threading.Event(), threading.Event()
        self._items = deque()
        self._history, self._history_bytes = {}, 0
        self._latest = encode(dict(console="READY", simulated=True, flight_approval=False))

    def _check_owner(self):
        if threading.get_ident() != self._owner:
            raise RuntimeError("console service requires engine owner")

    def request_stop(self, *, failed=False):
        """Also used by a future stdin reader on EOF/error; independent of FIFO."""
        if failed:self._fault.set()
        self.admission.signal_revoke()
        self._stop.set()

    def offer(self, raw):
        """Producer ingress, no engine reads. Overlong/nonbytes input stops run.

        A caller reading stdin must use bounded readline(MAX_LINE+1), never
        read an unlimited line and then pass it here. Malformed JSON is only
        INVALID; EOF/read failure must call request_stop, not offer(b'').
        """
        if type(raw) is not bytes or not 1 <= len(raw) <= self.MAX_LINE:
            self.request_stop()
            return "STOP_REQUESTED_INPUT_BOUND"
        try:
            value = c.decode(raw)
        except ValueError:
            return "INVALID"
        if value == {"action": "STOP"}:
            self.request_stop()
            return "STOP_REQUESTED"
        if self._stop.is_set():return "STOPPING"
        if not self._lock.acquire(blocking=False):return "BUSY"
        try:
            if self._stop.is_set():return "STOPPING"
            if len(self._items) >= self.MAX_MESSAGES:return "FULL"
            self._items.append(raw)
            return "QUEUED_NOT_VALIDATED"
        finally:self._lock.release()

    def _lookup(self, identity):
        entry = self._history.get(identity)
        if entry is None:return dict(console="UNKNOWN_ID", command_id=identity)
        _, canonical, initial = entry
        if isinstance(initial,dict):return copy.deepcopy(initial)
        if canonical is None:return dict(console=initial, command_id=identity)
        try:
            ack = self.admission.lookup_outcome(canonical, principal=self.binding["authority_id"])
        except RuntimeError:
            return dict(console="UNCERTAIN_NO_RETRY", command_id=identity)
        return dict(console=initial, command_id=identity, ack=ack)

    def _lifecycle(self, obj):
        from tools.simulation import console_lifecycle as life
        life.request(obj)
        identity, shorthand = obj["command_id"], encode(obj)
        previous = self._history.get(identity)
        if previous is not None:
            if previous[0] != shorthand:return dict(console="DUPLICATE_CONFLICT", command_id=identity)
            return self._lookup(identity)
        if len(self._history)>=self.MAX_HISTORY or self._history_bytes+len(shorthand)>self.MAX_HISTORY_BYTES:
            self.request_stop();return dict(console="HISTORY_FULL_STOP_REQUESTED")
        # Reserve before I/O. Unknown execution after intent/result failure is
        # retained as uncertain and causes FAILED; never retried automatically.
        self._history[identity]=[shorthand,None,dict(console="UNCERTAIN_NO_RETRY",command_id=identity)]
        self._history_bytes+=len(shorthand)
        before=life.state(self.engine)
        result=life.outcome(obj,before,self.binding["manifest"]["session_id"])
        try:
            self.engine.journal.append(life.record(self._lifecycle_index,"INTENT",obj,before,None,None,
                                       self.binding["manifest"]["session_id"]))
            self._lifecycle_index+=1
            # STOP observed after intent never allows a lifecycle transition.
            if self._stop.is_set():raise RuntimeError("stop interrupted lifecycle")
            if result=="APPLIED":
                if obj["action"]=="PAUSE":self.engine.pause()
                else:self.engine.resume()
            after=life.state(self.engine)
            self.engine.journal.append(life.record(self._lifecycle_index,"RESULT",obj,before,after,result,
                                       self.binding["manifest"]["session_id"]))
            self._lifecycle_index+=1
        except BaseException as exc:
            self.request_stop(failed=True)
            if self.engine.state in ("RUNNING","PAUSED","ARMED_TEST"):
                self.engine._fail("CONSOLE_LIFECYCLE_FAILURE")
            raise RuntimeError("lifecycle outcome uncertain; run failed") from exc
        reply=dict(console="LIFECYCLE_RESULT",command_id=identity,outcome=result,
                   before=before,after=after,durability="NOT_ATTESTED_UNTIL_RUN_VERIFIED")
        self._history[identity][2]=reply
        return copy.deepcopy(reply)

    def _command(self, obj):
        c.exact(obj, "action command_id sequence control value")
        c.token(obj["command_id"])
        c.integer(obj["sequence"], 1)
        c.require(obj["control"] in INPUTS, "unsupported console control")
        c.number(obj["value"], *c.ICD["controls"][obj["control"]])
        identity, shorthand = obj["command_id"], encode(obj)
        previous = self._history.get(identity)
        if previous is not None:
            if previous[0] != shorthand:
                return dict(console="DUPLICATE_CONFLICT", command_id=identity)
            return self._lookup(identity)
        # Retain the original identity even if admission fails. Never silently
        # reschedule an old ID with a later apply_tick or a new epoch.
        tick = self.engine.relative_tick + 1
        manifest = self.binding["manifest"]
        request = {k: manifest[k] for k in
                   ("schema", "session_id", "epoch", "simulated", "flight_approval")}
        request.update(kind="COMMAND", command_id=identity, sequence=obj["sequence"],
                       authority_id=self.binding["authority_id"], lease_id=self.binding["lease_id"],
                       apply_tick=tick, expires_tick=min(tick+5, self.binding["lease_until_tick"]),
                       action="SET_CONTROL", args=dict(control=obj["control"], value=obj["value"]))
        canonical = encode(request)
        size = len(shorthand) + len(canonical)
        if len(self._history) >= self.MAX_HISTORY or self._history_bytes+size > self.MAX_HISTORY_BYTES:
            self.request_stop()
            return dict(console="HISTORY_FULL_STOP_REQUESTED")
        self._history[identity] = [shorthand, None, "NOT_ADMITTED"]
        self._history_bytes += size
        if (self._stop.is_set() or self.engine.state != "RUNNING" or
                self.admission.commands.closed or self.admission.inbox.revoked):
            return self._lookup(identity)
        # Invalid expired window is retained as a failed admission, not restamped.
        if tick > self.binding["lease_until_tick"]:return self._lookup(identity)
        ingress = self.admission.offer(canonical, principal=self.binding["authority_id"])
        self._history[identity][2] = ingress
        if ingress == "QUEUED_NOT_VALIDATED":self._history[identity][1] = canonical
        return self._lookup(identity)

    def _handle(self, obj):
        c.require(isinstance(obj, dict), "console object")
        action = obj.get("action")
        if action == "SET_CONTROL":return self._command(obj)
        if action in ("PAUSE","RESUME") and self.lifecycle:return self._lifecycle(obj)
        if action == "LOOKUP":
            c.exact(obj, "action command_id");c.token(obj["command_id"])
            return self._lookup(obj["command_id"])
        c.exact(obj, "action")
        if action == "STATUS":
            return dict(console="STATUS", state=self.engine.state,
                        epoch=self.engine.manifest["epoch"], session_id=self.binding["manifest"]["session_id"],
                        relative_tick=self.engine.relative_tick, total_ticks=self.engine.total_ticks,
                        control_closed=self.admission.commands.closed,
                        history=len(self._history), history_bytes=self._history_bytes)
        # Lifecycle has no retry IDs in this V1. It is deliberately unsupported
        # rather than allowing a delayed PAUSE retry to pause a later epoch.
        return dict(console="UNSUPPORTED")

    def service(self):
        self._check_owner()
        if self._stop.is_set():
            if self.engine.state in ("RUNNING", "PAUSED", "ARMED_TEST"):
                if self._fault.is_set():self.engine._fail("CONSOLE_IO_FAILURE")
                else:self.engine.stop()
            self._latest = encode(dict(console="TERMINAL" if self.engine.state == "FAILED" else "STOPPED", state=self.engine.state,
                                       simulated=True, flight_approval=False))
            return self._latest
        if not self._lock.acquire(blocking=False):return self._latest
        try:
            batch = [self._items.popleft() for _ in range(min(len(self._items), self.MAX_PER_SERVICE))]
        finally:self._lock.release()
        replies = []
        for raw in batch:
            if self._stop.is_set():break
            try:reply = self._handle(c.decode(raw))
            except (ValueError, KeyError, TypeError):reply = dict(console="INVALID")
            replies.append(reply)
        self._latest = encode(replies)
        return self._latest

    def pump(self):
        """Runtime must call while RUNNING and PAUSED; not an autonomous watchdog."""
        self._check_owner()
        self.service()
        # A producer may latch STOP during service; do not advance physics.
        if self._stop.is_set():self.service();return False
        return self.engine.pump()

    def latest(self):
        self._check_owner()
        return self._latest
