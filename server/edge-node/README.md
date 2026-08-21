# EFIS EDGE V2

Shadow-mode services for the dedicated Blackbox node. They are installed beside,
and never replace, the existing `bbxlog`, `bbxmqtt`, `blackbox.target` or FUSOBBX1
archive.

Initial guarantees:

- read-only observation of CAN and existing services;
- no AF_CAN socket and no CAN transmission path;
- no flight-critical consumer depends on this service;
- atomic health status publication;
- loopback-only API until authentication and network policy are approved.

Run a one-shot audit:

```sh
python3 edge_health.py --once --output run/edge-health.json
```

Run the shadow API:

```sh
python3 edge_health.py --bind 127.0.0.1 --port 18091 --output run/edge-health.json
```
If local DNS does not publish the central server, use
`--resolve mappe.casa.lan=192.168.1.6`. HTTPS still validates the certificate
for `mappe.casa.lan`; this does not disable TLS verification.
