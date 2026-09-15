# EFIS NEXT — Passive MAVLink delivery batch 50F

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: IN PROGRESS / SOFTWARE PASS / HARDWARE CAPTURE PENDING / NON-CERT

ID interessati: `NET-001`, `GS-001`.

## Cinquanta punti del lotto

1. Riletta la baseline autorevole e le ADR pertinenti.
2. Identificati esplicitamente `NET-001` e `GS-001`.
3. Confermati branch e HEAD iniziale `8ab299f`.
4. Preservati gli output utente non tracciati.
5. Mantenuto POWER1 fuori scope.
6. Escluse modifiche a Pixhawk, parametri, firmware e controllo di volo.
7. Separata la misura passiva dal socket UDP posseduto da QGC.
8. Scelta cattura Linux `AF_PACKET` senza bind UDP concorrente.
9. Aperto esclusivamente un socket raw di ricezione.
10. Validato il nome interfaccia con grammatica chiusa.
11. Validati sorgente e destinazione come IPv4 specifici.
12. Limitata la sorgente a Blackbox `192.168.1.47` in esecuzione.
13. Limitata la destinazione a Ground Station `192.168.1.39`.
14. Limitata la porta destinazione a UDP 14550.
15. Supportati Ethernet II e fino a due tag VLAN.
16. Validati versione, IHL e lunghezza totale IPv4.
17. Validati protocollo e lunghezza UDP.
18. Rifiutati datagrammi osservatore frammentati.
19. Contati separatamente datagrammi malformati.
20. Riutilizzato il parser MAVLink2 strutturale del sink RX-only.
21. Documentato che il parser non sostituisce CRC/dialect Blackbox.
22. Separate le sequenze per coppia system/component.
23. Gestito correttamente il wrap MAVLink 255→0.
24. Contati i gap di sequenza.
25. Contati i duplicati.
26. Contati i frame fuori ordine.
27. Misurati gli intervalli fra datagrammi ricevuti.
28. Calcolato percentile 95 degli intervalli.
29. Calcolato mdev degli intervalli senza chiamarlo jitter certificato.
30. Calcolata la frequenza applicativa dei datagrammi.
31. Aggiunto gate di copertura temporale minima 95%.
32. Aggiunte soglie esplicite per rate, gap, malformed, duplicati e riordino.
33. Reso fail-closed il caso senza frame.
34. Reso fail-closed ogni errore del socket raw.
35. Vietata la persistenza dei payload grezzi.
36. Aggiunta scrittura JSON atomica opzionale.
37. Aggiunto output stdout per separare cattura root e file utente.
38. Richiesto root o `CAP_NET_RAW` prima di aprire la cattura.
39. Aggiunto test statico contro `send`, `sendto`, `sendmsg` e `SOCK_DGRAM`.
40. Verificato parser Ethernet non taggato.
41. Verificato parser Ethernet VLAN.
42. Verificato scarto dei protocolli e degli endpoint non pertinenti.
43. Verificati rifiuto di troncamento, frammentazione e UDP length errata.
44. Verificati wrap, gap, duplicato e riordino.
45. Verificata indipendenza degli stream system/component.
46. Verificati timing aggregato e tutti i gate di valutazione.
47. Suite dedicata: 12/12 test software `PASS`, incluso capture loop simulato.
48. Tool e dipendenza distribuiti sul portatile; `py_compile` Linux `PASS`.
49. Prova non privilegiata terminata con exit 77 prima della cattura; QGC è
    rimasto PID 2660, attivo, zero restart, coda/drop UDP zero.
50. Cattura hardware reale non ancora eseguita: richiede un solo comando
    locale `sudo`; POWER1 e la rete operativa non sono stati modificati.

## Comando di acceptance locale

```bash
mkdir -p /home/stefano/.local/state/efis-next
set -o pipefail
sudo /usr/bin/python3 \
  /home/stefano/.local/lib/efis-next/capture_mavlink_observer.py \
  --interface wlo1 \
  --source 192.168.1.47 \
  --destination 192.168.1.39 \
  --port 14550 \
  --duration-seconds 60 \
  --minimum-hz 5 \
  --max-gap-percent 0.1 \
  --max-malformed 0 \
  --max-duplicates 0 \
  --max-out-of-order 0 \
  --output - | tee \
  /home/stefano/.local/state/efis-next/mavlink-passive-50f.json
echo "CAPTURE_RC=${PIPESTATUS[0]}"
```

Il report non contiene payload MAVLink. `CAPTURE_RC=0` e `status=PASS` sono
necessari ma non sufficienti per qualificare il Wi-Fi come deterministico.

## Aperto / non dichiarare DONE

- esecuzione hardware reale e confronto con i contatori Blackbox nello stesso
  intervallo;
- verifica che il modello per system/component non produca falsi gap sul
  flusso ArduPlane reale;
- persistenza power save dopo reboot;
- fault perdita/jitter con rollback, shutdown QGC e preferenza Ethernet;
- POWER1 resta rinviato.
