# AVIONIX G1 — inventario fisico Sequent, batch 10

Data: 2026-09-18

Stato: **IMPLEMENTED-SW / NOT-CAPTURED / NO-GO**

## Scopo

Il manifest `config/hardware/avionix-g1-physical-inventory-v1.json` prepara la
raccolta visuale delle quattro schede Sequent e dell'alimentatore del nodo
`ems-sensor`. Non interroga il bus, non legge registri vendor e non autorizza
modifiche a DIP, cablaggi o alimentazione.

## Dieci punti implementati

1. `G1-I10-01` — identità, revisione e registro controlli chiusi.
2. `G1-I10-02` — provenienza legata al report verificato del nodo `.89`.
3. `G1-I10-03` — quattro slot fissi: 1×SM-I-018, 2×SM-I-011, 1×SM-I-019.
4. `G1-I10-04` — foto fronte/retro con percorso relativo e SHA-256.
5. `G1-I10-05` — modello, revisione e seriale ricavati solo dalle etichette.
6. `G1-I10-06` — stato DIP esclusivamente visuale, senza modifica.
7. `G1-I10-07` — slot fisico e connettori obbligatori.
8. `G1-I10-08` — modello, tensione, corrente e foto etichetta alimentatore.
9. `G1-I10-09` — probing, vendor read, attivazioni e mutazioni vietati.
10. `G1-I10-10` — inventario incompleto `NO-GO`; anche 4/4 visuali richiedono
    una successiva revisione tecnica e non producono approvazione al volo.

## Verifica software

Il validator accetta il template con `0/4` schede verificate e un manifest
sintetico completo con `4/4`, ma mantiene entrambi in stato non operativo.
Rigetta percorsi con escape, hash malformati, quantità o modelli alterati,
indirizzi I²C, probing elettrico, policy indebolite e promozioni premature.

```text
python tools/hardware/validate_g1_physical_inventory.py
python -m unittest tests.test_g1_physical_inventory -v
```

## Evidenza ancora necessaria

Servono fotografie nitide fronte/retro di ogni scheda, etichette, revisione,
seriale se presente, DIP visibili, connettori e foto dell'etichetta
dell'alimentatore. Nessun dato visuale è stato ancora acquisito.
