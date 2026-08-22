# EFIS NEXT system simulator

The simulator is a deterministic, non-flight test source for the complete EFIS
data path. Every generated sample retains `Validity::simulated` and
`origin.simulated=true`. Its output must never be used for calibration, engine
hour accounting or flight decisions.

## Scenarios

| Scenario | Purpose |
|---|---|
| `full-flight` | Cold-and-dark through shutdown, Treviso–Belluno route |
| `engine-emergency` | Low oil pressure followed by high CHT |
| `electrical-failure` | Alternator loss, low bus voltage and zero alternator current |
| `sensor-degradation` | Sequential airspeed, altitude and navigation loss/recovery |

The 10-minute timeline covers engine start, taxi, takeoff, climb, cruise,
descent, approach, landing and shutdown. It also drives RPM, manifold pressure,
fuel flow, fuel pump, pitot heat, gear locks, flap detents, simulated METAR/TAF
and weather-radar severity.

## Running

```sh
./efis-simulator --scenario full-flight --step-ms 1000 --duration-s 600 \
  --output full-flight.jsonl
```

The JSONL stream uses schema `efis-next.system-simulation.v1`; every row is
explicitly marked `"simulated":true`. Short targeted runs can jump through the
timeline by choosing a larger step. The simulator runs faster than real time so
the complete regression can be checked in seconds.

## Safety boundary

- No MAVLink transmission and no autopilot control path.
- No access to serial ports or CAN interfaces.
- No update of authoritative engine-hour state.
- Simulation data remains visibly and structurally distinguishable from live data.
- Intended for software tests and display demonstrations only.
