# Checkpoint — G1 physical inventory prepared

Data: 2026-09-18

Branch: `work/skyview-ems-hmi-contract-v1`

## Risultato

Il batch G1-I10 prepara un inventario visuale verificabile per le quattro
schede Sequent e l'alimentatore del nodo `ems-sensor`. Il template passa 10/10
controlli senza dichiarare evidenza hardware; la suite dedicata passa 8/8 test.

## Confine verificato

- Nessuna scansione o lettura di indirizzi I²C.
- Nessun comando vendor.
- Nessuna modifica a DIP, cablaggio, alimentazione, boot, rete o servizi.
- Nessuna attivazione di ingressi, uscite o relay.
- Nessuna approvazione al volo.

## Stato

- Nodo: identità precedente conservata, nessuna nuova prova.
- Schede: 0/4 verificate.
- Alimentatore: non verificato.
- `AVX-G1-01`: `IN_PROGRESS`.
- `AVX-G1-02`: `OPEN`.
- Decisione: `NO_GO_PENDING_PHYSICAL_BOARD_INVENTORY`.

## Prossima evidenza necessaria

Foto nitide fronte/retro per ciascuna scheda, etichette, revisione, seriale se
presente, DIP visibili, connettori, ordine fisico e foto dell'etichetta
dell'alimentatore. Dopo la compilazione il manifest resta NO-GO fino alla
revisione tecnica.
