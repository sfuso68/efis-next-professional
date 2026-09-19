# AVIONIX G1 — acquisizione read-only nodo sensori, batch 10

Data: 2026-09-18

Stato: **IMPLEMENTED-SW / VERIFIED-RPI4 / BOARDS-NOT-VERIFIED / NO-GO**

## Scopo

`tools/hardware/capture_g1_sensor_node.py` sostituisce la raccolta manuale con
un report JSON deterministico. Il tool usa soltanto comandi fissi di lettura,
non usa shell o `sudo` e non contiene utility di probing I²C, CLI Sequent,
installazioni, riavvii o scritture.

## Dieci punti implementati

1. `G1-C10-01` — schema e revisione chiusi.
2. `G1-C10-02` — UTC e hostname obbligatori.
3. `G1-C10-03` — OS, kernel, architettura e modello completi.
4. `G1-C10-04` — memoria, storage e filesystem acquisiti.
5. `G1-C10-05` — interfacce e indirizzi di rete acquisiti.
6. `G1-C10-06` — configurazione I²C e SHA-256 del boot config obbligatori.
7. `G1-C10-07` — solo enumerazione controller; `/dev/i2c-1` richiesto e
   probing/vendor read strutturalmente falsi.
8. `G1-C10-08` — GPIO2/SDA1 e GPIO3/SCL1 verificati.
9. `G1-C10-09` — temperatura e throttling presenti.
10. `G1-C10-10` — nessuna operazione vietata, zero schede promosse,
    `flight_approval=false` e decisione NO-GO.

## Verifica software

- Sette test Python PASS.
- Mutazione isolata di ciascuno dei dieci controlli rifiutata fail-closed.
- Comandi arbitrari e `i2cdetect -y 1` rifiutati prima dell'esecuzione.
- Suite CTest completa: 42/42 PASS.

## Verifica RPi4 reale

- Nodo `ems-sensor` (`192.168.1.89`): capture 10/10 PASS.
- Report persistente:
  `/home/pi/.local/state/efis-next/g1-node-capture-20260918.json`.
- UTC report: `2026-09-18T20:53:42Z`.
- SHA-256:
  `f7cb1da79623c37891e662ec966f8d64be743a8d471663970993e4eac93f5e94`.

## Limite

Il report prova il nodo e il controller, non le schede. Modelli, revisioni,
DIP, slot, connettori, alimentatore e indirizzi delle quattro Sequent restano
non verificati. Nessun adapter o publisher live è autorizzato.
