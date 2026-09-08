#!/usr/bin/env python3
"""Prove automatiche degli strumenti di lettura e scrittura parametri.

Ogni prova avvia il banco di prova MAVLink con un guasto scelto, esegue lo
strumento come processo separato e verifica il documento JSON che produce.
Nessuna prova tocca una seriale: il banco parla soltanto su UDP locale.

Le prove interessanti non sono quelle in cui la scrittura riesce. Sono quelle in
cui l'autopilota risponde di aver obbedito e non ha obbedito, in cui non
risponde affatto, e in cui il parametro chiesto non e' quello che si crede. Su
hardware vero quei casi o non si producono, o si producono lasciando il Pixhawk
in uno stato che nessuno voleva.

Uso:

    .../venv/bin/python tools/hardware/test_parameter_tools.py
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
SETTER = HERE / "set_mavlink_parameter.py"
BACKUP = HERE / "backup_parameters.py"

results: list[tuple[bool, str]] = []


def check(condition: bool, description: str) -> None:
    results.append((bool(condition), description))
    print(f"  {'ok  ' if condition else 'FALLITA'} {description}")


def parse_json(text: str):
    start = text.find("{")
    if start < 0:
        return None
    try:
        value, _ = json.JSONDecoder().raw_decode(text[start:])
        return value
    except json.JSONDecodeError:
        return None


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


def run_setter(port: int, *args: str, timeout: float = 60):
    completed = subprocess.run(
        [PYTHON, str(SETTER), "--device", f"udpin:127.0.0.1:{port}", *args],
        capture_output=True, text=True, timeout=timeout)
    return completed, parse_json(completed.stdout)


def main() -> int:
    print("Prove degli strumenti parametri ArduPilot\n")

    print("0. Recupero dal difetto pymavlink multi-istanza")
    sys.path.insert(0, str(HERE))
    from set_mavlink_parameter import receive_match  # noqa: E402

    class IntermittentLink:
        def __init__(self):
            self.calls = 0

        def recv_match(self, **_):
            self.calls += 1
            if self.calls == 1:
                raise TypeError("'NoneType' object does not support item assignment")
            return "heartbeat"

    intermittent = IntermittentLink()
    check(receive_match(intermittent, message_type="HEARTBEAT", timeout=1) == "heartbeat",
          "il frame multi-istanza difettoso viene scartato entro lo stesso timeout")
    check(intermittent.calls == 2, "la ricezione prosegue dopo il solo errore noto")

    class UnexpectedFailureLink:
        def recv_match(self, **_):
            raise TypeError("errore differente")

    try:
        receive_match(UnexpectedFailureLink(), message_type="HEARTBEAT", timeout=1)
    except TypeError:
        unexpected_visible = True
    else:
        unexpected_visible = False
    check(unexpected_visible, "gli altri TypeError non vengono nascosti")

    print("1. Prova a vuoto: legge, verifica la guardia, non scrive")
    with Stub(14551):
        completed, record = run_setter(
            14551, "--name", "SCR_HEAP_SIZE", "--expected", "102400", "--value", "204800")
        check(record is not None and record["outcome"] == "dry_run",
              "senza --commit l'esito e' una prova a vuoto")
        check(completed.returncode == 0, "la prova a vuoto non e' un errore")
        check("204800" in completed.stdout, "dichiara il valore che scriverebbe")

    print("\n2. Scrittura verificata su un parametro innocuo")
    with Stub(14552):
        _, record = run_setter(14552, "--name", "SCR_HEAP_SIZE", "--expected", "102400",
                               "--value", "204800", "--commit")
        check(record is not None and record["outcome"] == "verified",
              "la scrittura riletta con successo risulta verificata")
        check(record and record["value_before"] == 102400.0, "registra il valore di partenza")
        check(record and record["value_after"] == 204800.0, "registra il valore riletto")

    print("\n3. Guardia: il valore attuale non e' quello dichiarato")
    with Stub(14553):
        completed, record = run_setter(14553, "--name", "SCR_HEAP_SIZE", "--expected", "999",
                                       "--value", "204800", "--commit")
        check(record is not None and record["outcome"] == "guard_failed",
              "una guardia non soddisfatta impedisce la scrittura")
        check(completed.returncode != 0, "il fallimento della guardia e' un errore")
        check(record and "value_after" not in record,
              "non essendo stato scritto nulla, non c'e' alcun valore finale")

    print("\n4. Parametro protetto: ARSPD_USE non si abilita per distrazione")
    with Stub(14554):
        completed, record = run_setter(14554, "--name", "ARSPD_USE", "--expected", "0",
                                       "--value", "1", "--commit")
        check(record is not None and record["outcome"] == "refused_protected",
              "un parametro protetto viene rifiutato")
        check(completed.returncode != 0, "il rifiuto e' un errore, non un successo silenzioso")
        check("--allow-protected" in completed.stderr + completed.stdout,
              "spiega come procedere se la modifica e' davvero voluta")

    print("\n5. L'autopilota conferma ma non applica: il caso peggiore")
    with Stub(14555, "--ignore-writes"):
        completed, record = run_setter(14555, "--name", "SCR_HEAP_SIZE", "--expected", "102400",
                                       "--value", "204800", "--commit")
        check(record is not None and record["outcome"] == "readback_mismatch",
              "la riletta indipendente smaschera la conferma non mantenuta")
        check(record and record.get("echoed_value") == 204800.0,
              "l'eco riportava il valore nuovo, e non bastava")
        check(record and record.get("value_after") == 102400.0,
              "il valore reale era rimasto quello di prima")
        check(completed.returncode != 0, "una scrittura non applicata e' un errore")

    print("\n6. Nessuna risposta alla scrittura, ma il valore cambia")
    with Stub(14556, "--silent-writes"):
        _, record = run_setter(14556, "--name", "SCR_HEAP_SIZE", "--expected", "102400",
                               "--value", "204800", "--commit", "--timeout", "2")
        check(record is not None and record["outcome"] == "verified",
              "senza eco, ma con riletta concorde, la scrittura e' comunque verificata")
        check(record and record.get("echoed_value") is None,
              "l'assenza di eco viene registrata come tale")

    print("\n7. La scrittura non arriva affatto: si ripristina e si verifica")
    with Stub(14557, "--refuse-writes"):
        completed, record = run_setter(14557, "--name", "SCR_HEAP_SIZE", "--expected", "102400",
                                       "--value", "204800", "--commit", "--timeout", "2")
        check(record is not None and record["outcome"] == "readback_mismatch",
              "una scrittura rifiutata non passa per riuscita")
        check(record and record.get("rollback") == "verified",
              "il ripristino viene verificato, non soltanto tentato")
        check(record and record.get("value_restored") == 102400.0,
              "il valore ripristinato e' quello di partenza")
        check(completed.returncode != 0, "l'esito resta un errore")

    print("\n8. Letture perse: la richiesta viene ripetuta")
    with Stub(14558, "--drop-reads", "2"):
        _, record = run_setter(14558, "--name", "SCR_HEAP_SIZE", "--expected", "102400",
                               "--value", "204800", "--timeout", "2", "--retries", "4")
        check(record is not None and record["outcome"] == "dry_run",
              "due risposte perse non impediscono la lettura")
        check(record and record.get("read_attempts", 0) >= 3,
              "i tentativi effettuati vengono contati e dichiarati")

    print("\n9. Valore gia' impostato: non si riscrive")
    with Stub(14559):
        completed, record = run_setter(14559, "--name", "ARSPD_TYPE", "--expected", "8",
                                       "--value", "8", "--commit")
        check(record is not None and record["outcome"] == "already_set",
              "un parametro gia' al valore voluto non viene riscritto")
        check(completed.returncode == 0, "non essendoci nulla da fare, non e' un errore")

    print("\n10. Traccia di controllo")
    with tempfile.TemporaryDirectory() as directory:
        with Stub(14560):
            run_setter(14560, "--name", "SCR_HEAP_SIZE", "--expected", "102400",
                       "--value", "204800", "--commit", "--audit-dir", directory)
        files = list(Path(directory).glob("param-SCR_HEAP_SIZE-*.json"))
        check(len(files) == 1, "ogni esecuzione lascia un documento di controllo")
        if files:
            saved = json.loads(files[0].read_text(encoding="utf-8"))
            check(saved.get("outcome") == "verified", "il documento riporta l'esito")
            check("started_utc" in saved, "il documento riporta quando e' avvenuto")

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
