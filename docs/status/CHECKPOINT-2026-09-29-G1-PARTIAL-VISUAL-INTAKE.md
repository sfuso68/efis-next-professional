# CHECKPOINT — G1 capture nodo e intake visuale parziale

Data: 2026-09-29

Stato: **HW-PARTIAL / VISUAL-INTAKE-INCOMPLETE / NO-GO**

## Scopo

Confermare in sola lettura l'identità e la salute del nodo `ems-sensor` e
classificare le fotografie fornite dall'operatore senza promuoverle a
inventario fisico completo.

## Evidenza acquisita dal nodo

Il capture controllato è stato eseguito sul nodo `ems-sensor` `192.168.1.89`
con `tools/hardware/capture_g1_sensor_node.py`. Il report è conservato in
`docs/status/evidence/g1-node-capture-ems-sensor-20260929/node-report.json`.

SHA-256 del report:
`956413c4faf489ceff6a6238490f48ae269a3ddc9f5b726223d950d86edaa155`.

Risultati osservati:

- Raspberry Pi 4 Model B Rev 1.5, Debian 12, `aarch64`;
- `i2c-1`, `i2c-20` e `i2c-21` esposti; GPIO 2/3 in funzione SDA1/SCL1;
- nessun address probe e nessuna lettura vendor;
- Wi-Fi attivo su `192.168.1.89`; Ethernet `NO-CARRIER`;
- `get_throttled=0x0`, temperatura nel capture `38.4 C`;
- controlli G1-C10: 10/10 PASS;
- decisione invariata: `NO_GO_PENDING_PHYSICAL_BOARD_INVENTORY`.

## Fotografie ricevute ma non importate

Cinque immagini operatore sono state esaminate e sottoposte a hash, ma non
sono copiate nel repository né accettate dal packager perché il set richiesto
è incompleto.

| Input | Identificazione visuale provvisoria | Revisione | SHA-256 |
|---|---|---|---|
| `13266.jpg` | SM-I-018, 16 analog/digital inputs | V2.1 | `c745a05a47a63f17daa9f12ab0df511aa633ff742f01e3844fda30531a0b1718` |
| `13269.jpg` | prima SM-I-011, 8 LV/HV inputs | V6.0 | `bc9a115ac895942bb46e4a08a755d84106af3a4e89e08bb570c68f372a95e8c4` |
| `13267.jpg` | seconda SM-I-011, 8 LV/HV inputs | V6.0 | `96129e1a5de8aa593247a8c66a75b5c0ae2196c363ad629837ead88559899bcb` |
| `13268.jpg` | 8 Thermocouple DAQ, coerente con SM-I-019 | V2.1 | `187599b9ca93adc3204521c6d3a8e9bb670b1e4a3cf82f907e6e31ebfac68889` |
| `13270.jpg` | più alimentatori; label e sorgente non determinabili | n/a | `2d2d7606a596c832fa430cb3866ac75cfc2fedbe446b7d3da6099be826eec049` |

La posizione dei DIP è visibile ma non viene trascritta finché non è
revisionata su un set completo e nominato per slot. Nessun seriale è stato
dedotto e nessuna immagine è considerata foto del retro.

## Evidenza mancante

1. quattro fotografie nitide del retro, una per slot;
2. fotografia ravvicinata della label dell'alimentatore realmente usato;
3. modello, tensione e corrente leggibili;
4. conferma dell'operatore che si tratta della singola sorgente del nodo;
5. UTC e identità operatore per l'intake definitivo;
6. revisione tecnica di DIP, connettori e corrispondenza slot.

Fino alla completezza del set il manifest resta `0/4 VERIFIED_VISUAL`,
`hardware_verified=false` e `flight_approval=false`. Nessun DIP, cablaggio,
parametro, uscita o alimentazione è stato modificato.
