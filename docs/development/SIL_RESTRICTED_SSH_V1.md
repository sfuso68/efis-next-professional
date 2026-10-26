# SIL R1-20 — accesso dedicato serverha → PFD

2026-10-26. DEVELOPMENT / NON-CERT / flight_approval=false.
Autorizzazione utente: chiave dedicata e limitata al banco SIL.

## Confine operativo

Source JSBSim isolato e bridge girano su serverha192.168.1.6; una sola SSH
autenticata raggiunge il dispatcher PFD192.168.1.45. Windows può gestire
la prova, ma non è relay né requisito della durata dopo avvio della unità
transiente serverha. Nessun NAT, IP forwarding, porta LAN nuova o Pixhawk.
Il namespace PFD pubblica soltanto DDP SIM65000 a127.0.0.1:49152.

La chiave non può armare il display: il launcher di banco deve essere
attivato separatamente tramite accesso amministrativo già autorizzato.
SIL_FEED_V1 richiede la schermata isolata attiva con endpoint/utente attesi.
Le sessioni sono bounded1–120s, non un simulatore permanente/autostart.
Screen/feed hanno lease140s e backstop root180s; fine feed chiude lo screen.
Il launcher amministrativo ripristina il PFD normale; recorder non fermato.

## Credenziale e grant installati

- Privata SOLO su serverha: `/home/sfuso/.ssh/efis_sil_pfd_ed25519`,0600.
- Fingerprint pubblico: `SHA256:iSzl9PBvc3VvDrf+lrosgifo1y/WgAxg8n3AAqQHU0k`.
- authorized_keys conserva le vecchie righe e aggiunge `restrict`,
  `from="192.168.1.6"`, forced-command dispatcher fisso.
- Soli comandi: `SIL_CHECK_V1` e `SIL_FEED_V1`. Nessun eval/argomento libero.
- `/etc/sudoers.d/efis-sil-r120` concede solo runner check/feed fissi.
- Dispatcher/runner/adapter in `/usr/local/lib/efis-next/sil-r120`,root0555;
  proprietario non modificabile da efis. Hash adapter fissato nel runner.
- DynamicUser `efis-sil-bench`, non utente efis: PrivateNetwork/Devices/Tmp,
  ProtectHome, ProtectSystem=strict, NoNewPrivileges, capability vuote,
  solo AF_UNIX/AF_INET,128MiB/swap0/16task/CPU50%/RuntimeMax140s.

Backup root-only originale:
`/var/lib/efis-next/sil-ssh-backup-r120-1793050156892054242/authorized_keys.before`.
SHA originale `12987ef363a71f8b411c159519cea097ba4c0f6014a86cb3d3b8ba545e71aff5`.
Il confronto del prefisso originale è passato. Nessuna chiave privata copiata
o inclusa nel repository; nessun riavvio sshd/configurazione globale alterata.

## Uso di banco

Solo serverha/UID1000 e path chiave esatto sono ammessi in --direct-serverha.
Esempio check senza GUI, da copia sorgente preparata su serverha:

```sh
python3 -m tools.simulation.live_bench_bridge --direct-serverha \
  --ssh-key /home/sfuso/.ssh/efis_sil_pfd_ed25519 \
  --directory /home/sfuso/avionix-sil-r114-20261025/NEW_UNIQUE_CHECK \
  --session NEW_UNIQUE_SESSION --seconds 3
```

Per la prova grafica aggiungere --loopback SOLO dopo armo amministrativo
del banco isolato tramite `run_pfd_sil_live_r119.sh`; non basta la chiave SIL.
Directory/sessioni nuove obbligatorie, niente sovrascrittura delle evidenze.
L'esecuzione verificata usa unità utente transiente serverha, non un relay
legato a Windows. Il container source rimane quello pinned R1-19 senza rete.

## Verifiche e limiti

60 test Windows,70 serverha,4 CTest mirati PASS. Otto dinieghi effettivamente
provati: comando vuoto,id,iniezione,argomenti extra,SFTP,PTY,TCP diretto,
remote forwarding. Agent/X11 negati da restrict ma non provati separatamente.
Due run dirette60s/1201frame PASS,RTTmax61.275209/61.274087ms; seconda con
limiti risorse osservati su nodo reale. Source maxlateness11.829301ms.
GUI finale20Hz/accepted1185/rejected0/gap0/socket0: non tutti i1201 frame
sono attestati come renderizzati. Ripristino automatico e recorder invariato.

Nessuna nuova build ARM64 o modifica renderer; C172P REFERENCE_UNVALIDATED,
radiofari fittizi. GUI EMS/meteo/NAV completa, fanout, controllo pilota,
modelli reali/endurance/power/golden/touch PFD restano aperti.
Evidenze in `../status/evidence/r1-20-restricted-sil-ssh-20261026/`.

## Revoca sicura

Da accesso amministrativo indipendente, identificare e rimuovere SOLO la
nuova riga con fingerprint/commento dedicati e forced-command sil-r120;
verificare prima e dopo i grant rimasti. Non ripristinare ciecamente l'intero
backup se nel frattempo sono state aggiunte altre chiavi. Non cancellare
evidenze/copie esistenti. La revoca non è stata eseguita: accesso persiste
come autorizzato. L'installer rifiuta installazioni sovrapposte anziché
sovrascrivere helper o sudoers esistenti.
