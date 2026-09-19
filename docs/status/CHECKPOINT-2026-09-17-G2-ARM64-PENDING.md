# Checkpoint — G2 ARM64 autorizzato, evidenza pendente — 2026-09-17

## Punto congelato

Il proprietario ha autorizzato la verifica read-only del freeze G2 sui tre
nodi ARM64 PFD, EMS e MFD. Questa autorizzazione non equivale a esito positivo.

## Tentativo eseguito

Dal workstation Windows è stato tentato esclusivamente l'accesso SSH in
lettura ai nodi noti `.45`, `.36` e `.49`. Tutti e tre hanno rifiutato
l'autenticazione. Il servizio `ssh-agent` locale risultava fermo/disabilitato e
non era disponibile una chiave utilizzabile. Nessun nodo e nessun servizio è
stato modificato.

## Procedura predisposta

È stato consegnato un blocco di comandi da eseguire sul PFD. La procedura:

1. recupera l'HEAD congelato `c331a9d3846d1f59addcea76f9db530174c8ec11`;
2. estrae in `/tmp` soltanto i sei file G2 necessari;
3. esegue validator e unit test sul PFD;
4. ripete la stessa verifica read-only su EMS e MFD;
5. salva il riepilogo in `/tmp/avionix-g2-arm64-results.txt`.

## Stato evidenza

- PFD ARM64: PASS parziale acquisito il 2026-09-18. Sul nodo fisico
  `efis-pfd-01` (`192.168.1.45`, `aarch64`, Python 3.13.5) il validator ha
  restituito `AVIONIX ICD V1: PASS`, tutti gli 11 test hanno superato la prova
  e i quattro digest congelati coincidono. Il report originario è archiviato
  byte-per-byte in `evidence/g2-arm64-pfd-20260918/` con SHA-256
  `80f6258c2fc7c469470768fef35687e1bea7424532a1b3b4bcc039867201841f`.
- EMS ARM64: PASS acquisito il 2026-09-18 sul nodo fisico `efis`
  (`192.168.1.36`, RPi4, `aarch64`, Python 3.13.5): validator e 11/11 test
  PASS, digest congelati coincidenti. Il report originario copiato dal target
  misura 2142 byte e coincide con lo SHA-256 emesso dal target
  `e66951087e5687339353faff04fb51cdfd99bf5728b27f8267bc119049644bea`.
- MFD ARM64: PASS parziale acquisito il 2026-09-18. Sul nodo fisico `pi5`
  (`192.168.1.49`, `aarch64`, Python 3.13.5) validator e 11/11 test sono PASS;
  il report originario è archiviato in `evidence/g2-arm64-mfd-20260918/` con
  SHA-256 `5fb12c74488f6b0ff46d51d027e176b115dd3cc0db5937587bc34399354c7d3e`.
- Hardware reale verificato da questa attività: due Raspberry Pi 5 (PFD/MFD)
  e un Raspberry Pi 4 (EMS), per il solo replay software G2 read-only.

`AVX-G2-01` è ora `ACCEPTED` e il bundle consolidato è archiviato. `PRJ-003`
resta `VERIFY-HW`: la prova dati live `AVX-G2-02` è separata, resta
`PROPOSED` e non è stata autorizzata da questa verifica.
