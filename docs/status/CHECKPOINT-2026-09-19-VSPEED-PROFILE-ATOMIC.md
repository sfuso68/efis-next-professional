# Checkpoint — V-speed profile fail-closed

Data: 2026-09-19  
Branch: `safety/vspeed-profile-atomic-v1`  
Requisiti: `CFG-001`, `PFD-002`

## Difetto riprodotto

Con `EFIS_AIRCRAFT_PROFILE` puntato a un file inesistente il nuovo test falliva:

```text
FAILED: an absent profile must expose no numeric V-speeds
```

Il controller esponeva dieci valori cablati; un profilo parziale poteva inoltre
sostituirne uno solo dopo avere già applicato altre sezioni del documento.

## Correzione

- nessun default numerico;
- dieci chiavi obbligatorie, finite, positive e coerenti;
- validazione prima di qualsiasi mutazione del controller;
- stato e messaggio espliciti per l'HMI;
- nessuna best-glide approvata nell'assistenza emergenza senza profilo valido.

## Evidenza software

```text
efis_pfd_aircraft_profile_tests: PASS
efis_hmi_golden_tools_tests: PASS
efis_avionix_hmi_contract_tests: PASS
Qt/QML AOT target efis-pfd-qt: PASS
```

Nessuna evidenza ARM64 o hardware reale è dichiarata. Nessun PR, merge o
deployment è incluso nel checkpoint.
