# AVIONIX G1 — ricognizione ASSO5/Sequent, batch 20

Data: 2026-09-18

Stato: **PREPARED-SW / NOT-RUN / NO-GO**

## Scopo

Questo batch prepara una ricognizione esclusivamente read-only del futuro nodo
`ASSO5-SENSOR-NODE` e delle quattro Sequent confermate dal proprietario. Non
identifica il nodo per supposizione, non esegue scansioni I²C, non installa CLI
o driver e non autorizza modifiche a DIP, cablaggi, alimentazione o servizi.

La specifica normativa è
`config/hardware/avionix-g1-sensor-node-discovery-v1.json`; il validator è
`tools/hardware/validate_g1_sensor_node_discovery.py`.

## Venti controlli

1. `G1-D20-01` — schema, revisione e registro controlli chiusi.
2. `G1-D20-02` — stato obbligatorio `PREPARED_SW_NOT_RUN`, senza autorizzazione
   di esecuzione o volo.
3. `G1-D20-03` — autorità legata a Master Brief, decision register e
   `AVX-G1-01/02`.
4. `G1-D20-04` — ruolo ASSO5 noto, identità fisica/IP/hostname ancora
   `UNRESOLVED`.
5. `G1-D20-05` — raccolta read-only di OS, kernel e architettura.
6. `G1-D20-06` — raccolta read-only di alimentazione, memoria e storage.
7. `G1-D20-07` — sola enumerazione controller I²C; probing e vendor read
   vietati prima della revisione hardware.
8. `G1-D20-08` — inventario atteso di quattro schede.
9. `G1-D20-09` — quantità congelate: 1×SM-I-018, 2×SM-I-011, 1×SM-I-019.
10. `G1-D20-10` — seconda SM-I-018 `NOT_INSTALLED_UNVERIFIED`, non promossa a
    fatto osservato.
11. `G1-D20-11` — foto fronte/retro, modello, revisione, seriale, connettori e
    SHA-256 per ciascuna scheda.
12. `G1-D20-12` — DIP acquisiti visivamente, senza toccarli.
13. `G1-D20-13` — singola sorgente di alimentazione; modello e rating
    obbligatori.
14. `G1-D20-14` — nessuna uscita, relay, registro I²C/Modbus, EEPROM o firmware.
15. `G1-D20-15` — nessuna installazione, upgrade o modifica boot/service/rete.
16. `G1-D20-16` — adapter reale e SIM distinguibili; trasporto publisher resta
    `UNRESOLVED`.
17. `G1-D20-17` — vocabolario health `VALID/STALE/LOST/FAIL/SIMULATED`.
18. `G1-D20-18` — catalogo fault minimo, con soglie e recovery non inventate.
19. `G1-D20-19` — raw output, operatore, UTC, SHA-256 e registro redazioni.
20. `G1-D20-20` — NO-GO finché nodo e 4/4 schede non sono verificati e
    approvati dall'operatore.

## Confine operativo

Il contratto non è un comando da eseguire. Una futura finestra di ricognizione
richiederà conferma esplicita del proprietario davanti al banco. In quella
finestra saranno ammessi soltanto letture del sistema, elenco dei controller
I²C senza interrogare indirizzi e fotografie. Ogni comando vendor verrà
valutato soltanto dopo aver letto modello e revisione dalle schede reali.

## Verifica software

```text
python tools/hardware/validate_g1_sensor_node_discovery.py
python -m unittest tests.test_g1_sensor_node_discovery -v
```

Esito atteso: 20/20 controlli PASS sul contratto, 0/4 schede verificate e
`execution_status=NOT_RUN`. Questo non costituisce evidenza hardware.
