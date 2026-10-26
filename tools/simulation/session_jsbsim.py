"""Explicit real-JSBSim adapter to candidate SIL session frames. No sockets.

Raw physics/world frames stay in the test journal. Missing product pipelines,
per-cylinder channels and pressure sensors remain UNAVAILABLE, never fabricated.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import time

from tools.simulation import session_contract as c
from tools.simulation.jsbsim_session import Backend, SCENARIOS, VERSION
from tools.simulation.session_engine import SessionEngine, TruthJournal, TICK_NS, encode
from tools.simulation.sil_world import config_digest

FLIGHT = {"roll_deg": ("flight", "roll_deg", "deg"),
    "pitch_deg": ("flight", "pitch_deg", "deg"),
    "heading_true_deg": ("flight", "heading_true_deg", "deg"),
    "cas_kt": ("flight", "cas_kt", "kt"), "tas_kt": ("flight", "tas_kt", "kt"),
    "geometric_msl_ft": ("flight", "altitude_msl_ft", "ft"),
    "vs_fpm": ("flight", "vs_fpm", "ft/min"),
    "ias_kt": ("sensors", "ias_kt", "kt"),
    "pressure_altitude_ft": ("sensors", "pressure_altitude_ft", "ft"),
    "indicated_altitude_ft": ("sensors", "indicated_altitude_ft", "ft"),
    "heading_magnetic_deg": ("sensors", "heading_mag_deg", "degMAG"),
    "latitude_deg": ("gnss", "lat", "deg"), "longitude_deg": ("gnss", "lon", "deg"),
    "gs_kt": ("gnss", "gs_kt", "kt"), "track_true_deg": ("gnss", "track_true_deg", "degTRUE")}


def mapped_sample(source, expected_unit, target_unit, channel, tick):
    sample = dict(value=None, raw=None, unit=target_unit, validity="UNAVAILABLE", simulated=True,
        source_node=65000, channel=channel, sample_tick=tick, age_ms=0, quality=0,
        calibration_revision="sim-reference", profile_revision="c172p-reference-v1")
    if source is not None:
        c.require(source["unit"] == expected_unit and source["simulated"] is True, "adapter source unit/SIM")
        c.require(source["validity"] in ("SIMULATED", "UNAVAILABLE"), "adapter source validity")
        if source["validity"] == "SIMULATED":
            sample.update(value=source["value"],raw=source.get("raw",source["value"]),
                          validity="SIMULATED",quality=1)
        else:
            c.require(source["value"] is None and source.get("raw") is None, "unavailable source value")
    c.sample(sample,target_unit,tick)
    return sample


def normalize(raw, manifest, tick, mono_ns):
    c.require(raw["schema"] == "fuso-avionics.physics-sil.v1"
        and raw["backend"] == "JSBSim-"+VERSION and raw["model"] == "c172p"
        and raw["profile_status"] == "REFERENCE_UNVALIDATED"
        and raw["session_id"] == manifest["session_id"]
        and raw["simulated"] is True and raw["flight_approval"] is False, "physics binding")
    pools = {"flight":raw["flight"], "sensors":raw["world"]["sensors"],
             "gnss":raw["world"]["navigation"]["gnss"]}
    groups = {}
    for offset,(group,signals) in enumerate(c.ICD["signals"].items()):
        groups[group] = {}
        for channel,(name,unit) in enumerate(signals.items(), offset*100):
            source=None;expected=unit
            if group == "flight" and name in FLIGHT:
                pool,key,expected=FLIGHT[name];source=pools[pool][key]
            elif group == "engine":
                key={"fuel_left_gal":"left_gal","fuel_right_gal":"right_gal"}.get(name,name)
                source=raw["engine"][key]
            elif group == "airframe" and name == "flap_position_deg":
                source=raw["airframe"]["flap_measured_deg"]
            groups[group][name]=mapped_sample(source,expected,unit,channel,tick)
    result=dict(schema=c.SCHEMA,kind="FRAME",session_id=manifest["session_id"],epoch=manifest["epoch"],
        simulated=True,flight_approval=False,sequence=tick//5,tick=tick,sim_time_ns=tick*TICK_NS,
        producer_mono_ns=mono_ns,config_sha256=manifest["config_sha256"],groups=groups,
        products={name:dict(status="UNAVAILABLE",sha256=None,observed_sim_ns=None,valid_until_sim_ns=None)
                  for name in c.ICD["product_kinds"]})
    return c.frame(result,manifest)


class PhysicalAdapter:
    def __init__(self, backend, scenario, journal):
        if scenario not in SCENARIOS:raise ValueError("scenario")
        self.backend,self.scenario,self.journal=backend,scenario,journal
        self.frames=0

    def step(self):self.backend.step(self.scenario)

    def frame(self, manifest, tick, mono_ns):
        raw=self.backend.frame(self.scenario,manifest["session_id"])
        c.require(raw["sim_time_ms"] == self.backend.ticks*10, "physical clock")
        value=normalize(raw,manifest,tick,mono_ns)
        self.journal.append(dict(kind="PHYSICS_WORLD",session_id=manifest["session_id"],epoch=manifest["epoch"],
                                 total_ticks=self.backend.ticks,relative_tick=tick,frame=raw,
                                 simulated=True,flight_approval=False))
        self.frames+=1
        return value


def run(directory, world, *, session_id, source_commit, scenario="turn", seconds=3, pause_tick=None,
        journal_mode="sync"):
    c.token(session_id)
    c.integer(seconds,1,60)
    if pause_tick is not None:c.integer(pause_tick,1,seconds*100-1)
    config_digest(world)
    c.require(journal_mode in ("sync","async-test"),"unknown journal mode")
    from tools.simulation.async_journal import AsyncTruthJournal
    backend=Backend(copy.deepcopy(world))
    configuration=dict(world=world,scenario=scenario,seconds=seconds,pause_tick=pause_tick,
                       backend="JSBSim-"+VERSION,model="c172p",
                       recording_policy=copy.deepcopy(AsyncTruthJournal.policy) if journal_mode=="async-test"
                                        else dict(mode="SYNC_PER_RECORD"))
    manifest=dict(schema=c.SCHEMA,kind="MANIFEST",session_id=session_id,epoch=0,simulated=True,
        flight_approval=False,mode="LIVE",epoch_utc=world["epoch_utc"],integration_hz=100,output_hz=20,
        source_node=65000,config_sha256=hashlib.sha256(encode(configuration)).hexdigest(),
        model_sha256=backend.model_digest["sha256"],source_commit=source_commit,
        profile_status="REFERENCE_UNVALIDATED",event_ids=[],station_ids=[],route_hashes=[])
    c.manifest(manifest)
    journal=(AsyncTruthJournal if journal_mode=="async-test" else TruthJournal)(directory)
    try:
        journal.append(dict(kind="RUN_CONFIGURATION",configuration=configuration,manifest=manifest))
        adapter=PhysicalAdapter(backend,scenario,journal)
        engine=SessionEngine(manifest,adapter,journal,max_ticks=seconds*100)
        for name in ("fast_probe","slow_probe","absent_probe"):engine.register(name)
        engine.validate();engine.arm_software_test();engine.start()
        received=0;paused=False;started=time.monotonic()
        while engine.state == "RUNNING":
            if pause_tick is not None and not paused and engine.total_ticks >= pause_tick:
                engine.pause()
                pause_end=time.monotonic()+.1
                while time.monotonic()<pause_end:
                    if journal_mode=="async-test":journal.check()
                    time.sleep(.001)
                engine.resume();paused=True
            if engine.poll("fast_probe") is not None:received+=1
            if not engine.pump():time.sleep(max(0,engine.next_deadline-time.monotonic_ns())/1e9)
        summary=dict(status=engine.state,frames=engine.frames,total_ticks=engine.total_ticks,
            elapsed_s=time.monotonic()-started,maximum_lag_ms=engine.maximum_lag_ns/1e6,
            fast_probe_receipts=received,slow_overwritten=engine.slots["slow_probe"].overwritten,
            epoch=engine.manifest["epoch"],backend="JSBSim-"+VERSION,simulated=True,
            flight_approval=False,network_publisher=False,actual_display_connected=False)
        summary["journal_mode"]=journal_mode
        if journal_mode=="async-test":
            summary.update(maximum_pending_records=journal.maximum_pending,
                maximum_observed_ack_age_ms=journal.maximum_ack_age_ms,
                durable_records=journal.durable)
        with (Path(directory)/"probe-report.json").open("xb") as stream:stream.write(encode(summary))
        return summary
    finally:
        journal.close()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory",required=True,type=Path)
    parser.add_argument("--world",required=True,type=Path)
    parser.add_argument("--session-id",required=True)
    parser.add_argument("--source-commit",required=True)
    parser.add_argument("--scenario",choices=SCENARIOS,default="turn")
    parser.add_argument("--seconds",type=int,default=3)
    parser.add_argument("--pause-tick",type=int)
    parser.add_argument("--journal-mode",choices=("sync","async-test"),default="sync")
    args=parser.parse_args()
    try:
        c.require(args.world.stat().st_size<=65536,"world size")
        print(json.dumps(run(args.directory,c.decode(args.world.read_bytes()),session_id=args.session_id,
            source_commit=args.source_commit,scenario=args.scenario,seconds=args.seconds,
            pause_tick=args.pause_tick,journal_mode=args.journal_mode)))
    except (ValueError,OSError,RuntimeError,ImportError) as exc:parser.exit(2,f"Session JSBSim FAILED: {exc}\n")


if __name__=="__main__":main()
