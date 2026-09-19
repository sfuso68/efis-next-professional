# CHECKPOINT — isolamento fan-out MAVLink V1

- Data: 2026-09-19
- Branch: `safety/mavlink-fanout-isolation-v1`
- Baseline: `80791a4e72430dd7e9e14491fcb428e8716b750c`
- Ambito: hardening software; nessun deployment o intervento hardware
- Requisiti interessati: `NET-001`, `BBX-003`

## Dieci punti eseguiti

1. Formalizzata nel codice la rete protetta FLIGHT-DATA `10.77.10.0/24`.
2. Collegata la sorgente MAVLink configurata alla policy del fan-out.
3. Impedita ogni destinazione observer interna alla rete protetta.
4. Impedita una destinazione observer uguale alla sorgente configurata.
5. Conservati i rifiuti preesistenti di wildcard, broadcast, multicast e
   duplicati.
6. Aggiunti test C++ per Pixhawk, altro nodo avionico, source equality e rete
   adiacente ammessa.
7. Applicata la stessa policy al forwarder USB Python, inclusa la costruzione
   diretta senza CLI.
8. Aggiunti test Python e registrata la suite nel CTest di progetto.
9. Aggiunti test di processo del broker per entrambe le configurazioni
   vietate e aggiornata la documentazione operativa/ADR.
10. Aggiornati stato e master matrix, lasciando `NET-001` e `BBX-003` in
    `PARTIAL` fino a CI, deployment e prove hardware autorizzate.

## Evidenza software

- Build core MSVC Debug: PASS.
- Suite core completa: 48/48 CTest PASS dopo l'ultima modifica.
- Build completa Qt/QML `build-hmi`: PASS.
- `efis_mavlink_observer_fanout_tests`: PASS.
- `efis_blackbox_broker_observer_tests`: PASS.
- `efis_mavlink_readonly_forwarder_tests`: PASS su Windows; due prove seriali
  pseudo-terminale sono POSIX-only e vengono eseguite dalla CI Linux.

## Evidenza hardware

Nessuna. Il branch non e' stato distribuito su Blackbox, portatile, server o
altri nodi. Non sono state modificate configurazioni, firewall o connessioni.

## Rischi e lavoro aperto

- CI Linux e review indipendente non ancora acquisite.
- Startup e fault injection della configurazione non verificati sulla
  Blackbox reale.
- Continuita' di recorder, DDP, EDP e observer durante fault/recovery non
  verificata con questa versione.
- Nessuna approvazione al volo o autorita' di comando deriva dal checkpoint.
