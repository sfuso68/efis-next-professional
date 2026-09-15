# EFIS NEXT — MAVLink burst and HEARTBEAT diagnostics batch 50H

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: IMPLEMENTED / SOFTWARE PASS / DEPLOYED / HARDWARE DIAGNOSTIC PENDING / NON-CERT

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
50. QGC verificato ancora attivo con PID 2660 e zero restart; la cattura 50H
    resta un gate hardware esplicito e non è dichiarata eseguita.

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

## Aperto / non dichiarare DONE

- acquisire report v2 sul portatile con QGC attivo;
- confrontare numero raffiche e intervalli oltre 500/1.000 ms;
- verificare separatamente il gate HEARTBEAT;
- ripetere la diagnostica su ingresso Ethernet Blackbox con privilegi locali;
- solo il confronto dei due lati può localizzare la formazione delle raffiche.
