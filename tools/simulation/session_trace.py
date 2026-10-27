"""Offline trace integrity/normalization verifier, not an independent FDM oracle."""
import argparse
import hashlib
import json
from pathlib import Path
from tools.simulation import session_contract as c
from tools.simulation.session_engine import encode
from tools.simulation.session_jsbsim import normalize
from tools.simulation.pilot_fixture import Audit


def verify(directory):
    root=Path(directory)
    result=c.decode((root/"result.json").read_bytes())
    c.require(result["status"]=="COMPLETE_SOFTWARE_ONLY" and result["simulated"] is True
              and result["flight_approval"] is False,"not a completed SIM run")
    digest=hashlib.sha256();count=frames=0;pending=None;manifest=None
    phase="NEW";offset=0;last_sequence=-1;last_mono=-1;last_total=0;pilot=None
    with (root/"truth.jsonl").open("rb") as stream:
        while True:
            raw=stream.readline(65537)
            if not raw:break
            c.require(len(raw)<=65536 and raw.endswith(b"\n"),"truncated or oversize trace record")
            digest.update(raw);count+=1
            c.require(count<=200000,"trace bound")
            r=c.decode(raw)
            if r["kind"]=="RUN_CONFIGURATION":
                c.require(count==1,"configuration order")
                manifest=c.manifest(r["manifest"])
                c.require(manifest["epoch"]==0,"initial epoch")
                c.require(hashlib.sha256(encode(r["configuration"])).hexdigest()==manifest["config_sha256"],"configuration digest")
                pilot=Audit(r["configuration"].get("pilot_fixture"),manifest)
                continue
            c.require(manifest is not None and r["session_id"]==manifest["session_id"]
                and r["simulated"] is True and r["flight_approval"] is False,"trace identity/SIM")
            if r["kind"]=="PILOT_FIXTURE":
                c.require(pending is None and r["epoch"]==0 and manifest["epoch"]==0
                    and last_total<=r["total_ticks"]<=last_total+5,"pilot frame/epoch order")
                pilot.record(r,phase)
                continue
            if r["kind"]=="PHYSICS_WORLD":
                c.require(phase=="RUNNING" and pending is None,"physics order")
                pending=r;continue
            c.require(r["kind"]=="SESSION_EVENT","unknown trace record")
            event=r["event"]
            if event=="EPOCH":
                c.require(phase=="PAUSED" and pending is None and r["epoch"]==manifest["epoch"]+1,"epoch order")
                c.require(r["total_ticks"]==last_total,"pause advanced physical clock")
                offset=r["parent_total_ticks"];c.require(offset==last_total,"epoch parent")
                manifest=dict(manifest,epoch=r["epoch"]);last_sequence=-1;phase="RESUMING"
                continue
            c.require(r["epoch"]==manifest["epoch"],"event epoch")
            if event=="FRAME":
                c.require(phase=="RUNNING" and pending is not None,"frame order")
                f=c.frame(r["frame"],manifest)
                c.require(f["sequence"]==last_sequence+1 and f["producer_mono_ns"]>=last_mono,"frame continuity")
                total=r["total_ticks"]
                c.require(total==offset+f["tick"] and pending["total_ticks"]==total
                          and pending["relative_tick"]==f["tick"] and pending["epoch"]==f["epoch"],"physical/epoch clock")
                c.require(pending["frame"]["sim_time_ms"]==total*10,"raw physical clock")
                c.require(normalize(pending["frame"],manifest,f["tick"],f["producer_mono_ns"])==f,"normalization differs")
                pending=None;frames+=1;last_sequence=f["sequence"];last_mono=f["producer_mono_ns"];last_total=total
            else:
                c.require(pending is None,"unpaired physics")
                transitions={"CREATED":("NEW","CREATED"),"VALIDATED":("CREATED","VALIDATED"),
                    "ARMED_TEST":("VALIDATED","ARMED_TEST"),"PAUSED":("RUNNING","PAUSED"),
                    "END_OF_DATA":("STOPPING","ENDED")}
                if event=="RUNNING":
                    c.require(phase in ("ARMED_TEST","RESUMING"),"run transition");phase="RUNNING"
                elif event=="STOPPING":
                    c.require(phase in ("RUNNING","PAUSED"),"stop transition");phase="STOPPING"
                else:
                    c.require(event in transitions and phase==transitions[event][0],"lifecycle transition")
                    phase=transitions[event][1]
                if event in ("PAUSED","STOPPING"):
                    c.require(r["total_ticks"]>=last_total,"physical clock reversed")
                    last_total=r["total_ticks"]
    c.require(phase=="ENDED" and pending is None,"incomplete lifecycle")
    applied=pilot.finish()
    c.require(result["session_id"]==manifest["session_id"] and result["truth_records"]==count
        and result["truth_sha256"]==digest.hexdigest() and result["frames"]==frames
        and result["total_ticks"]==last_total,"trace result digest/count")
    return dict(status="PASS",frames=frames,total_ticks=last_total,epochs=manifest["epoch"]+1,
                pilot_commands_applied=applied,
                scope="TRACE_INTEGRITY_NOT_MODEL_VALIDATION",flight_approval=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("directory",type=Path)
    args=parser.parse_args()
    try:print(json.dumps(verify(args.directory)))
    except (ValueError,OSError,KeyError,TypeError) as exc:parser.exit(2,f"Session trace FAILED: {exc}\n")


if __name__=="__main__":main()
