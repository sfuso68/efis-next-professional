# Checkpoint — MAVLink air-data health

Data: 2026-09-19  
Branch: `safety/mavlink-airdata-health-v1`  
Requisiti: `PIX-001`, `PIX-002`

## Difetto riprodotto

Un `VFR_HUD` seguito da `SYS_STATUS` con il bit absolute-pressure spento
manteneva quota e variometro utilizzabili:

```text
FAILED: an unhealthy absolute barometer must invalidate altitude and variometer
```

## Correzione

- bit `0x08` absolute pressure acquisito da presence/health;
- flag EKF `POS_VERT_ABS` acquisito separatamente da `VELOCITY_VERT`;
- validazione iniziale e rivalidazione retroattiva dei due campioni;
- nessuna modifica al canale differenziale IAS.

## Evidenza software

`efis_core_tests.exe`: PASS, inclusi guasto/ripristino barometro e guasti EKF
verticali indipendenti. Suite completa CTest: 47/47 PASS.

Nessuna verifica ARM64/hardware, nessun PR, merge, parametro o deployment è
incluso in questo checkpoint.
