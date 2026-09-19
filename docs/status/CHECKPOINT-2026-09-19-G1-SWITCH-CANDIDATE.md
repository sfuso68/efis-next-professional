# Checkpoint 2026-09-19 — candidato switch G1 MokerLink

## Autorizzazione

Il proprietario ha confermato: «Confermo registrazione documentale MokerLink
come candidato non verificato.» L'autorizzazione copre soltanto documentazione,
contratto di proposta e controlli fail-closed. Non autorizza acquisto,
cablaggio, alimentazione, configurazione, deployment o uso in volo.

## Registrazione

- Produttore: MokerLink.
- Modello presunto dal titolo commerciale: `IND-F082GS`.
- Identità: `PRESUMED_FROM_MARKETING_TITLE_NOT_LABEL_VERIFIED`.
- Segmento candidato: solo `FLIGHT-DATA`.
- Stato: `CANDIDATE_NOT_PHYSICALLY_VERIFIED`.
- Capacità dichiarata: 8×RJ45 10/100, 1×RJ45 Gigabit, 1×SFP Gigabit.
- Porte rame dichiarate: 9; minimo G1 con nodo sensori: 7.
- Margine puramente numerico: 2 porte.
- Switch installato autorevole: Waveshare Gigabit-Switch-5P (EU), invariato.

## Vincoli mantenuti

- `installed=false`.
- `execution_authorized=false`.
- `flight_approved=false`.
- `NO_GO_PENDING_ARCHITECTURE_AND_HARDWARE_REVIEW` invariato.
- VIDEO e telecamere vietati sul candidato.
- Switch VIDEO fisicamente separato ancora obbligatorio.
- SFP non conteggiata come rame senza transceiver verificato.
- Extend vietato; VLAN vietata finché non caratterizzata.
- Nessuna modifica a `avionics-network-v1.json`, nodi, flussi, IP o firewall.

## Evidenze ancora necessarie

1. Foto della targhetta modello.
2. Foto morsettiera ed etichetta alimentazione.
3. Foto port map e selettori di modalità.
4. Link test di tutte le porte RJ45.
5. Velocità negoziata per ogni porta.
6. Prova ingresso minimo/massimo e brownout.
7. Assorbimento, temperatura e riavvio.
8. Prova di isolamento fisico FLIGHT-DATA/VIDEO.
9. Fault e recovery test.

## Esito

La capacità teorica è `CANDIDATE_CAPACITY_UNVERIFIED`; il deficit dello switch
attualmente installato non è chiuso. La decisione resta **NO-GO** finché
identità e nove evidenze non sono acquisite e riesaminate.
