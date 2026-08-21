#!/usr/bin/env python3
"""Misura, da una registrazione MAVLink reale, quanto ogni grandezza si muove.

Serve a tarare `FlightModelPolicy`, e distingue due parametri che si tarano con
dati diversi.

`resolution` e' la variazione minima che il rilevatore di stasi conta come
movimento. Deve stare **sopra** il rumore del sensore, e la ragione non e'
estetica: un pitot o uno statico ostruito non congela l'elettronica. Il sensore
continua a leggere la pressione intrappolata con il proprio rumore, quindi il
suo valore continua a ballare di qualche unita' anche se non descrive piu' il
mondo. Se `resolution` sta dentro quel ballo, il ballo passa per movimento e il
guasto non viene mai dichiarato. Questo si misura da una registrazione a riposo.

`suspect_after` e' per quanto una grandezza sana puo' restare ferma. Questo si
misura **solo in volo**: a terra ogni grandezza sta ferma per definizione, e
nessuna registrazione da banco puo' dire quanto a lungo una quota sana resti
immobile a 5000 piedi. Finche' non ci sono voli registrati, quel parametro resta
DEVELOPMENT.

Uso:

    python -m venv .venv && .venv/bin/pip install pymavlink
    .venv/bin/python tools/tuning/measure_signal_noise.py <registrazione.mav>

La registrazione puo' essere un flusso grezzo di byte MAVLink, come quelle
prodotte in ascolto passivo dal Pixhawk.
"""

from __future__ import annotations

import argparse
import warnings
from pathlib import Path

warnings.filterwarnings("ignore")

# Grandezza sorvegliata -> messaggio, campo, fattore verso l'unita' del modello,
# e la risoluzione attualmente configurata in FlightModelPolicy.
SIGNALS = {
    "roll":               ("ATTITUDE", "roll", 57.2957795, 0.1),
    "pitch":              ("ATTITUDE", "pitch", 57.2957795, 0.1),
    "indicated_airspeed": ("VFR_HUD", "airspeed", 1.94384449, 1.0),
    "ground_speed":       ("VFR_HUD", "groundspeed", 1.94384449, 0.5),
    "pressure_altitude":  ("VFR_HUD", "alt", 3.280839895, 5.0),
}


def read(path: Path):
    from pymavlink.dialects.v20 import ardupilotmega as dialect

    link = dialect.MAVLink(None)
    link.robust_parsing = True
    series = {name: [] for name in SIGNALS}
    boot_times = []

    for byte in path.read_bytes():
        try:
            message = link.parse_char(bytes([byte]))
        except Exception:
            continue
        if message is None:
            continue
        kind = message.get_type()
        if kind == "BAD_DATA":
            continue
        if hasattr(message, "time_boot_ms"):
            boot_times.append(message.time_boot_ms)
        for name, (expected, field, factor, _) in SIGNALS.items():
            if kind == expected and hasattr(message, field):
                series[name].append(getattr(message, field) * factor)
    return series, boot_times


def quantile(values, q):
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(q * (len(ordered) - 1)))]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("recording", type=Path)
    args = parser.parse_args()

    series, boot_times = read(args.recording)

    # La cadenza si ricava dalla differenza mediana positiva fra i tempi
    # dichiarati dall'autopilota. Una registrazione puo' contenere piu' sessioni:
    # il salto fra l'una e l'altra e' enorme e non va scambiato per un periodo.
    steps = [b - a for a, b in zip(boot_times, boot_times[1:]) if 0 < b - a < 10_000]
    period_ms = quantile(steps, 0.5) if steps else 0
    rate = 1000.0 / period_ms if period_ms else 0.0

    print(f"registrazione: {args.recording}")
    print(f"cadenza rilevata: {rate:.1f} Hz\n")
    print(f"{'grandezza':<22}{'n':>5}{'p50':>10}{'p95':>10}{'p99':>10}"
          f"{'risol.':>9}{'sopra':>8}   giudizio")
    print("-" * 92)

    for name, (_, _, _, resolution) in SIGNALS.items():
        values = series[name]
        if len(values) < 10:
            print(f"{name:<22}{len(values):>5}   troppo pochi campioni")
            continue

        deltas = [abs(b - a) for a, b in zip(values, values[1:])]
        # Si scartano le due variazioni maggiori: in una registrazione che
        # contiene piu' sessioni, o un assestamento all'avvio, sono salti
        # singoli che non descrivono il rumore.
        deltas = sorted(deltas)[:-2]
        above = sum(1 for d in deltas if d > resolution) / len(deltas)
        p99 = quantile(deltas, 0.99)

        if p99 == 0.0:
            verdict = "fermo: nessun rumore misurabile"
        elif resolution <= p99:
            verdict = f"RISOLUZIONE DENTRO IL RUMORE (serve > {p99:.2f})"
        elif resolution < p99 * 2:
            verdict = "margine stretto"
        else:
            verdict = f"margine {resolution / p99:.0f}x"

        print(f"{name:<22}{len(values):>5}{quantile(deltas,0.5):>10.4f}"
              f"{quantile(deltas,0.95):>10.4f}{p99:>10.4f}{resolution:>9.2f}"
              f"{above*100:>7.1f}%   {verdict}")

    print("\nLa colonna 'sopra' e' la frazione di campioni che a riposo supera")
    print("la risoluzione. Se e' alta, un sensore bloccato ma rumoroso non verra'")
    print("mai dichiarato fermo, perche' il suo rumore continua a passare per")
    print("movimento.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
