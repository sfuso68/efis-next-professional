# Sblocco attivazione candidate a0be9a5 — banco NON-CERT

## Stato verificato

PR #49 integrata in main `4aea1e4edf8678c180e04171813d842ca6ff58a0`.
Cruscotto serverha GITHUB_SYNC_OK sul merge; replica NAS PASS, restore PASS,
refs preesistenti preservati nel report 20261019T175111Z. Le candidate
restano compilate da a0be9a5: il merge documentale NON le ha attivate.

| Nodo | Gate automatico attuale | Passo necessario |
|---|---|---|
| MFD `.49` | storico alimentazione 0x50000 | identificare/qualificare alimentazione, poi attivatore nuovo e canary |
| PFD `.45` | preflight automatico PASS | canary MFD accettato e finestra banco prima dello switch dual-role |
| EMS `.36` | sudo non interattivo assente | comando root preciso nella sessione dell'operatore e acceptance pannello |
| Blackbox `.47` | storico alimentazione 0x50000 | qualificare alimentazione; preservare NAV fail-closed e servizi accessori |
| Sensore `.89` | SSH timeout, G1 aperto | recuperare accesso/inventario per sensori reali; separato dai display SIM |

## 1. Cosa deve fare Stefano per l'alimentazione

Per prima cosa: comunicare se MFD e Blackbox sono alimentati via USB-C o
HAT/convertitore e fornire foto leggibili delle etichette. Non cambiare
collegamenti HAT/GPIO e non mettere in parallelo alimentazioni senza schema.

Se si usa USB-C, Raspberry Pi raccomanda per RPi5 l'alimentatore ufficiale
27 W, che offre 5,1 V/5 A. Un generico caricatore dichiarato 27/65 W non
dimostra quel profilo. Riferimenti ufficiali:
[RPi5](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html),
[scheda PSU 27 W](https://datasheets.raspberrypi.com/power-supply/27w-usb-c-power-supply-product-brief.pdf).
Questo è un riferimento per il banco, non una qualifica dell'alimentazione
dell'aeromobile o di un HAT non identificato.

Prima di qualsiasi arresto conservare il report attuale e gli eventi kernel:

```sh
date -u
hostname
vcgencmd get_throttled
vcgencmd measure_temp
sudo journalctl -b -k --no-pager | grep -Ei 'under.?voltage|voltage normal|throttl'
```

Solo dopo aver identificato la catena e concordato la finestra di banco:
arresto pulito di UN nodo alla volta (`sudo systemctl poweroff`), attesa dello
spegnimento, eventuale correzione dell'alimentazione, avvio controllato e nuova
verifica con carico rappresentativo del suo ruolo. Non fermare insieme tutti
i nodi e non staccare l'alimentazione mentre scrivono.

0x50000 registra eventi storici, non prova un difetto attivo. Un avvio nuovo
con 0x0 da solo non dimostra che la causa sia stata risolta: servono evidenza
della catena e prova sotto carico, oltre alle acceptance/endurance del piano.
Non ridurre tensioni di soglia o disattivare allarmi per ottenere PASS.

## 2. EMS: privilegi senza aprire sudo indiscriminatamente

Da PowerShell aprire una sessione interattiva:

```powershell
ssh pi@192.168.1.36
```

Nel terminale EMS:

```sh
hostname
sudo -v
sudo -n true && echo SUDO_LOCALE_OK
```

Non inviare password in chat. Questo prova il sudo nella TUA sessione: il
timestamp normalmente non abilita una sessione SSH diversa. Non creare regole
`NOPASSWD: ALL`, non abilitare login root e non copiare chiavi private.
Quando siamo nella finestra prevista, verrà preparato e verificato il comando
di attivazione root esatto da eseguire qui, con sorgente, hash, rollback e gate.
La semplice abilitazione di sudo non è l'attivazione della nuova release.

Aggiornamento EMS del 19: attivatore nuovo preparato e provato solo come
preflight/test software. Comando locale con hash fissato in
[`EMS_A0BE9A5_ACTIVATION.md`](EMS_A0BE9A5_ACTIVATION.md). Questo aggiorna il
passo di preparazione EMS, non autorizza gli attivatori MFD o EMS storici.

## 3. Preflight già disponibile: nessun restart

Su ciascun nodo, nella directory del suo utente:

```sh
# MFD, utente pi
cd /home/pi/efis-activation-preflight-20261019-v2
python3 -B -m tools.deployment.check_rollout_readiness --node MFD
```

Per EMS e Blackbox usare la stessa directory con `--node EMS` o
`--node BLACKBOX`. Sul PFD utente efis la directory è
`/home/efis/efis-activation-preflight-20261019-v2`, opzione `--node PFD`.
Exit 2 significa gate bloccato; exit 0 significa soltanto che i controlli
automatici passano. `activation_authorized` rimane sempre false.

Il tool non fa restart, switch, reboot, chown/chmod, accesso ai bus o modifiche
sudoers. L'unica lettura root eventuale è `sudo -n readlink -f /proc/PID/exe`
per il processo broker appartenente a un altro utente. Il codice controlla
anche tutti i ruoli PFD+RECORDER, non solo l'ultimo flag della CLI legacy.

## 4. Attivazione: ordine e stop condition

Non lanciare `activate_mfd_legibility_immutable_release.sh` o gli attivatori
EMS storici: contengono pin di release precedenti. Inoltre il trap del vecchio
MFD può riavviare il servizio anche se fallisce il gate prima dello switch.
Non sono stati eseguiti né modificati in questo blocco. Serve una versione
nuova che armi il recovery soltanto dopo la prima mutazione, conservi i file
precedenti e controlli l'integrità degli helper/dipendenze prima dell'uso root.

Sequenza futura: preflight e approvazione fisica → finestra di banco → MFD
con prova rollback/ritorno candidato → reboot e verifica visiva → PFD dual-role
con retention preservata. EMS e Blackbox hanno acceptance proprie; nessuna
qualifica del canary MFD si trasferisce automaticamente ad altri ruoli.
Al primo errore fermare la promozione; non saltare i controlli.

Il sensore `.89` richiede accesso locale (`hostname -I`, `ip -br address`)
e G1 prima di qualunque adapter reale, scansione bus o calibrazione. Non
blocca in sé la presentazione SIM sui display; non usare dati SIM per
convalidare limiti o sonde dell'aeromobile.
