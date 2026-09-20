# Release immutability — hardware evidence 2026-09-20

Scope: authorized migration of MFD `pi5`, EMS `efis` and Blackbox
`efisblackbox` to the immutable-release V1 contract. Product status remains
DEVELOPMENT / NON-CERT and `flight_approval=false` in every manifest.

| Node | Primary release | Platform | Rollback drill | Reboot acceptance | Result |
|---|---|---|---|---|---|
| MFD `.49` | `mfd-3432402-primary-v1` | RPI5_AARCH64 | PASS | PASS | PASS |
| EMS `.36` | `ems-7655799-primary-v1` | RPI4_AARCH64 | PASS | PASS | PASS |
| Blackbox `.47` | `blackbox-7655799-primary-v1` | RPI5_AARCH64 | PASS | PASS | PASS |

EMS evidence additionally proves that mutable engine runtime, fuel-event and
capture files are outside the read-only release. The rebooted process listened
on `10.77.10.12:49003`, accepted EDP from Blackbox node 47 and reported
`LIVE • SIM`; SIM is the explicit payload provenance, not a real-engine test.

The directories below contain node-produced systemd, validation, digest,
rollback and journal records. `ems/post-reboot-acceptance.txt` records the
final independent read-only verification. Historical installations were not
deleted and remain recovery material.

This bundle closes only the authorized MFD/EMS/Blackbox rollout. PFD and its
recorder remain outside this hardware batch, therefore global requirement
`PROD-005` remains PARTIAL.
