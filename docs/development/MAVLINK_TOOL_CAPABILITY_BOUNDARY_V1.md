# N1 — decisione B: ritiro writer e classificazione strumenti

2026-11-01. Conferma proprietario: «procedi» dopo proposta opzione B.
Branch dedicato safety/mavlink-write-quarantine-v1 / PR52 già esistente.
Nessun merge con il filone SIL della PR59, rebase o modifica a main.
ADR-015 rimane invariata: nessuna manutenzione bidirezionale autorizzata.

## Capacità nel sorgente candidato

| Strumento | Trasmissione applicativa | Limite |
|---|---|---|
| set_mavlink_parameter.py | Nessuna | Tombstone senza pymavlink/connessioni; CLI legacy e API rifiutano, anche --commit/--allow-protected |
| audit_pixhawk.py | Nessuna | Attesa passiva HEARTBEAT/versione/status; versione assente è NOT_OBSERVED, non un valore inventato |
| backup_parameters.py | PARAM_REQUEST_LIST | Lettura attiva non mutante; NON compatibile con observer unidirezionale |
| parameter_reads.py | Nessuna | Parser/ricezione passiva bounded, recupero solo errore pymavlink noto; nessun setter o connection factory |
| mavlink_parameter_stub.py | HEARTBEAT e PARAM_VALUE simulati | Fixture dichiarata: CLI solo udpin/udpout su IP letterale127.0.0.1,porte1024..65535 |

Audit e backup non costruiscono più COMMAND_LONG per richiedere la versione.
Entrambi accettano AUTOPILOT_VERSION soltanto se osservato spontaneamente.
Il backup conserva esplicitamente firmware=null/NOT_OBSERVED se non ricevuto.
Il suo manifesto dichiara ACTIVE_PARAMETER_QUERY_NON_MUTATING,
transmitted_message_types e observer_path_compatible=false.

Passivo applicativo non equivale a fd seriale O_RDONLY o filtro kernel:
questa modifica non ricostruisce il trasporto pymavlink e non certifica la
libreria. Il gateway operativo ADR-015 resta isolato e non viene modificato.
Il backup non ottiene un ritorno attraverso Blackbox/FLIGHT-DATA: una futura
lettura attiva reale richiede un accesso separatamente autorizzato.

Il nuovo helper conserva parser e gestione timeout senza introdurre nuove
query. Due osservazioni ricevute non attestano una riletta attiva indipendente.
Il ritiro completo del writer elimina anche il percorso dove timeout dopo
PARAM_SET poteva saltare rollback/audit: non si tenta di legittimarlo con patch
parziali a PROTECTED, disarmo o conferme da terminale. Una conferma software
non sostituisce un interlock fisico. Nessuna qualificazione/certificazione di
QGroundControl viene dedotta; operazioni manuali reali richiedono procedura.

## Verifica e perimetro

Sette test di quarantena preesistenti più11 nuovi test di confine (18) PASS
locali; con6 regressioni Project Control totale24 PASS. Tutti senza Pixhawk,
pymavlink, device o socket: moduli caricati con dipendenze iniettate/tripwire.
I test verificano versione assente/presente passiva, manifesto backup attivo,
sola richiesta lista, parser/timeout, rifiuto endpoint remoto prima della
connection factory e allowlist AST dei metodi send nei quattro entrypoint.
Non è una dimostrazione formale contro codice Python arbitrario/riflessione.
Suite aggiunta al workflow Project Control. Nessuna dipendenza installata.
Ripetizione Linux sul commit e sintassi YAML da registrare nell'evidenza.

Riga PIX-004 introdotta per censire questo confine manutentivo, PARTIAL:
implementazione candidata non significa ritiro delle copie già installate.
N1/PIX-002 resta separato da PIX-003 comandi autopilota, ancora fuori scope.
Il numero di righe sale a72 per tracciabilità, non per una nuova capacità TX.

Prima dell'integrazione servono review/CI PR52, riconciliazione dei documenti
con gli altri branch e autorizzazione merge; successivamente inventario copie
remote/staging/backup, distribuzione e acceptance solo se autorizzate.
Le versioni storiche Git non vengono cancellate né autorizzate all'uso.
Nessun dispositivo reale contattato, deployment, servizio, firmware, firewall,
rete o parametro cambiato. Non dichiarare il sistema intero privo di ogni
costruttore MAVLink: le fixture di simulazione sono esplicitamente distinte.
