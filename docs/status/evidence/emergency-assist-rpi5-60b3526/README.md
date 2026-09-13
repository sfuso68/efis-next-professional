# Emergency Assist — evidenza rendering RPi5 `60b3526`

Questa directory conserva le otto catture prodotte sul PFD reale
`efis-pfd-01` con la release immutabile `60b3526`.

## Perimetro della verifica

- target: Raspberry Pi 5, renderer Wayland, display 1024×600;
- modalità: preview da banco, dati e pagina esplicitamente `SIM`;
- famiglie: perdita potenza, assetto anomalo, velocità non sicura, anomalia
  elettrica, fumo/incendio, meteo/terreno, perdita sensori/PFD e
  incapacità pilota;
- risultato: tutte le catture hanno restituito `saved=true`;
- il comando di preview non aveva la working directory della release e ha
  quindi usato profilo e cataloghi fallback; il successivo avvio del servizio
  operativo ha ricaricato profilo, cataloghi e imagery corretti;
- le immagini dimostrano rendering e routing delle otto famiglie. Non
  dimostrano interazione touch, intelligibilità audio, ergonomia accettata,
  validità di procedure POH/AFM o capacità di comando del velivolo.

La verifica associata comprende inoltre 24 transizioni UI persistite sulla
Blackbox. Due record aggiuntivi sono stati prodotti separatamente per la prova
di fault del trasporto; non corrispondono a ulteriori schermate.

## SHA-256

```text
0034baf0814d9b6350f6593e36a0bd1be8bc58149dcdcfb55b6c2ea4a13903ec  ABNORMAL_ATTITUDE.png
15e2dbef3931f906aaf334090bbcfffd9a0395919d023de7dfa2d32dc88d4f1b  ELECTRICAL_FAILURE.png
74fe28ac03d20a14625d865764001f55c820d5d4ca60607de3cf77f46fc62081  ENGINE_POWER_LOSS.png
10bdbce43304b327502936480fb91588f1b60c54923dc91cd939b78215acb8a2  PILOT_INCAPACITATION.png
6d47264d62ab4450a2a90cae39adb571146238350097af7df58ccc10c63ebfb8  SENSOR_OR_PFD_LOSS.png
8ca1efa0205278c4cbea38de2b8c3913717f9e878c4a65d95fea57344e28e176  SMOKE_OR_FIRE.png
0bfbd016c4e8a4f350bc6633b2c42250939b70cf843d0886dfc6834e109e5608  UNSAFE_AIRSPEED.png
ac86b78c06fcf71a8fa29f763d74f6aa7b34cdb01c10f1049aac2abab18d5f4c  WEATHER_OR_TERRAIN.png
```
