# AVIONIX G1 — MokerLink switch acceptance, batch 20

Data: 2026-09-19

Stato: `PREPARED-SW / NOT_RUN / NO-GO`

Oggetto: candidato MokerLink `IND-F082GS`, identità ancora presunta.

Questo batch rende verificabile l'accettazione al banco ma non la esegue. Il
Waveshare 5P resta l'unico switch installato autorevole. Non sono autorizzati
alimentazione, collegamenti, cambio IP, firewall, VLAN, Extend, deployment o
uso in volo.

| Controllo | Vincolo | Esito corrente |
|---|---|---|
| G1-S20-01 | identità e revisione contratto | PASS-SW |
| G1-S20-02 | schema chiuso | PASS-SW |
| G1-S20-03 | stato preparato ma non eseguito | PASS-SW |
| G1-S20-04 | nessuna autorità di mutazione | PASS-SW |
| G1-S20-05 | identità coerente con proposta G1 | PASS-SW |
| G1-S20-06 | topologia installata invariata | PASS-SW |
| G1-S20-07 | mappa porte attesa esatta | PASS-SW |
| G1-S20-08 | SFP esclusa dal conteggio rame | PASS-SW |
| G1-S20-09 | margine +2 solo teorico | PASS-SW |
| G1-S20-10 | destinazione esclusiva FLIGHT-DATA | PASS-SW |
| G1-S20-11 | VIDEO/telecamere vietati | PASS-SW |
| G1-S20-12 | VLAN/Extend vietati | PASS-SW |
| G1-S20-13 | tre fotografie identità obbligatorie | PREPARED-HW |
| G1-S20-14 | link e velocità di tutte le RJ45 | PREPARED-HW |
| G1-S20-15 | ingresso min/max e brownout | PREPARED-HW |
| G1-S20-16 | corrente, temperatura e restart | PREPARED-HW |
| G1-S20-17 | fault, recovery e isolamento | PREPARED-HW |
| G1-S20-18 | UTC, operatore, path sicuri e SHA-256 | PASS-SW |
| G1-S20-19 | NOT_RUN non può contenere risultati | PASS-SW |
| G1-S20-20 | BENCH_PASS non concede flight approval | PASS-SW |

## Procedura futura, non eseguita

1. Ottenere approvazione separata e presenza dell'operatore.
2. Fotografare targhetta, alimentazione e port map/DIP prima di alimentare.
3. Confermare modello e morsettiera dal manuale effettivo, non dal titolo.
4. Usare un banco isolato dalla rete avionica installata e da VIDEO.
5. Provare singolarmente le nove RJ45 e registrare la velocità negoziata.
6. Eseguire il protocollo elettrico approvato per min/max e brownout.
7. Registrare corrente, temperatura, riavvio, fault e recovery.
8. Produrre i dieci artefatti, manifest UTC, operatore e SHA-256.
9. Validare il bundle. Anche `BENCH_PASS` mantiene `flight_approval=false`.
10. Demandare installazione e accettazione di volo a una decisione distinta.

Le grandezze elettriche dichiarate dal venditore non sono soglie di
accettazione. Limiti, carichi e strumenti di prova devono essere approvati nel
workstream G4 prima dell'esecuzione.
