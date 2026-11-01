"""No physical devices, network or pymavlink required; injected tripwires."""
import argparse
import ast
import contextlib
import importlib.util
import io
import json
from pathlib import Path
from types import ModuleType,SimpleNamespace
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]


def load(name):
    fake=ModuleType("pymavlink");fake.mavutil=SimpleNamespace()
    dialects=ModuleType("pymavlink.dialects");v20=ModuleType("pymavlink.dialects.v20")
    class Dialect:
        def __getattr__(self,_):return 1
    v20.ardupilotmega=Dialect()
    modules={"pymavlink":fake,"pymavlink.dialects":dialects,"pymavlink.dialects.v20":v20}
    with patch.dict("sys.modules",modules):
        spec=importlib.util.spec_from_file_location(name,ROOT/"tools/hardware"/(name+".py"))
        result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result)
    return result


class PassiveLink:
    def __init__(self,messages=()):self.messages=list(messages);self.target_system=1;self.target_component=1
    @property
    def mav(self):raise AssertionError("outgoing MAVLink API touched")
    def recv_match(self,**_):return self.messages.pop(0) if self.messages else None
    def wait_heartbeat(self,**_):
        return SimpleNamespace(type=1,autopilot=1,base_mode=128,custom_mode=0,system_status=4)


class Clock:
    def __init__(self):self.t=0
    def __call__(self):self.t+=.01;return self.t


