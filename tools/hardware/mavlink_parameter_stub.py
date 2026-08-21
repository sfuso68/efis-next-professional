#!/usr/bin/env python3
"""Banco di prova MAVLink per gli strumenti che leggono e scrivono parametri.

Il paragrafo 16 dell'handoff prescrive di riprodurre i problemi di
set_mavlink_parameter.py "con un mock o SITL", e di non provarlo direttamente sui
parametri critici del Pixhawk reale. SITL non e' installato sulla ground station,
e per questo scopo un banco locale e' comunque preferibile: e' deterministico,
parte in un decimo di secondo e soprattutto sa comportarsi male su richiesta.

Un autopilota funzionante dimostra soltanto che il caso normale funziona. Cio'
che conta in uno strumento di scrittura e' cosa succede quando la scrittura non
va a buon fine: e quei casi, su hardware vero, o non si riescono a produrre o si
producono lasciando il Pixhawk in uno stato che nessuno voleva.

Il banco parla MAVLink su UDP in locale. Non tocca alcuna seriale e non puo'
raggiungere il Pixhawk.

Guasti che sa simulare:

  --ignore-writes   accetta la scrittura e risponde, ma non cambia il valore.
                    E' il caso peggiore: l'autopilota sembra aver obbedito.
  --silent-writes   non risponde alla scrittura. Il valore cambia lo stesso.
  --refuse-writes   non risponde e non cambia nulla.
  --drop-reads N    ignora le prime N letture, per provare i tentativi ripetuti.
  --clamp-writes    applica il valore ma limitato a un massimo, come fa un
                    autopilota che rifiuta un valore fuori intervallo.
"""

from __future__ import annotations

import argparse
import time

from pymavlink import mavutil
from pymavlink.dialects.v20 import ardupilotmega as dialect

# Sottoinsieme realistico, con i valori osservati sul Pixhawk 6X del banco il
# 20 agosto 2026. I tipi rispecchiano quelli che ArduPilot dichiara davvero.
DEFAULT_PARAMETERS = {
    "ARSPD_TYPE": (8.0, dialect.MAV_PARAM_TYPE_INT8),
    "ARSPD_USE": (0.0, dialect.MAV_PARAM_TYPE_INT8),
    "ARSPD_DEVID": (32003.0, dialect.MAV_PARAM_TYPE_INT32),
    "ARSPD_SKIP_CAL": (0.0, dialect.MAV_PARAM_TYPE_INT8),
    "CAN_P1_DRIVER": (1.0, dialect.MAV_PARAM_TYPE_INT8),
    "CAN_D1_PROTOCOL": (1.0, dialect.MAV_PARAM_TYPE_INT8),
    "SERIAL3_PROTOCOL": (-1.0, dialect.MAV_PARAM_TYPE_INT8),
    "SERIAL4_PROTOCOL": (5.0, dialect.MAV_PARAM_TYPE_INT8),
    "COMPASS_DEV_ID": (658961.0, dialect.MAV_PARAM_TYPE_INT32),
    "COMPASS_DEV_ID2": (331777.0, dialect.MAV_PARAM_TYPE_INT32),
    "ARMING_CHECK": (1.0, dialect.MAV_PARAM_TYPE_INT32),
    "BRD_SAFETY_DEFLT": (1.0, dialect.MAV_PARAM_TYPE_INT8),
    # Parametro senza alcun ruolo operativo, pensato per le prove di scrittura.
    "SCR_HEAP_SIZE": (102400.0, dialect.MAV_PARAM_TYPE_INT32),
}


def parameter_id(name: str) -> bytes:
    """Forma richiesta in trasmissione: pymavlink vuole byte per un char[16]."""
    return name.encode("ascii")[:16]


