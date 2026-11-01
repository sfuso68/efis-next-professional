"""Virtual-clock fixture composition; never an aerodynamic/timing oracle."""
import hashlib
from tools.simulation import session_contract as c
from tools.simulation.control_audit import execution_policy,FILE
from tools.simulation.pilot_fixture import INPUTS
from tools.simulation.session_control import SessionAdmission
from tools.simulation.session_engine import SessionEngine,TruthJournal,TICK_NS,encode
from tools.simulation.session_jsbsim import PhysicalAdapter
from tests.test_session_jsbsim import raw_fixture
from tests.test_sil_session_engine import Clock


class FixtureFdm:
    def __init__(self):
        self.ticks=0;self.catalog=set(INPUTS.values());self.test_controls={};self.values={}
    def set_test_control(self,control,value):self.test_controls[control]=value
    def clear_test_controls(self):self.test_controls.clear()
    def read(self,prop):return self.values.get(prop)
    def step(self,_):
        self.ticks+=1
        self.values.update({INPUTS[k]:v for k,v in self.test_controls.items()})
    def frame(self,scenario,session_id):
        raw=raw_fixture();raw.update(session_id=session_id,sim_time_ms=self.ticks*10)
        return raw


def run_live(root,backend=None,*,multi=False):
    vectors=c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())
    manifest=vectors["manifest"]
    policy=execution_policy("instructor","lease-001",100)
    config=dict(test_fixture=True,control_policy=policy)
    manifest["config_sha256"]=hashlib.sha256(encode(config)).hexdigest()
    backend=backend or FixtureFdm()
    if hasattr(backend,"model_digest"):manifest["model_sha256"]=backend.model_digest["sha256"]
    journal=TruthJournal(root)
    try:
        journal.append(dict(kind="RUN_CONFIGURATION",configuration=config,manifest=manifest))
        clock=Clock();adapter=PhysicalAdapter(backend,"cruise",journal)
        engine=SessionEngine(manifest,adapter,journal,clock=clock,max_ticks=10)
        control=SessionAdmission(manifest,principal="instructor",lease_id="lease-001",
                 lease_until_tick=100,lease_duration_ns=1_000_000_000,clock=clock)
        control.enable_execution(backend,root/FILE)
        try:
            engine.attach_admission(control)
            engine.validate();engine.arm_software_test();engine.start()
            def offer(seq,tick,name,value):
                request=dict(vectors["command"],command_id=f"live-{seq}",sequence=seq,
                     apply_tick=tick,expires_tick=tick+2,action="SET_CONTROL",
                     args=dict(control=name,value=value))
                control.offer(encode(request),principal="instructor");engine.pump()
            offer(1,1,"aileron",.25)
            for _ in range(5):clock.now+=TICK_NS;engine.pump()
            offer(2,6,"elevator",.1);offer(3,6,"rudder",.1)
            if multi:offer(4,6,"throttle",.5)
            for _ in range(2):clock.now+=TICK_NS;engine.pump()
            engine.pause();clock.now+=100_000_000;engine.pump();engine.resume()
            while engine.state=="RUNNING":clock.now+=TICK_NS;engine.pump()
            (root/"probe-report.json").write_bytes(encode(dict(status=engine.state,
                frames=engine.frames,total_ticks=engine.total_ticks,simulated=True,flight_approval=False)))
            return dict(manifest=manifest,policy=policy,metadata=control.executor.audit.metadata())
        finally:
            control.executor.audit.close()
    finally:
        journal.close()
