# AVIONIX G1 — proposta upstream sensori, batch 20

Data: 2026-09-19

Stato: **PROPOSED-SW / NOT-APPROVED / NOT-RUN / NO-GO**

## Obiettivo

Definire una proposta verificabile per il tratto `sensor-rpi4 → Blackbox`
senza modificare l'ICD congelato, configurare la rete o distribuire servizi.
La proposta separa l'acquisizione grezza dall'EDP V1 destinato all'EMS.

## Venti punti

1. Identità e revisione del contratto congelate.
2. Esecuzione e uso in volo esplicitamente non autorizzati.
3. Nodo `sensor-rpi4` proposto su `10.77.10.10`, non configurato.
4. Unica destinazione Blackbox `10.77.10.2`.
5. ERAP V1 separato da EDP e ancora non congelato.
6. UDP unicast `49004` soltanto riservata, non aperta.
7. Flusso one-way senza comandi.
8. Blackbox unica autorità di normalizzazione e arbitraggio.
9. Payload minimo esclusivamente raw, identità e health.
10. `SourceId::asso5_engine = 9` soltanto riservato append-only.
11. Vietato riusare alias legacy.
12. CRC32C, boot ID, sequenza e tempo monotono obbligatori.
13. Rate non inventato prima della caratterizzazione hardware.
14. Freshness non inventata prima della qualifica del rate.
15. Raw registrato sul nodo e all'ingresso Blackbox.
16. Replay sul medesimo dataset.
17. Pixhawk/MAVLink/display diretto/bridge/NAT/attuazioni assenti.
18. Capacità switch dichiarata bloccante.
19. Nove blocker di deployment obbligatori.
20. Decisione finale NO-GO.

## Scelte tecniche proposte

`ERAP V1` trasporterebbe campioni grezzi identificati per nodo, canale e
sessione. Qualsiasi valore engineering prodotto localmente sarebbe soltanto
diagnostico: Blackbox accetta la revisione di calibrazione, normalizza e
pubblica EDP V1. La porta proposta `49004` non viene aggiunta al piano rete
finché review, inventario e capacità dello switch non sono chiusi.

## Criticità emersa

Lo switch Waveshare installato dispone di 5 porte. Il piano corrente ne indica
già almeno 6 come minimo operativo; aggiungendo il nodo sensori ne servono
almeno 7. È necessaria una revisione fisica della topologia o uno switch con
capacità adeguata, senza sacrificare collegamenti essenziali.

## Confine

Il batch non modifica `SourceId`, ICD, `avionics-network-v1.json`, firewall,
indirizzi, servizi o nodi reali. Trasforma due incognite architetturali in una
proposta sottoponibile a review, non in una decisione esecutiva.
