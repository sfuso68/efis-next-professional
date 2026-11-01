import copy
import unittest
from tools.simulation import session_contract as c
from tools.simulation.session_jsbsim import normalize, mapped_sample, FLIGHT


def raw_fixture():
    def datum(unit,value=1):return dict(value=value,raw=value,unit=unit,validity="SIMULATED",simulated=True)
    pools={"flight":{},"sensors":{},"gnss":{}}
    for pool,key,unit in FLIGHT.values():pools[pool][key]=datum(unit)
    return dict(schema="fuso-avionics.physics-sil.v1",session_id="fixture-sil-001",backend="JSBSim-1.3.3",
        model="c172p",profile_status="REFERENCE_UNVALIDATED",simulated=True,flight_approval=False,
        flight=pools["flight"],world=dict(sensors=pools["sensors"],navigation=dict(gnss=pools["gnss"])),
        engine={ {"fuel_left_gal":"left_gal","fuel_right_gal":"right_gal"}.get(name,name):datum(unit)
                 for name,unit in c.ICD["signals"]["engine"].items()},airframe=dict(flap_measured_deg=datum("deg")))


class AdapterTests(unittest.TestCase):
    def setUp(self):
        self.m=c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())["manifest"]
        self.raw=raw_fixture()

    def test_explicit_mapping_and_unknowns(self):
        f=normalize(self.raw,self.m,0,0)
        self.assertEqual(f["groups"]["engine"]["fuel_left_gal"]["value"],1)
        for signal in ("static_hpa","differential_pa"):
            self.assertIsNone(f["groups"]["flight"][signal]["value"])
        self.assertIsNone(f["groups"]["airframe"]["gear_down_locked"]["value"])
        self.assertTrue(all(p["status"]=="UNAVAILABLE" for p in f["products"].values()))

    def test_gnss_fault_not_replaced_with_truth(self):
        for datum in self.raw["world"]["navigation"]["gnss"].values():datum.update(value=None,raw=None,validity="UNAVAILABLE")
        f=normalize(self.raw,self.m,0,0)
        self.assertIsNone(f["groups"]["flight"]["latitude_deg"]["value"])
        self.assertEqual(f["groups"]["flight"]["roll_deg"]["value"],1)

    def test_unit_origin_and_approval_mutations(self):
        for key,value in (("model","rotax"),("simulated",False),("flight_approval",True),("session_id","wrong")):
            raw=copy.deepcopy(self.raw);raw[key]=value
            with self.assertRaises(ValueError):normalize(raw,self.m,0,0)
        self.raw["flight"]["roll_deg"]["unit"]="rad"
        with self.assertRaises(ValueError):normalize(self.raw,self.m,0,0)

    def test_no_silent_value_or_raw_repair(self):
        source=dict(unit="kt",simulated=True,validity="UNAVAILABLE",value=10,raw=None)
        with self.assertRaises(ValueError):mapped_sample(source,"kt","kt",1,0)
        source.update(validity="SIMULATED",value=float("nan"),raw=1)
        with self.assertRaises(ValueError):mapped_sample(source,"kt","kt",1,0)


if __name__=="__main__":unittest.main()
