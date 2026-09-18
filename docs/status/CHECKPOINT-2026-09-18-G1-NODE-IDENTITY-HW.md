# Checkpoint — G1 identità nodo sensori — 2026-09-18

## Verificato su hardware reale

- Nodo: `ems-sensor`, IPv4 Wi-Fi `192.168.1.89`.
- Raspberry Pi 4 Model B Rev 1.5, 8 GB, `aarch64`.
- Debian 12 Bookworm, kernel `6.12.109+rpt-rpi-v8`.
- microSD 53,7 GB; root ext4 con circa 45 GB disponibili.
- Nessun throttling (`0x0`); temperatura post-riavvio 44,8 °C.
- I²C GPIO abilitato con backup reversibile della configurazione.
- `/dev/i2c-1` presente; GPIO2=`SDA1`, GPIO3=`SCL1`.
- Utente `pi` membro del gruppo `i2c`.

## Modifica hardware/software eseguita

- Decommentato esclusivamente `dtparam=i2c_arm=on` in
  `/boot/firmware/config.txt` e riavviato il nodo.
- Backup conservato in
  `/boot/firmware/config.txt.pre-avionix-i2c-20260918`.
- Nessuna scansione di indirizzi, lettura vendor, scrittura registro, comando
  uscita o modifica DIP/cablaggio.

## Evidenza

- Report: `evidence/g1-node-identity-ems-sensor-20260918/node-report.txt`.
- SHA-256:
  `7f63177f97e119d50e3c31ab9b4db98f226e4abecf63b04f273433b166bf7bc1`.
- Raccolta UTC: `2026-09-18T18:49:45Z`.

## Aperto / non dichiarare DONE

- Modello e rating dell'alimentatore non acquisiti.
- Ethernet `eth0` è senza carrier; l'IP avionico non è configurato.
- Schede Sequent verificate: 0/4; mancano fotografie, modelli, revisioni, DIP,
  slot fisici e connettori.
- Nessun indirizzo I²C o driver è qualificato.
- `AVX-G1-01` passa a `IN_PROGRESS`; `AVX-G1-02` resta `OPEN` e
  `AVX-G1-03` resta `IN_PROGRESS` soltanto per il kernel software.
