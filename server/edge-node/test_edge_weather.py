import json, socket, tempfile, time, unittest
from pathlib import Path
import edge_weather

class EdgeWeatherTest(unittest.TestCase):
    def test_missing_station_is_explicit_no_data(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(edge_weather.cached(directory,"LIPH")["state"],"NO_DATA")
    def test_cached_station_exposes_age_and_source(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory,"LIPH.json").write_text(json.dumps({"provider_payload":{"metar":"TEST"}}))
            item=edge_weather.cached(directory,"LIPH")
            self.assertEqual(item["state"],"AVAILABLE"); self.assertEqual(item["edge_source"],"CENTRAL_CACHE")
    def test_pinned_resolver_preserves_named_host_policy(self):
        mapping=edge_weather.parse_resolve("mappe.casa.lan=192.168.1.6")
        with edge_weather.temporary_resolver(mapping):
            result=socket.getaddrinfo("mappe.casa.lan",443,type=socket.SOCK_STREAM)
        self.assertTrue(result); self.assertEqual(result[0][4][0],"192.168.1.6")
    def test_invalid_resolver_is_rejected(self):
        with self.assertRaises(ValueError): edge_weather.parse_resolve("mappe.casa.lan=not-an-ip")
    def test_station_without_bulletin_is_not_reported_available(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory,"LIDB.json").write_text(json.dumps({"provider_payload":{"metar":None,"taf":None}}))
            self.assertEqual(edge_weather.cached(directory,"LIDB")["state"],"NO_REPORT")

if __name__=="__main__": unittest.main()
