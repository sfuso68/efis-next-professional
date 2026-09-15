# EFIS NEXT — MAVLink freshness gates batch 50G

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: IMPLEMENTED / SOFTWARE PASS / DEPLOYED / HARDWARE RECHECK PENDING / NON-CERT

ID interessati: `NET-001`, `GS-001`.

## Cinquanta punti del lotto

1. Riletta la baseline autorevole e le ADR pertinenti.
2. Preservati gli output utente non tracciati.
3. Mantenuto POWER1 fuori scope.
4. Escluse modifiche a Pixhawk, firmware e parametri.
5. Acquisito il report hardware 50F originale.
6. Verificato l'hash SHA-256 del report 50F.
7. Confermato `CAPTURE_RC=0` della prova 50F.
8. Confermati 1.578 datagrammi e 1.578 frame.
9. Confermata frequenza applicativa di 26,260094 Hz.
10. Confermato un solo stream system 1/component 1.
11. Confermati zero gap di sequenza.
12. Confermati zero duplicati.
13. Confermati zero frame fuori ordine.
14. Confermati zero frame malformati.
15. Confermati zero errori socket.
16. Confermata assenza di payload persistito.
17. Separata integrità di sequenza dalla regolarità temporale.
18. Identificato massimo interarrival di 1.012,999 ms.
19. Identificato p95 interarrival di 47,726 ms.
20. Identificato mdev interarrival di 176,557 ms.
21. Rifiutata la promozione del Wi-Fi a canale deterministico.
22. Confermata la soglia CORE `stale_after=1000 ms` come confine di salute.
23. Adottato 1.000 ms come gate acceptance massimo, non certificativo.
24. Adottato 100 ms come gate acceptance p95, non certificativo.
25. Aggiunta opzione CLI `--max-interarrival-ms`.
26. Aggiunta opzione CLI `--max-p95-interarrival-ms`.
27. Resi espliciti i default 1.000/100 ms.
28. Aggiunto gate `timing_samples_present`.
29. Aggiunto gate `maximum_interarrival_within_limit`.
30. Aggiunto gate `p95_interarrival_within_limit`.
31. Reso fail-closed il report senza campioni timing.
32. Reso fail-closed un massimo oltre soglia.
33. Reso fail-closed un p95 oltre soglia.
34. Mantenute inclusive le soglie esatte e arrotondati massimo/p95 per eccesso.
35. Rifiutati limiti timing nulli o negativi.
36. Rifiutato un limite p95 maggiore del limite assoluto.
37. Persistiti nel report i limiti usati per l'acceptance.
38. Mantenuta invariata l'assenza di percorso di invio.
39. Mantenuta invariata l'assenza di bind UDP concorrente.
40. Mantenuta invariata l'assenza di payload nel report.
41. Aggiunti test di fallimento per tutti i gate timing.
42. Aggiunti test delle soglie esatte.
43. Aggiornato il test end-to-end con limiti espliciti.
44. Suite dedicata aumentata a 14/14 `PASS`.
45. Suite di regressione rete: 111 test, 109 `PASS`, 2 skip attesi.
46. Sintassi Python del tool e dei test `PASS`.
47. Distribuita la nuova versione sul portatile Debian 13.
48. Verificato hash repository/nodo identico `58fb5a...416c`.
49. Verificati help CLI e `py_compile` sul nodo reale.
50. Verificato QGC ancora `active/running`, PID 2660, zero restart; la
    ripetizione raw con i nuovi gate resta intenzionalmente pendente.

## Comando di acceptance locale

Lasciare QGC attivo. Il comando conserva solo aggregati e termina `FAIL` se
ricompare un intervallo superiore a un secondo o se il p95 supera 100 ms.

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
  --output - | tee \
  /home/stefano/.local/state/efis-next/mavlink-passive-50g.json
echo "CAPTURE_RC=${PIPESTATUS[0]}"
```

## Aperto / non dichiarare DONE

- eseguire la nuova cattura reale da 180 s e conservare hash/report;
- campionare i contatori Blackbox esattamente ai limiti della stessa finestra;
- se il gate massimo fallisce, caratterizzare frequenza e causa delle pause;
- verificare persistenza power save dopo reboot e preferire Ethernet;
- fault perdita/jitter, shutdown QGC e POWER1 restano gate separati.
