"""Local lifecycle composition with fixture clock; NOT a timing oracle."""
import hashlib
from tools.simulation import session_contract as c
from tools.simulation.control_audit import execution_policy, FILE
from tools.simulation.console_lifecycle import MODE
from tools.simulation.local_console import LocalConsole
from tools.simulation.session_control import SessionAdmission
from tools.simulation.session_engine import SessionEngine, TruthJournal, TICK_NS, encode
from tools.simulation.session_jsbsim import PhysicalAdapter
from tests.live_control_fixture import FixtureFdm
from tests.test_sil_session_engine import Clock


def run_lifecycle(root, backend=None):
    manifest=c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())["manifest"]
    config=dict(test_fixture=True,control_policy=execution_policy("instructor","lease-001",100),
                console_policy=dict(lifecycle=MODE))
    manifest["config_sha256"]=hashlib.sha256(encode(config)).hexdigest()
    backend=backend or FixtureFdm()
    if hasattr(backend,"model_digest"):manifest["model_sha256"]=backend.model_digest["sha256"]
    journal=TruthJournal(root);control=None
    try:
        journal.append(dict(kind="RUN_CONFIGURATION",configuration=config,manifest=manifest))
        clock=Clock();adapter=PhysicalAdapter(backend,"cruise",journal)
        engine=SessionEngine(manifest,adapter,journal,clock=clock,max_ticks=10)
        control=SessionAdmission(manifest,principal="instructor",lease_id="lease-001",
                 lease_until_tick=100,lease_duration_ns=1_000_000_000,clock=clock)
        control.enable_execution(backend,root/FILE)
        engine.attach_admission(control);engine.validate();engine.arm_software_test();engine.start()
        console=LocalConsole(engine,control,lifecycle=True)
        def send(obj):console.offer(encode(obj));console.pump()
        send(dict(action="SET_CONTROL",command_id="input-1",sequence=1,control="aileron",value=.2))
        for _ in range(5):clock.now+=TICK_NS;console.pump()
        pause=dict(action="PAUSE",command_id="pause-1",session_id=manifest["session_id"],expected_epoch=0)
        resume=dict(pause,action="RESUME",command_id="resume-1")
        send(pause);send(pause)
        clock.now+=100_000_000;console.pump()
        send(resume);send(pause);send(resume)
        send(dict(pause,command_id="stale-pause"))
        while engine.state=="RUNNING":clock.now+=TICK_NS;console.pump()
        (root/"probe-report.json").write_bytes(encode(dict(status=engine.state,frames=engine.frames,
                         total_ticks=engine.total_ticks,simulated=True,flight_approval=False)))
        return manifest
    finally:
        if control is not None and control.executor is not None:control.executor.audit.close()
        journal.close()
