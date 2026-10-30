"""Bounded cross-thread SIL ingress. NO network authentication or actuator API.

Producer threads may offer immutable payloads with a trusted transport principal
or signal revoke. Only the owner services CommandQueue. A future executor must
recheck authorize_dispatch immediately before mutating its FDM inputs.
"""
from collections import deque
import hashlib
import threading
from tools.simulation import session_contract as c


class ControlInbox:
    MAX_MESSAGES = 16
    MAX_BYTES = 262144
    MAX_MESSAGE_BYTES = 65536
    MAX_PER_SERVICE = 4

    def __init__(self):
        self._owner = threading.get_ident()
        self._lock = threading.Lock()
        self._items = deque()
        self._bytes = 0
        self._revoked = threading.Event()

    def revoke(self):
        """Separate coalescing latch; never consumes ordinary FIFO capacity."""
        self._revoked.set()

    def offer(self, raw, *, principal):
        c.require(type(raw) is bytes and 1 <= len(raw) <= self.MAX_MESSAGE_BYTES,
                  "immutable bounded command bytes required")
        c.token(principal)
        if self._revoked.is_set():return "REVOKED"
        if not self._lock.acquire(blocking=False):return "BUSY"
        try:
            if self._revoked.is_set():return "REVOKED"
            if len(self._items)>=self.MAX_MESSAGES or self._bytes+len(raw)>self.MAX_BYTES:
                return "FULL"
            self._items.append((principal,raw));self._bytes+=len(raw)
            return "QUEUED_NOT_VALIDATED"
        finally:self._lock.release()

    def authorize_dispatch(self, commands, request, *, current_tick):
        """Owner rechecks latch as well as lease just before FDM mutation."""
        if threading.get_ident()!=self._owner:raise RuntimeError("dispatch check requires owner")
        if self._revoked.is_set():
            commands.revoke();return False
        return commands.authorize_dispatch(request,current_tick=current_tick)

    def lookup_outcome(self, commands, raw, *, principal):
        """Owner-side retry query independent of ingress revocation.

        Not a producer queue or endpoint. Future transports must arrange owner
        service for bounded lookup even after revocation.
        """
        if threading.get_ident()!=self._owner:raise RuntimeError("lookup requires owner")
        c.require(type(raw) is bytes and 1 <= len(raw) <= self.MAX_MESSAGE_BYTES,
                  "immutable bounded command bytes required")
        return commands.lookup_outcome(raw,authenticated_principal=principal)

    def service(self, commands, *, current_tick):
        """Owner-side admission budget, also callable while physics is paused.

        Results are returned to the caller, not buffered without bound. Local
        FULL/BUSY are ingress diagnostics, not new frozen ICD ACK enums.
        This function does NOT dispatch, step, execute or journal commands.
        """
        if threading.get_ident()!=self._owner:raise RuntimeError("inbox service requires owner")
        if self._revoked.is_set():
            commands.revoke();return ({"ingress":"REVOKED"},)
        commands.maintain(current_tick=current_tick)
        if commands.closed:
            self.revoke();return ({"ingress":"NO_LEASE"},)
        if not self._lock.acquire(blocking=False):return ({"ingress":"BUSY"},)
        try:
            batch=[]
            for _ in range(min(len(self._items),self.MAX_PER_SERVICE)):
                principal,raw=self._items.popleft();self._bytes-=len(raw)
                batch.append((principal,raw))
        finally:self._lock.release()
        responses=[]
        for principal,raw in batch:
            if self._revoked.is_set():
                commands.revoke();break
            try:
                ack=commands.submit(raw,authenticated_principal=principal,current_tick=current_tick)
                responses.append(dict(ingress="VALIDATED",ack=ack))
            except (ValueError,KeyError,TypeError,UnicodeError):
                responses.append(dict(ingress="REJECTED_INVALID",payload_sha256=hashlib.sha256(raw).hexdigest()))
            if commands.closed:
                self.revoke();break
        if self._revoked.is_set():commands.revoke()
        return tuple(responses)
