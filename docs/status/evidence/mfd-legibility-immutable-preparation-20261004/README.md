# MFD legibility immutable release — software preparation

Date: 2026-10-04
Status: PREPARED-SW / NOT RUN ON HARDWARE / DEVELOPMENT-NON-CERT

## Identity

- Candidate source commit:
  `76afead639599a08cb187bf93f1127c84a4df9ed`
- Candidate release id: `mfd-76afead-legibility-v1`
- Candidate target: `/home/pi/efis-next-release-mfd-76afead-v1`
- Rollback target: `/home/pi/efis-next-release-mfd-3432402-v1`
- Source archive: `efis-next-76afead.tar.gz`
- Archive bytes: `50548958`
- Archive SHA-256:
  `b318d9da3914826cdc4c5a07b5803be25d17973b4685df64633646a053ee740b`
- Target node/role/platform: `pi5` / `MFD` / `RPI5_AARCH64`

The archive was generated twice locally with `git archive`; both files had the
same byte length and SHA-256. The archive itself is not committed.

## Implemented assets

- `deploy/scripts/prepare_mfd_legibility_immutable_release.sh`
- `deploy/scripts/activate_mfd_legibility_immutable_release.sh`
- `deploy/scripts/verify_mfd_legibility_reboot_acceptance.sh`
- corrected canonical drop-in
  `deploy/systemd/release-v1/efis-mfd-release-v1.override.conf`

The preparation does not switch the active symlink or restart systemd and
prints `EXECUTION_AUTHORIZED=false`. Activation is fail-closed on the exact
rollback target, node identity, AArch64, one display supervisor and
`get_throttled=0x0`. It restores the previous symlink and drop-in on error.

## Software verification

- Bash syntax for the three scripts: PASS.
- Release/supervision/paired-display tests: 37 PASS, 1 Windows symlink SKIP.
- `tools/project_status/generate_status.py --check`: PASS.
- `git diff --check`: PASS.

## Hardware verification

None. No SSH connection, build, installation, symlink switch, service restart,
rollback, reboot or visual/touch acceptance was performed on `192.168.1.49`.
The active release remains `mfd-3432402-primary-v1` according to the latest
canonical evidence.

## Blocking gates

1. Qualify MFD PSU/cable and obtain `vcgencmd get_throttled = 0x0` after a
   clean power cycle. Historical `0x50000` is not accepted by activation.
2. Obtain a separate bench-window authorization.
3. Transfer the exact archive and run preparation as `pi`.
4. Review the generated manifest and transition evidence before activation.
5. Run activation, real rollback, reboot acceptance and operator visual/touch
   acceptance at 1024×600.
6. Do not prepare the primary PFD until the MFD gate passes.

`flight_approval=false` remains mandatory.
