# Release immutability — hardware evidence 2026-09-20

Scope: authorized migration of PFD/recorder `efis-pfd-01`, MFD `pi5`, EMS
`efis` and Blackbox `efisblackbox` to the immutable-release V1 contract.
Product status remains
DEVELOPMENT / NON-CERT and `flight_approval=false` in every manifest.

| Node | Primary release | Platform | Rollback drill | Reboot acceptance | Result |
|---|---|---|---|---|---|
| MFD `.49` | `mfd-3432402-primary-v1` | RPI5_AARCH64 | PASS | PASS | PASS |
| EMS `.36` | `ems-7655799-primary-v1` | RPI4_AARCH64 | PASS | PASS | PASS |
| Blackbox `.47` | `blackbox-7655799-primary-v1` | RPI5_AARCH64 | PASS | PASS | PASS |
| PFD + recorder `.45` | `pfdrec-d52e377-primary-v1` | RPI5_AARCH64 | PASS | PASS | PASS |

EMS evidence additionally proves that mutable engine runtime, fuel-event and
capture files are outside the read-only release. The rebooted process listened
on `10.77.10.12:49003`, accepted EDP from Blackbox node 47 and reported
`LIVE • SIM`; SIM is the explicit payload provenance, not a real-engine test.

The directories below contain node-produced systemd, validation, digest,
rollback and journal records. `ems/post-reboot-acceptance.txt` records the
final independent read-only verification. Historical installations were not
deleted and remain recovery material.

The PFD/recorder directory also records the two fail-closed recovery events,
the successful final drill, the expected one-time Wayland readiness retry and
the changed boot ID. No failed attempt was discarded from the audit trail.

This bundle closes the immutable update/rollback rollout for the managed
avionics processes PFD, recorder, MFD, EMS and Blackbox. Requirement
`PROD-005` is DONE for that contract; this does not close visual, sensor,
power, endurance or flight-approval gates.