class ToolBoundaryTests(unittest.TestCase):
    def test_audit_passive_with_armed_heartbeat_and_missing_version(self):
        module=load("audit_pixhawk");link=PassiveLink()
        module.mavutil=SimpleNamespace(mavlink_connection=lambda *a,**k:link,mavlink=SimpleNamespace(enums={}))
        with patch("sys.argv",["audit","--timeout",".1"]),patch.object(module.time,"monotonic",Clock()), \
             contextlib.redirect_stdout(io.StringIO()) as out:module.main()
        result=json.loads(out.getvalue())
        self.assertEqual(result["transmitted_messages"],[])
        self.assertEqual(result["version_status"],"NOT_OBSERVED")
        self.assertNotIn("version",result)

    def test_audit_unsolicited_version_is_observed_without_request(self):
        version=SimpleNamespace(get_type=lambda:"AUTOPILOT_VERSION",flight_sw_version=1,middleware_sw_version=1,
            os_sw_version=1,board_version=1,vendor_id=1,product_id=1,uid=1,uid2=b'a',
            capabilities=1,flight_custom_version=b'b')
        module=load("audit_pixhawk");link=PassiveLink([version])
        module.mavutil=SimpleNamespace(mavlink_connection=lambda *a,**k:link,mavlink=SimpleNamespace(enums={}))
        with patch("sys.argv",["audit","--timeout",".1"]),patch.object(module.time,"monotonic",Clock()), \
             contextlib.redirect_stdout(io.StringIO()) as out:module.main()
        self.assertEqual(json.loads(out.getvalue())["version_status"],"OBSERVED")

    def test_backup_firmware_wait_never_uses_mav_send(self):
        module=load("backup_parameters")
        with patch.object(module.time,"monotonic",Clock()):
            self.assertIsNone(module.firmware_of(PassiveLink(),.1))

    def test_backup_collect_only_requests_parameter_list(self):
        module=load("backup_parameters");calls=[]
        class Mav:
            def param_request_list_send(self,*args):calls.append(args)
            def __getattr__(self,name):raise AssertionError(name)
        link=SimpleNamespace(mav=Mav(),target_system=1,target_component=1,
            recv_match=lambda **_:SimpleNamespace(param_id="ARSPD_USE",param_value=0,param_type=1,param_index=0,param_count=1))
        result=module.collect(link,1,.1)
        self.assertEqual(calls,[(1,1)]);self.assertEqual(result[0],{"ARSPD_USE":0.0})

    def test_backup_manifest_explicitly_reports_active_query_and_missing_version(self):
        import tempfile
        module=load("backup_parameters");calls=[]
        class Mav:
            def param_request_list_send(self,*args):calls.append(args)
            def __getattr__(self,name):raise AssertionError(name)
        link=SimpleNamespace(mav=Mav(),target_system=1,target_component=1,wait_heartbeat=lambda **_:True,
            recv_match=lambda **_:SimpleNamespace(param_id="ARSPD_USE",param_value=0,param_type=1,param_index=0,param_count=1))
        module.mavutil=SimpleNamespace(mavlink_connection=lambda *a,**k:link)
        with tempfile.TemporaryDirectory() as directory,patch("sys.argv",["backup","--device","fixture",
             "--output-dir",directory]),patch.object(module,"firmware_of",return_value=None), \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(module.main(),0)
            manifest=json.loads(next(Path(directory).glob("*/manifest.json")).read_text(encoding="utf-8"))
        self.assertEqual(manifest["access_mode"],"ACTIVE_PARAMETER_QUERY_NON_MUTATING")
        self.assertEqual(manifest["transmitted_message_types"],["PARAM_REQUEST_LIST"])
        self.assertFalse(manifest["observer_path_compatible"])
        self.assertEqual(manifest["firmware_status"],"NOT_OBSERVED")
        self.assertEqual(calls,[(1,1)])

    def test_parameter_receive_retains_parser_without_transmission_or_cache(self):
        module=load("parameter_reads")
        messages=[SimpleNamespace(param_id=b"OTHER\0",param_value=99,param_type=1),
                  SimpleNamespace(param_id=b"ARSPD_USE\0",param_value=0,param_type=1),
                  SimpleNamespace(param_id="ARSPD_USE",param_value=1,param_type=1)]
        link=PassiveLink(messages)
        self.assertEqual(module.receive_parameter(link,"ARSPD_USE"),(0.,1))
        self.assertEqual(module.receive_parameter(link,"ARSPD_USE"),(1.,1))

    def test_parameter_receive_known_typeerror_bounded_timeout_and_invalid_values(self):
        module=load("parameter_reads")
        class Broken(PassiveLink):
            def recv_match(self,**_):raise TypeError(module.KNOWN_MULTI_INSTANCE_ERROR)
        with patch.object(module.time,"monotonic",Clock()):
            self.assertIsNone(module.receive_match(Broken(),message_type="HEARTBEAT",timeout=.1))
        for name in ("", "a", "X"*17,"ARSPD_USE\0"):
            with self.assertRaises(ValueError):module.parameter_id(name)
        with self.assertRaises(module.ParameterReadError):
            module.receive_parameter(PassiveLink([SimpleNamespace(param_id="ARSPD_USE",param_value=float("nan"),param_type=1)]),"ARSPD_USE")

    def test_unexpected_receive_error_is_not_hidden(self):
        module=load("parameter_reads")
        class Broken(PassiveLink):
            def recv_match(self,**_):raise TypeError("different")
        with self.assertRaisesRegex(TypeError,"different"):
            module.receive_match(Broken(),message_type="HEARTBEAT",timeout=.1)

    def test_fixture_endpoint_only_literal_loopback_udp(self):
        module=load("mavlink_parameter_stub")
        self.assertEqual(module.loopback_endpoint("udpout:127.0.0.1:14561"),"udpout:127.0.0.1:14561")
        for endpoint in ("udpout:10.77.10.3:14550","udpin:0.0.0.0:14550","tcp:127.0.0.1:14550",
                         "udpout:localhost:14550","/dev/ttyACM0","udpout:127.0.0.1:65536"):
            with self.assertRaises(argparse.ArgumentTypeError):module.loopback_endpoint(endpoint)

    def test_stub_rejects_remote_endpoint_before_connection_factory(self):
        module=load("mavlink_parameter_stub")
        module.mavutil=SimpleNamespace(mavlink_connection=lambda *a,**k:(_ for _ in ()).throw(AssertionError("connection")))
        with patch("sys.argv",["stub","--endpoint","udpout:10.77.10.3:14550"]), \
             contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit) as failure:
            module.main()
        self.assertEqual(failure.exception.code,2)

    def test_scoped_source_send_allowlist(self):
        allowed={"set_mavlink_parameter":set(),"audit_pixhawk":set(),
                 "backup_parameters":{"param_request_list_send"},"parameter_reads":set()}
        for name,expected in allowed.items():
            tree=ast.parse((ROOT/"tools/hardware"/(name+".py")).read_text(encoding="utf-8"))
            sends={node.func.attr for node in ast.walk(tree) if isinstance(node,ast.Call)
                   and isinstance(node.func,ast.Attribute) and (node.func.attr.endswith("_send") or node.func.attr=="send")}
            self.assertEqual(sends,expected,name)


if __name__=="__main__":unittest.main()
