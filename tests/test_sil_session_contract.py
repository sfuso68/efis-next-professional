import copy
import json
import unittest
from tools.simulation import session_contract as c


class SessionContractTests(unittest.TestCase):
    def setUp(self):
        self.v = c.decode((c.ROOT / "tests/data/protocols/sil-session-v1-vectors.json").read_bytes())
        self.m = self.v["manifest"]
        self.cmd = self.v["command"]

    def validate_command(self, obj=None, **kw):
        context = dict(current_tick=0, authority_id="instructor", lease_id="lease-001", lease_until_tick=100)
        context.update(kw)
        return c.command(self.cmd if obj is None else obj, self.m, **context)

    def frame(self):
        groups = {}
        for group, signals in c.ICD["signals"].items():
            groups[group] = {}
            for name, unit in signals.items():
                value = copy.deepcopy(self.v["sample"])
                value["unit"] = unit
                groups[group][name] = value
        return dict(schema=c.SCHEMA, kind="FRAME", session_id=self.m["session_id"], epoch=0,
                    simulated=True, flight_approval=False, sequence=0, tick=0, sim_time_ns=0,
                    producer_mono_ns=100, config_sha256=self.m["config_sha256"], groups=groups,
                    products={kind:dict(status="UNAVAILABLE", sha256=None, observed_sim_ns=None,
                                        valid_until_sim_ns=None) for kind in c.ICD["product_kinds"]})

    def test_manifest_and_candidate_boundary(self):
        c.manifest(self.m)
        self.assertFalse(c.ICD["deployment_authorized"])
        self.assertEqual(c.ICD["transport"], "OFFLINE_OBJECTS_ONLY")
        self.assertEqual(len(c.ICD["signals"]["engine"]), 18)

    def test_manifest_mutations(self):
        for key, value in (("source_node",47), ("integration_hz",True), ("output_hz",50),
                           ("flight_approval",0), ("simulated",1), ("epoch",False),
                           ("config_sha256","x"), ("profile_status","APPROVED"),
                           ("epoch_utc","2026-10-26"), ("event_ids",["../../shell"]),
                           ("route_hashes",["a"*64,"a"*64])):
            with self.subTest(key=key):
                obj=copy.deepcopy(self.m); obj[key]=value
                with self.assertRaises(ValueError): c.manifest(obj)

    def test_json_duplicate_nonfinite_size_and_extra_field(self):
        for raw in ('{"a":1,"a":2}', '{"x":NaN}', '{"x":1e999}', ' '*65537):
            with self.assertRaises(ValueError): c.decode(raw)
        self.m["endpoint"]="192.168.1.47"
        with self.assertRaises(ValueError): c.manifest(self.m)

    def test_complete_frame_and_missing_channel(self):
        obj=self.frame(); c.frame(obj,self.m)
        del obj["groups"]["engine"]["egt4"]
        with self.assertRaises(ValueError): c.frame(obj,self.m)

    def test_unavailable_never_zero_and_units_checked(self):
        for key,value in (("value",0), ("raw",0), ("unit","rad"), ("source_node",47),
                          ("validity","VALID"), ("simulated",False)):
            obj=self.frame(); obj["groups"]["flight"]["roll_deg"][key]=value
            with self.assertRaises(ValueError): c.frame(obj,self.m)

    def test_clock_binding_and_age_cannot_reset(self):
        for key,value in (("session_id","other"), ("epoch",1), ("sequence",1),
                          ("tick",5), ("sim_time_ns",1), ("producer_mono_ns",-1)):
            obj=self.frame();obj[key]=value
            with self.assertRaises(ValueError): c.frame(obj,self.m)
        obj=self.frame();obj.update(sequence=1,tick=5,sim_time_ns=50000000)
        for group in obj["groups"].values():
            for s in group.values():s["age_ms"]=50
        c.frame(obj,self.m)
        obj["groups"]["flight"]["roll_deg"]["age_ms"]=0
        with self.assertRaises(ValueError): c.frame(obj,self.m)

    def test_stale_retains_sim_and_nonfinite_denied(self):
        obj=self.frame();s=obj["groups"]["flight"]["roll_deg"]
        s.update(value=2.0,raw=2.0,validity="STALE")
        c.frame(obj,self.m)
        for value in (float("nan"),float("inf"),True):
            s["value"]=value
            with self.assertRaises(ValueError): c.frame(obj,self.m)

    def test_product_reference_and_expiry(self):
        obj=self.frame();p=obj["products"]["WEATHER"]
        p.update(status="SIMULATED",sha256="d"*64,observed_sim_ns=0,valid_until_sim_ns=100)
        c.frame(obj,self.m)
        p["observed_sim_ns"]=1
        with self.assertRaises(ValueError):c.frame(obj,self.m)
        p.update(status="UNAVAILABLE",observed_sim_ns=0)
        with self.assertRaises(ValueError):c.frame(obj,self.m)

    def test_command_validation_is_not_application(self):
        result=self.validate_command()
        self.assertEqual(result["application"],"NOT_APPLIED")
        self.assertEqual(result,self.validate_command())

    def test_command_authority_epoch_and_deadlines(self):
        for key,value in (("lease_id","other"),("authority_id","other"),("epoch",1),
                          ("apply_tick",0),("apply_tick",101),("expires_tick",101),
                          ("sequence",False),("flight_approval",True)):
            obj=copy.deepcopy(self.cmd);obj[key]=value
            with self.assertRaises(ValueError):self.validate_command(obj)
        with self.assertRaises(ValueError):self.validate_command(lease_until_tick=0)

    def test_control_unknown_range_boolean_and_injection(self):
        for args in ({"control":"servo1","value":0}, {"control":"aileron","value":2},
                     {"control":"throttle","value":True}, {"control":"elevator","value":float("nan")},
                     {"control":"aileron","value":0,"shell":"id"}):
            self.cmd["args"]=args
            with self.assertRaises(ValueError):self.validate_command()
        self.cmd.update(action="MAVLINK",args={})
        with self.assertRaises(ValueError):self.validate_command()

    def test_lifecycle_nav_and_frozen_scenario_events(self):
        for action,args in (("START",{}),("STOP",{}),("PAUSE",{}),("RESUME",{}),
                            ("SET_OBS",{"degrees_magnetic":359}),
                            ("SELECT_NAV",{"station_id":"SIMVOR"}),
                            ("LOAD_ROUTE",{"sha256":"c"*64}),
                            ("SCENARIO_EVENT",{"event_id":"flap-jam"})):
            self.cmd.update(action=action,args=args);self.validate_command()
        self.cmd["args"]={"event_id":"exec-file"}
        with self.assertRaises(ValueError):self.validate_command()

    def test_ack_correlation_and_applied_tick(self):
        ack={k:self.cmd[k] for k in ("schema","session_id","epoch","simulated","flight_approval","command_id","sequence")}
        ack.update(kind="ACK",status="QUEUED",reason="NONE",applied_tick=None)
        c.acknowledgement(ack,self.cmd)
        ack.update(status="APPLIED",applied_tick=1)
        c.acknowledgement(ack,self.cmd)
        for key,value in (("command_id","other"),("sequence",True),("applied_tick",21),("reason","INVALID")):
            obj=copy.deepcopy(ack);obj[key]=value
            with self.assertRaises(ValueError):c.acknowledgement(obj,self.cmd)
        ack.update(status="REJECTED",reason="EXPIRED",applied_tick=None)
        c.acknowledgement(ack,self.cmd)

    def test_valid_measurement_and_expired_product(self):
        obj=self.frame();obj.update(sequence=1,tick=5,sim_time_ns=50000000)
        for group in obj["groups"].values():
            for s in group.values():s["age_ms"]=50
        obj["groups"]["flight"]["roll_deg"].update(value=10,raw=10,validity="SIMULATED")
        p=obj["products"]["RADAR"]
        p.update(status="SIMULATED",sha256="e"*64,observed_sim_ns=0,valid_until_sim_ns=49999999)
        with self.assertRaises(ValueError):c.frame(obj,self.m)
        p["status"]="STALE"
        c.frame(obj,self.m)


if __name__ == "__main__":
    unittest.main()
