"""Passive parameter decoding/receive helpers, no connection factory or TX.

A trusted caller supplies received messages/link. No query or setter is
emitted. Distinct observations are NOT proof of an independent active read.
"""
import math
import time

KNOWN_MULTI_INSTANCE_ERROR = "'NoneType' object does not support item assignment"


class ParameterReadError(RuntimeError):
    pass


def parameter_id(name):
    if not isinstance(name,str):raise ValueError("parameter name")
    encoded=name.encode("ascii")
    if not 1<=len(encoded)<=16 or any(not (c==95 or 48<=c<=57 or 65<=c<=90) for c in encoded):
        raise ValueError("parameter identifier must be 1..16 uppercase ASCII characters")
    return encoded


def receive_match(link, *, message_type, timeout):
    if type(timeout) not in (int,float) or not math.isfinite(timeout) or not 0<timeout<=60:
        raise ValueError("bounded timeout")
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        try:
            message=link.recv_match(type=message_type,blocking=True,
                timeout=min(.5,max(0,deadline-time.monotonic())))
        except TypeError as exc:
            if KNOWN_MULTI_INSTANCE_ERROR not in str(exc):raise
            continue
        if message is not None:return message
    return None


def receive_parameter(link,name,timeout=5):
    parameter_id(name)
    if type(timeout) not in (int,float) or not math.isfinite(timeout) or not 0<timeout<=60:
        raise ValueError("bounded timeout")
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        message=receive_match(link,message_type="PARAM_VALUE",
            timeout=min(.5,max(1e-9,deadline-time.monotonic())))
        if message is None:continue
        raw=message.param_id
        received=(bytes(raw).rstrip(b"\0").decode("ascii","strict")
                  if isinstance(raw,(bytes,bytearray)) else str(raw).rstrip("\0"))
        if received!=name:continue
        value=float(message.param_value)
        if not math.isfinite(value):raise ParameterReadError("nonfinite parameter")
        return value,int(message.param_type)
    raise ParameterReadError("parameter not observed; no query, write or rollback attempted")
