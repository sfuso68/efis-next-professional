# FUSO AVIONICS NEXT — checkpoint sviluppo offline GitHub

Data: 2026-09-30

Stato prodotto: **DEVELOPMENT / NON-CERT / NON APPROVATO AL VOLO**

## Motivo e vincolo operativo

Il proprietario ha comunicato il raggiungimento del limite mensile GitHub.
Fino a una sua esplicita comunicazione di ripristino non devono essere
eseguiti:

- `fetch`, `pull`, `push` o accessi API GitHub;
- apertura o modifica di issue e pull request;
- merge remoto o consultazione/avvio di GitHub Actions;
- operazioni che assumano che `main` remoto sia rimasto invariato.

Sono consentiti sviluppo, test e commit esclusivamente locali. Le attività che
richiedono nodi reali, riavvii, servizi, reti o hardware conservano inoltre i
rispettivi gate di autorizzazione: l'indisponibilità GitHub non amplia
l'autorità operativa.

## Baseline congelata nota

| Campo | Valore |
|---|---|
| repository | `sfuso68/efis-next-professional` |
| ultima `main` remota osservata | `ff75b242db0aa4075e19cecc07d5cfa779397e07` |
| ultimo merge osservato | PR #44, Blackbox NAV immutable release |
| branch locale offline | `work/offline-development-20260930` |
| punto di partenza branch | `ff75b242db0aa4075e19cecc07d5cfa779397e07` |
| working tree iniziale | pulito |

La baseline è soltanto l'ultima osservazione attendibile prima del blocco. Non
deve essere descritta come HEAD corrente di GitHub durante il periodo offline.

## Lavoro già integrato prima del blocco

1. PR #43: sanitizer Linux ASan/UBSan reso bloccante, merge `46e56a9`.
2. PR #44: policy fail-closed `bbxnav` inclusa nel contratto di release
   immutabile candidata, merge `ff75b24`.
3. La release attiva Blackbox non è stata modificata: resta
   `blackbox-7655799-primary-v1`; la candidata NAV resta NOT DEPLOYED.

## Coda locale ammessa

Priorità per lavori che non richiedono GitHub né intervento sui nodi:

1. riallineare lo stato documentale alla baseline `ff75b24` e mantenere un
   ledger dei commit locali;
2. revisionare e rafforzare test/contratti software già presenti, senza
   promuovere stati hardware;
3. preparare pacchetti e runbook fail-closed per le verifiche future, lasciando
   `execution_authorized=false` e `NOT_RUN`;
4. correggere difetti riproducibili con test locali e aggiornare nello stesso
   commit Master Matrix e stato corrente;
5. sospendere i blocchi che dipendono da manuali mancanti, foto posteriori,
   mapping Sequent, sensori, pannelli, alimentazione, server o nodi reali.

## Punti esplicitamente sospesi

- build/attivazione/reboot della candidata Blackbox NAV;
- build/attivazione/reboot del writer meteo asincrono PFD;
- soak retention PFD e qualunque lettura o modifica dei nodi;
- inventario ASSO5, MokerLink, M10, Matek e verticale G2;
- acceptance HMI/Synthetic Vision su PFD, MFD ed EMS;
- qualifica PSU/cavi e prove brownout;
- qualunque dichiarazione `DONE` basata solo su test workstation.

## Regole per i commit offline

- un branch locale unico conserva l'ordine dei cambiamenti;
- ogni blocco deve essere un commit autonomo con test e documentazione;
- `git diff --check` e Project Control devono essere PASS prima del commit;
- nessun commit locale deve dichiarare CI GitHub PASS;
- gli output hardware devono restare `NOT_RUN` se non realmente acquisiti;
- non usare rebase, reset o riscrittura della cronologia.

## Procedura di ripresa futura

Quando il proprietario comunicherà il ripristino di GitHub:

1. conservare l'hash dell'ultimo commit locale e verificare working tree
   pulito;
2. acquisire in sola lettura la nuova `main` remota;
3. confrontare la divergenza dalla baseline `ff75b24`;
4. se `main` è avanzata, integrare con un merge esplicito sul branch locale,
   senza rebase o reset;
5. rieseguire test mirati, Project Control e `git diff --check`;
6. pubblicare un solo branch di integrazione oppure più branch ricostruiti per
   commit autonomi, dopo review dell'ordine;
7. aprire PR indicando chiaramente quali verifiche sono soltanto locali;
8. attendere tutti i gate CI e richiedere/rispettare l'autorizzazione al merge;
9. aggiornare questo checkpoint con PR, merge commit e attività ancora aperte.

Questo checkpoint è la memoria operativa del periodo offline e non sostituisce
la Master Matrix, il registro delle release attive o le evidenze hardware.

## Ledger dei commit locali

| Commit | Contenuto | Verifica disponibile |
|---|---|---|
| `4c1f282` | checkpoint e riallineamento documentale alla baseline congelata | Project Control e `git diff --check` PASS |
| `c916bf3` | hardening fail-closed cache edge METAR/TAF e registrazione CTest | 9 test Python, CTest mirato, Project Control e `git diff --check` PASS; hardware NOT_RUN |
| `4a37458` | allowlist fail-closed degli stati radar presentabili in QML | 3 test contratto responsive, validatore UI, Project Control e `git diff --check` PASS; hardware NOT_RUN |
| `902e063` | classificazione freshness radar C++ a precisione di secondo e clock skew fail-closed | build Qt/MSVC, test servizio radar e contratto responsive via CTest, Project Control e `git diff --check` PASS; hardware NOT_RUN |

Questi commit sono presenti soltanto nel repository locale e non risultano
pubblicati su GitHub o replicati sul NAS durante il periodo di sospensione.
