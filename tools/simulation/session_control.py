"""Owner-serviced SIL admission, not a live executor or network endpoint.

One lease per engine; a pause/epoch/terminal boundary closes it permanently.
The immutable latest reply batch is diagnostic, not a durable audit. Callers
must poll retained outcomes for lost replies; no consumer callback is invoked.
"""
import copy
import threading

from tools.simulation import session_contract as c
from tools.simulation.command_queue import CommandQueue
from tools.simulation.control_inbox import ControlInbox
from tools.simulation.session_engine import encode


class SessionAdmission:
    def __init__(self, manifest, **queue_options):
        c.manifest(manifest)
        self._manifest = copy.deepcopy(manifest)
        self._owner = threading.get_ident()
        self._bound = False
        self.commands = CommandQueue(manifest, **queue_options)
        self.inbox = ControlInbox()
        self._latest = b"[]\n"
        self.executor = None

    def enable_execution(self, backend, audit_path):
        """Explicit local SIL opt-in before binding; no transport capability."""
        from tools.simulation.control_execution import ControlExecutor
        self._owner_check()
        c.require(not self._bound and self.executor is None, "executor attachment boundary")
        self.executor = ControlExecutor(self, backend, audit_path)

    def _owner_check(self):
        if threading.get_ident() != self._owner:
            raise RuntimeError("session admission requires owner")

    def bind(self, manifest, backend=None):
        self._owner_check()
        c.require(not self._bound and manifest == self._manifest, "admission binding")
        if self.executor is not None:
            c.require(getattr(backend, "backend", backend) is self.executor.backend,
                      "executor FDM binding")
            c.require(getattr(backend, "pilot", None) is None, "conflicting pilot fixture")
        self._bound = True

    def offer(self, raw, *, principal):
        """Producer ingress only. Principal must come from trusted caller context."""
        return self.inbox.offer(raw, principal=principal)

    def signal_revoke(self):
        """Cross-thread latch. Owner applies it at its next service opportunity."""
        self.inbox.revoke()

    def revoke(self):
        self._owner_check()
        self.inbox.revoke()
        if self.executor is not None:self.executor.invalidate_inflight()
        self.commands.revoke()
        if self.executor is not None:self.executor.release()

    def service(self, *, current_tick, epoch):
        self._owner_check()
        c.require(self._bound, "unbound session admission")
        c.integer(epoch)
        if epoch != self._manifest["epoch"]:
            self.revoke()
        replies = self.inbox.service(self.commands, current_tick=current_tick)
        # At most MAX_PER_SERVICE responses, overwritten rather than accumulated.
        self._latest = encode(replies)
        if self.commands.closed and self.executor is not None:self.executor.release()

    def before_step(self, tick, guard):
        self._owner_check()
        if self.executor is not None:self.executor.before_step(tick, guard)

    def after_step(self, tick):
        self._owner_check()
        if self.executor is not None:
            self.executor.after_step(tick)
            # Producer revoke arriving during physics must preserve known
            # APPLIED outcomes before releasing authority/overrides.
            if self.inbox.revoked:self.revoke()

    def authorize_step(self, tick):
        self._owner_check()
        return self.executor.authorize_step(tick) if self.executor is not None else False

    def physical_step_started(self):
        self._owner_check()
        if self.executor is not None:self.executor.physical_step_started()

    def latest_replies(self):
        """Owner-only immutable snapshot, not execution/durability evidence."""
        self._owner_check()
        return self._latest

    def lookup_outcome(self, raw, *, principal):
        self._owner_check()
        return self.inbox.lookup_outcome(self.commands, raw, principal=principal)
