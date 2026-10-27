"""Owner-thread SIL command admission, not authentication or an FDM executor.

The caller must authenticate the principal independently. No sockets, hardware,
callbacks, lease renewal, or APPLIED acknowledgements are implemented here.
Dispatch yields each command once; it does not assert that physics applied it.
"""
import copy
import threading
import time

from tools.simulation import session_contract as c
from tools.simulation.session_engine import encode


class CommandQueue:
    MAX_PENDING = 16
    MAX_HISTORY = 256
    MAX_BYTES = 524288
    MAX_LEASE_NS = 5_000_000_000

    def __init__(self, manifest, *, principal, lease_id, lease_until_tick,
                 lease_duration_ns, clock=time.monotonic_ns):
        c.manifest(manifest)
        c.token(principal)
        c.token(lease_id)
        c.integer(lease_until_tick, 1)
        c.integer(lease_duration_ns, 1, self.MAX_LEASE_NS)
        self._context = copy.deepcopy(manifest)
        self._principal, self._lease_id = principal, lease_id
        self._until_tick, self._clock = lease_until_tick, clock
        self._owner = threading.get_ident()
        self._last_ns = self._last_tick = 0
        self._deadline = self._now() + lease_duration_ns
        self._history, self._pending = {}, {}
        self._bytes = self._sequence = 0
        self.closed = False

    def _owner_check(self):
        if threading.get_ident() != self._owner:
            raise RuntimeError("command admission requires owner thread")

    def _now(self):
        now = self._clock()
        c.integer(now)
        if now < self._last_ns:
            self.revoke()
            raise ValueError("command monotonic clock reversed")
        self._last_ns = now
        return now

    def _advance(self, tick):
        self._owner_check()
        try:
            c.integer(tick, self._last_tick)
        except ValueError:
            self.revoke()
            raise
        self._last_tick = tick
        now = self._now()
        if now >= self._deadline or tick > self._until_tick:
            self.revoke()
        for identity, request in list(self._pending.items()):
            if tick > request["expires_tick"]:
                self._reject_pending(identity, "EXPIRED")

    @staticmethod
    def _ack(request, reason="NONE"):
        ack = {k: request[k] for k in ("schema", "session_id", "epoch", "simulated",
                                      "flight_approval", "command_id", "sequence")}
        ack.update(kind="ACK", status="QUEUED" if reason == "NONE" else "REJECTED",
                   reason=reason, applied_tick=None)
        return c.acknowledgement(ack, request)

    def _reject_pending(self, identity, reason):
        request = self._pending.pop(identity)
        self._history[identity][1] = self._ack(request, reason)

    def revoke(self):
        """Independent of queue capacity; no future admission after revocation."""
        self._owner_check()
        self.closed = True
        for identity in list(self._pending):
            self._reject_pending(identity, "NO_LEASE")

    def submit(self, raw, *, authenticated_principal, current_tick):
        """Strict JSON only. Returns QUEUED/REJECTED, never APPLIED.

        Principal is trusted caller context, not a credential in the command.
        Wrong principal and malformed requests raise without allocating history.
        Capacity exhaustion fails the entire admission queue closed; no eviction.
        """
        self._owner_check()
        c.require(authenticated_principal == self._principal, "unauthenticated principal")
        self._advance(current_tick)
        request = c.decode(raw)
        # Structural validation first, with a synthetic admission tick for old
        # identical retries. Actual horizon/lease checks follow for NEW requests.
        c.require(isinstance(request, dict), "command object")
        apply_tick = request.get("apply_tick")
        c.integer(apply_tick, 1)
        c.command(request, self._context, current_tick=apply_tick-1,
                  authority_id=self._principal, lease_id=self._lease_id,
                  lease_until_tick=max(self._until_tick, apply_tick+100))
        canonical = encode(request)
        identity = request["command_id"]
        if identity in self._history:
            previous, ack = self._history[identity]
            return copy.deepcopy(ack) if previous == canonical else self._ack(request, "DUPLICATE_CONFLICT")
        if len(self._history) >= self.MAX_HISTORY or self._bytes+len(canonical) > self.MAX_BYTES:
            self.revoke()
            raise ValueError("command history capacity exhausted; queue revoked")
        reason = "NONE"
        if self.closed:
            reason = "NO_LEASE"
        elif request["sequence"] <= self._sequence:
            reason = "INVALID"
        else:
            try:
                c.command(request, self._context, current_tick=current_tick,
                          authority_id=self._principal, lease_id=self._lease_id,
                          lease_until_tick=self._until_tick)
            except ValueError:
                reason = "EXPIRED" if request["apply_tick"] <= current_tick else "INVALID"
        if reason == "NONE" and len(self._pending) >= self.MAX_PENDING:
            reason = "INVALID"  # Frozen ICD has no QUEUE_FULL reason.
        ack = self._ack(request, reason)
        self._history[identity] = [canonical, ack]
        self._bytes += len(canonical)
        if reason == "NONE":
            self._sequence = request["sequence"]
            self._pending[identity] = request
        return copy.deepcopy(ack)

    def take_due(self, *, current_tick):
        """Immutable bytes handed off at most once. NOT an execution receipt.

        The future executor must check model capabilities, audit intent/result,
        and produce its own APPLIED evidence. Lifecycle/pause is not handled by
        this queue. A missed apply tick is allowed only through expires_tick.
        """
        self._advance(current_tick)
        due = sorted((r for r in self._pending.values() if r["apply_tick"] <= current_tick),
                     key=lambda r: (r["apply_tick"], r["sequence"]))
        for request in due:
            del self._pending[request["command_id"]]
        return tuple(encode(r) for r in due)

    @property
    def counts(self):
        self._owner_check()
        return dict(pending=len(self._pending), history=len(self._history), bytes=self._bytes)
