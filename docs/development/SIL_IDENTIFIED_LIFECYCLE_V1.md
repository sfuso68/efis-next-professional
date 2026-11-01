# R1-35 — pausa/ripresa SIL identificate, senza riarmo

2026-11-01. DEVELOPMENT / NON-CERT. Solo processo SIL locale nel contesto OS
fidato già descritto in SIL_LOCAL_STDIO_CONSOLE_V1.md. Nessun server, rete,
servizio, chiave, display, Pixhawk o attuatore modificato.

## Opt-in e richiesta

Il runner `tools.simulation.owner_console` accetta `--lifecycle`. La modalità
PAUSE_RESUME_IDS_V1 è congelata nel config hash prima del manifesto. Senza
flag resta STOP_ONLY e le richieste identificate non sono abilitate.

```json
{"action":"STATUS"}
{"action":"PAUSE","command_id":"pause-1","session_id":"console-banco-001","expected_epoch":0}
{"action":"RESUME","command_id":"resume-1","session_id":"console-banco-001","expected_epoch":0}
{"action":"LOOKUP","command_id":"pause-1"}
```

STATUS ora espone session_id ed epoch. Campi esatti, token bounded ed epoch
intera non booleana. PAUSE richiede RUNNING, RESUME richiede PAUSED; entrambe
richiedono sessione/epoch attese. Un rifiuto valido è conservato come tale.
Sessione, epoch e stato non sono credenziali: il chiamante è già il processo
locale fidato. Non è una nuova API di rete autenticata.

La storia degli ID è condivisa con SET_CONTROL: 256 identità/512 KiB, senza
eviction. Prima di qualsiasi I/O si riserva l'identità originale. Retry identico
ritorna l'esito memorizzato anche quando l'epoch corrente è cambiata; non ripete
la transizione. Un payload diverso sullo stesso ID è DUPLICATE_CONFLICT. Questo
vale anche per il riuso di un ID fra input FDM e lifecycle.

## Revoca e limiti intenzionali

PAUSE revoca la lease, rimuove gli override e ferma la fisica. Il pump continua
a servire watchdog e STOP. RESUME preserva lo stato FDM e il contatore fisico
totale ma incrementa l'epoch e riavvia i clock relativi. **Non ricrea coda,
executor o lease**: i vecchi controlli restano revocati; nuovi SET_CONTROL non
vengono ammessi. START/REARM/rinnovo lease sono ancora disabilitati.
Una nuova run finita è oggi l'unico modo per ottenere una nuova autorità SIL.
Il riarmo nella stessa run richiede contratto nuovo multi-lease/audit/recovery.

STOP rimane indipendente da FIFO e storia. Se viene osservato durante INTENT,
non si prosegue con la transizione. Errore di intent, transizione o risultato
marca l'esito incerto, revoca e fa fallire la run; non viene convertito in un
semplice INVALID e non pubblica un successo. Nessun retry automatico/crash
recovery. Il punto di servizio resta cooperativo, non un arresto hardware.

## Audit e verificatore

INTENT/RESULT CONSOLE_LIFECYCLE sono nel truth journal esistente, con indice,
richiesta originale, sessione/epoch, snapshot prima/dopo e risultato. Non è
necessario un quinto payload: archivio V2 conserva il truth completo.
Con recorder async append significa QUEUED_NOT_DURABLE: la risposta dichiara
NOT_ATTESTED_UNTIL_RUN_VERIFIED. Non è prova di fsync prima della transizione.
La finalizzazione della run/restore verifica integrità e correlazione, non
firma, conformità temporale wall, modello aerodinamico o recovery.

Il parser richiede policy esplicita, campi e tipi esatti, ID/indici unici,
coppie complete e assenza di interleaving. PAUSED ed EPOCH/RUNNING di ripresa
richiedono intent corrispondente; rimuovere una coppia completa e ricalcolare
gli hash non può nascondere una transizione. RESULT corrisponde agli eventi
reali: PAUSED oppure EPOCH→RUNNING→FRAME senza avanzamento fisico in pausa.
Controlla anche collisioni e limiti condivisi fra ID lifecycle e richieste
osservate nel sidecar. È un lower bound della storia: i rifiuti di ammissione
input non sono nel vecchio sidecar, quindi non si pretende un audit ingress
completo. Un'epoch o sessione errata produce un rifiuto identificato.

## Correzione restore emersa dalle prove

Il TAR prodotto da tarfile può terminare con 10752 byte zero quando i due
blocchi obbligatori attraversano il bordo del record da 10240 byte. Il vecchio
limite 10240 rifiutava alcuni archivi creati dallo stesso packager. Il restore
ora ammette 1024..RECORDSIZE+512 byte, multipli di512 e tutti zero. Allowlist,
ordine/offset dei membri, anti-link/PAX/traversal e payload aggiunti rimangono
verificati. Nuovo test sul caso esatto10752 e sua corruzione nonzero.

## Evidenze e aperti

17 nuovi metodi locali. Prima regressione264 e ripetizione hanno incontrato
due timeout preesistenti di frame persistence>100ms prima di arrivare ai fault
iniettati nei test runner; log conservati. Suite runner isolata3 PASS.
I soli unit test di failure injection ora usano clock fixture per raggiungere
il confine iniettato, non come evidenza timing. Nessuna soglia runtime cambiata.
Regressione finale264 PASS. CTest Python/server da completare sul sorgente
congelato. Gate Windows nativo WinError4551 e warning path restano aperti.

P03/M2 PARTIAL: riarmo sicuro/dedup durevole, audit ammissione/wall,
replay fisico esecutivo, fanout e consumer simultanei, modelli/impianti completi
e acceptance real-time/power/pannelli non conclusi. Nessun nuovo DONE.
