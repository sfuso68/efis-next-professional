# EFIS NEXT — MAVLink burst and HEARTBEAT diagnostics batch 50H

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: IMPLEMENTED / SOFTWARE PASS / HARDWARE COMPARED / LIVENESS PASS / WIFI UNIFORMITY FAIL / NON-CERT

ID interessati: `NET-001`, `GS-001`.

## Cinquanta punti del lotto

1. Riletta la baseline autorevole e l'ADR rete.
2. Preservati tutti gli output utente non tracciati.
3. Mantenuto POWER1 fuori scope.
4. Escluse modifiche al controllo di volo.
5. Classificato 50G come timing `FAIL`, non perdita dati.
6. Confermata integrità 50G su 4.734 frame.
7. Confermati zero errori del broker durante la finestra.
8. Confermata stabilità QGC dopo la finestra.
9. Identificata la firma statistica a raffiche.
10. Evitato di attribuire la causa al Wi-Fi senza evidenza.
11. Evitato di allargare la soglia per ottenere un PASS.
12. Separata freshness HEARTBEAT dalla cadenza di tutti i messaggi.
13. Incrementato lo schema report da v1 a v2.
14. Conservata leggibilità dei report v1 come evidenza storica.
15. Aggiunto stato aggregato HEARTBEAT per system/component.
16. Contati separatamente i frame HEARTBEAT.
17. Misurato l'interarrival HEARTBEAT per stream.
18. Calcolati media, massimo, p95 e mdev HEARTBEAT.
19. Aggregati gli intervalli senza mescolare timestamp fra stream.
20. Aggiunto gate di presenza di almeno due HEARTBEAT.
21. Aggiunto gate di presenza del timing HEARTBEAT.
22. Aggiunto gate massimo interarrival HEARTBEAT.
23. Impostato il default HEARTBEAT a 3.000 ms.
24. Registrato il limite HEARTBEAT nel report.
25. Rifiutato un limite HEARTBEAT non positivo.
26. Aggiunta distribuzione degli intervalli in bucket.
27. Coperti bucket da 0–5 ms fino a 1.500–3.000 ms.
28. Contati gli intervalli oltre 3.000 ms.
29. Contati esplicitamente quelli oltre 100 ms.
30. Contati esplicitamente quelli oltre 500 ms.
31. Contati esplicitamente quelli oltre 1.000 ms.
32. Aggiunta analisi delle raffiche con separatore 100 ms.
33. Contato il numero delle raffiche.
34. Calcolata dimensione media delle raffiche.
35. Calcolate dimensioni minima e massima delle raffiche.
36. Calcolato p95 dei datagrammi per raffica.
37. Validata coerenza datagrammi/intervalli nel diagnostico.
38. Distinto esplicitamente datagrammi da frame MAVLink.
39. Mantenuto il report privo di payload.
40. Mantenuta assenza di qualsiasi percorso di trasmissione.
41. Mantenuto il socket AF_PACKET separato da UDP QGC.
42. Aggiunti test fail-closed dei tre gate HEARTBEAT.
43. Aggiunti test della distribuzione degli intervalli.
44. Aggiunti test dell'analisi delle raffiche.
45. Aggiunti test di non persistenza timestamp HEARTBEAT assoluti.
46. Suite dedicata aumentata a 17/17 `PASS`.
47. Regressione rete: 114 test, 112 `PASS`, 2 skip POSIX attesi.
48. Sintassi Python e `diff --check` verificati.
49. Distribuita la v2 sul portatile con hash locale/remoto identico
    `b0bb9e2369445d802b8c0c01c1920ea89c018f986522518d7f819f9661d6eabd`.
50. Acquisite entrambe le catture 50H: la struttura a raffiche è già presente
    sull'ingresso Ethernet; QGC e broker restano attivi con zero restart.

## Comando diagnostico locale

```bash
mkdir -p /home/stefano/.local/state/efis-next
set -o pipefail
sudo /usr/bin/python3 \
  /home/stefano/.local/lib/efis-next/capture_mavlink_observer.py \
  --interface wlo1 \
  --source 192.168.1.47 \
  --destination 192.168.1.39 \
  --port 14550 \
  --duration-seconds 180 \
  --minimum-hz 5 \
  --max-gap-percent 0.1 \
  --max-malformed 0 \
  --max-duplicates 0 \
  --max-out-of-order 0 \
  --max-interarrival-ms 1000 \
  --max-p95-interarrival-ms 100 \
  --max-heartbeat-interarrival-ms 3000 \
  --output - | tee \
  /home/stefano/.local/state/efis-next/mavlink-passive-50h.json
echo "CAPTURE_RC=${PIPESTATUS[0]}"
```

## Evidenza hardware comparativa

| Metrica | Pixhawk→Blackbox Ethernet | Blackbox→QGC Wi-Fi |
|---|---:|---:|
| Durata | 180,024 s | 180,052 s |
| Datagrammi/frame | 4.734/4.734 | 4.736/4.736 |
| Frequenza | 26,296 Hz | 26,303 Hz |
| Gap/duplicati/riordino/malformed/errori | 0 | 0 |
| HEARTBEAT | 180 | 180 |
| HEARTBEAT medio/max/mdev | 1.000,035/1.000,341/0,099 ms | 999,977/1.046,943/8,882 ms |
| Pause oltre 500 ms | 179 | 180 |
| Pause oltre 1.000 ms | 0 | 1 |
| Raffiche >100 ms | 194 | 196 |
| Datagrammi medi/max per raffica | 24,402/32 | 24,163/32 |
| Massimo interarrival | 979,900 ms | 1.014,687 ms |
| Esito gate stretti | PASS | FAIL massimo |

- Report ingresso Blackbox:
  `/home/pi/.local/state/efis-next/mavlink-ingress-50h.json`, SHA-256
  `e75fb1982c082597987e14ca213d52f123991374996cd80afb6bb196f0be05d3`.
- Report portatile:
  `/home/stefano/.local/state/efis-next/mavlink-passive-50h.json`, SHA-256
  `87fd999c508b63411afbcb06f76af2d91b6b69830e1b143c602d9a16b718d9e8`.
- La corrispondenza di 179/180 pause lunghe, 180 HEARTBEAT e circa 24
  datagrammi per raffica dimostra che la cadenza raggruppata è già presente
  prima del fan-out Wi-Fi. Le finestre non erano simultanee, quindi il delta
  esatto di jitter non è attribuibile pacchetto per pacchetto.
- Il Wi-Fi conserva integrità e liveness ma aggiunge variabilità osservabile:
  HEARTBEAT massimo +46,602 ms e mdev +8,783 ms rispetto alla prova Ethernet.
- Broker PID 2448 e QGC PID 2660 restano `active/running`, `NRestarts=0`;
  recorder/DDP/EDP continuano a crescere, failure e contatori UDP sono zero,
  throttling Blackbox `0x0`.

## Aperto / non dichiarare DONE

- definire nel tool stati separati per integrità, liveness e uniformità;
- non usare il limite uniforme di 1.000 ms come sinonimo di perdita link;
- mantenere la qualità Wi-Fi degradata finché non verificata su Ethernet;
- persistenza power save post-reboot e fault confinato restano aperti;
- nessun risultato costituisce approvazione o certificazione al volo.
