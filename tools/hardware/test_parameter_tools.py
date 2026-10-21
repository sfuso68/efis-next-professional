#!/usr/bin/env python3
"""Parameter-tool checks; legacy writer quarantined under ADR-015.

Writer behavior is verified without serial, sockets or pymavlink.
The remaining historical backup fixtures use only a local UDP simulator;
their request-generating behavior is not a passive observer qualification.
No real Pixhawk is accessed by this harness.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
PYTHON = sys.executable
STUB = HERE / "mavlink_parameter_stub.py"
BACKUP = HERE / "backup_parameters.py"

results: list[tuple[bool, str]] = []


def check(condition: bool, description: str) -> None:
    results.append((bool(condition), description))
    print(f"  {'ok  ' if condition else 'FALLITA'} {description}")


class Stub:
    """Banco di prova avviato per la durata di una singola prova."""

    def __init__(self, port: int, *flags: str):
        self.port = port
        self.process = subprocess.Popen(
            [PYTHON, str(STUB), "--endpoint", f"udpout:127.0.0.1:{port}", *flags],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        time.sleep(1.2)

    def __enter__(self):
        return self

    def __exit__(self, *_):
        # Un banco caduto a meta' prova produce una fila di fallimenti che
        # sembrano difetti dello strumento e non lo sono. Va detto subito, e per
        # quello che e'.
        if self.process.poll() is not None:
            output = self.process.stdout.read() if self.process.stdout else ""
            print("  BANCO CADUTO durante la prova:")
            for line in output.strip().splitlines()[-6:]:
                print(f"    {line}")

        # Si chiude per identificatore di processo, mai per corrispondenza sulla
        # riga di comando: quel modello comparirebbe anche nel processo che sta
        # eseguendo le prove, e chiuderebbe anche quello.
        self.process.terminate()
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()


def main() -> int:
    import unittest
    suite = unittest.defaultTestLoader.discover(
        str(HERE.parents[1] / "tests"), pattern="test_mavlink_write_quarantine.py")
    if not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful():
        return 1

    print("\n11. Copia di sicurezza dei parametri")
    with tempfile.TemporaryDirectory() as directory:
        with Stub(14561):
            completed = subprocess.run(
                [PYTHON, str(BACKUP), "--device", "udpin:127.0.0.1:14561",
                 "--output-dir", directory, "--label", "prova",
                 "--timeout", "20", "--quiet-after", "2"],
                capture_output=True, text=True, timeout=60)
        manifest_files = list(Path(directory).glob("prova-*/manifest.json"))
        check(len(manifest_files) == 1, "la copia produce un manifesto")
        if manifest_files:
            manifest = json.loads(manifest_files[0].read_text(encoding="utf-8"))
            check(manifest.get("complete") is True, "la copia risulta completa")
            check(manifest.get("received_count") == manifest.get("declared_count"),
                  "i parametri ricevuti sono quanti l'autopilota ne aveva dichiarati")
            check(len(manifest.get("sha256_mav_parm", "")) == 64,
                  "il contenuto porta la propria impronta")
            parm = manifest_files[0].parent / "mav.parm"
            check(parm.exists() and "ARSPD_USE" in parm.read_text(encoding="ascii"),
                  "il file e' nel formato MAVProxy e contiene i parametri")
            check("PARZIALE" not in str(manifest_files[0].parent),
                  "una copia completa non viene marcata parziale")
        check(completed.returncode == 0, "la copia completa non e' un errore")

    print("\n12. I valori si scrivono nella forma piu' corta che si rilegge identica")
    from backup_parameters import format_value  # noqa: E402

    import struct as _struct

    def same_float32(text: str, value: float) -> bool:
        pack = lambda x: _struct.unpack("<f", _struct.pack("<f", float(x)))[0]
        return pack(text) == pack(value)

    check(format_value(0.1) == "0.1",
          "0.1 resta 0.1 e non diventa 0.100000001")
    check(format_value(0.0005) == "0.0005", "i valori piccoli restano leggibili")
    check(format_value(-1.0) == "-1", "gli interi non prendono decimali")
    # L'offset del sensore aria misurato sul banco. Nove cifre significative ne
    # scriverebbero 17.1814117; ne bastano otto per rileggere lo stesso float a
    # 32 bit, e la forma piu' corta e' quella che va scritta.
    offset = 17.181411743164062
    check(format_value(offset) == "17.181412",
          "conserva tutte le cifre che servono, e nessuna in piu'")
    check(len(format_value(offset)) < len(f"{offset:.9g}"),
          "e resta piu' corta delle nove cifre significative")
    check(all(same_float32(format_value(v), v) for v in
              (0.1, 0.0005, -1.0, 17.181411743164062, 100659.703125, 1e-5, 32003.0)),
          "ogni valore riletto produce lo stesso float a 32 bit")

    passed = sum(1 for ok, _ in results if ok)
    total = len(results)
    print(f"\n{'=' * 60}\n{passed}/{total} verifiche superate")
    if passed != total:
        print("\nFallite:")
        for ok, description in results:
            if not ok:
                print(f"  - {description}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