def decode_name(raw) -> str:
    """Forma restituita in ricezione, che non e' la stessa.

    pymavlink pretende `bytes` quando si trasmette un campo di caratteri e
    restituisce `str` quando lo si riceve. E' l'asimmetria che aveva reso
    inutilizzabile la versione precedente di set_mavlink_parameter.py con il
    TypeError riportato al paragrafo 16 dell'handoff, e in cui questo banco e'
    caduto dal lato opposto costruendo `bytes` da una stringa. Attraversare il
    confine in un solo punto evita di ripeterla una terza volta.
    """
    if isinstance(raw, (bytes, bytearray)):
        return bytes(raw).rstrip(b"\x00").decode("ascii", "replace")
    return str(raw).rstrip("\x00")


class ParameterStub:
    def __init__(self, connection, parameters, options):
        self.link = connection
        self.parameters = dict(parameters)
        self.options = options
        self.reads_dropped = 0
        self.order = list(self.parameters)

    def send_value(self, name, override=None):
        value, kind = self.parameters[name]
        if override is not None:
            value = override
        self.link.mav.param_value_send(
            parameter_id(name), float(value), kind,
            len(self.parameters), self.order.index(name))

    def handle(self, message):
        kind = message.get_type()
        if kind == "PARAM_REQUEST_READ":
            name = decode_name(message.param_id)
            if name not in self.parameters:
                return
            if self.reads_dropped < self.options.drop_reads:
                self.reads_dropped += 1
                return
            self.send_value(name)
        elif kind == "PARAM_REQUEST_LIST":
            for name in self.order:
                self.send_value(name)
        elif kind == "PARAM_SET":
            name = decode_name(message.param_id)
            if name not in self.parameters:
                return
            if self.options.refuse_writes:
                return
            _, kind_code = self.parameters[name]
            requested = float(message.param_value)
            if self.options.ignore_writes:
                # Il caso peggiore, e la ragione per cui l'eco non e' una
                # verifica: l'autopilota conferma il valore chiesto e conserva
                # quello vecchio. Uno strumento che si fermasse alla risposta
                # immediata non se ne accorgerebbe mai.
                echo = requested
            else:
                applied = requested
                if self.options.clamp_writes is not None:
                    applied = min(applied, self.options.clamp_writes)
                self.parameters[name] = (applied, kind_code)
                echo = applied
            if not self.options.silent_writes:
                self.send_value(name, override=echo)

    def run(self):
        next_heartbeat = 0.0
        while True:
            now = time.monotonic()
            if now >= next_heartbeat:
                self.link.mav.heartbeat_send(
                    dialect.MAV_TYPE_FIXED_WING, dialect.MAV_AUTOPILOT_ARDUPILOTMEGA,
                    0, 0, dialect.MAV_STATE_STANDBY)
                next_heartbeat = now + 1.0
            message = self.link.recv_match(blocking=True, timeout=0.2)
            if message is not None and message.get_type() != "BAD_DATA":
                self.handle(message)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", default="udpin:127.0.0.1:14560",
                        help="estremo MAVLink su cui restare in ascolto")
    parser.add_argument("--ignore-writes", action="store_true")
    parser.add_argument("--silent-writes", action="store_true")
    parser.add_argument("--refuse-writes", action="store_true")
    parser.add_argument("--clamp-writes", type=float, default=None)
    parser.add_argument("--drop-reads", type=int, default=0)
    options = parser.parse_args()

    if not options.endpoint.startswith(("udpin:", "udpout:", "tcpin:", "tcp:")):
        raise SystemExit(
            "il banco accetta soltanto estremi di rete: puntarlo a una seriale "
            "significherebbe parlare all'autopilota vero")

    link = mavutil.mavlink_connection(options.endpoint, source_system=1,
                                      source_component=1, dialect="ardupilotmega")
    stub = ParameterStub(link, DEFAULT_PARAMETERS, options)
    print(f"banco parametri in ascolto su {options.endpoint}", flush=True)
    try:
        stub.run()
    except KeyboardInterrupt:
        return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
