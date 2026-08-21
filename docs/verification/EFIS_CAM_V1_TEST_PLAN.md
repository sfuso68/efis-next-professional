# EFIS CAM V1 verification plan

Status: prototype gates; completion does not constitute aeronautical approval.

## Gate A - component proof

- [ ] Detect IMX662 and global-shutter modules after 25 cold boots each.
- [ ] Capture 1080p30 continuously for two hours without process restart.
- [ ] Demonstrate fixed focus from 2 m to infinity and lock the lens.
- [ ] Compare available lenses for distortion, flare and useful field of view.
- [ ] Verify HDR transition from dark terrain to direct bright sky.
- [ ] Confirm the forward global-shutter image has no unacceptable propeller or
      vibration deformation.

Pass condition: both sensor candidates are stable and one lens is selected for
each role.

## Gate B - network and latency

- [ ] Operate two nodes over representative cable lengths at 1080p30 H.264.
- [ ] Measure capture-to-display latency, not network latency alone.
- [ ] Achieve <= 180 ms median and <= 250 ms 99th percentile for 30 minutes.
- [ ] Remove and restore each cable; recovery must be automatic.
- [ ] Inject 1%, 3% and 5% loss and verify correct quality annunciation.
- [ ] Verify timestamps against EFIS telemetry within 100 ms.

Pass condition: Gate B authorises purchase/build of CAM-03 and CAM-04.

## Gate C - four-channel integration

- [ ] Decode four main streams concurrently on the RPi4 Video Core.
- [ ] Display single, 2x2 mosaic and picture-in-picture modes.
- [ ] Record all original streams plus telemetry sidecars for two hours.
- [ ] Keep aggregate configured traffic <= 40 Mbit/s.
- [ ] Disconnect one camera without frame disruption on the other three.
- [ ] Confirm forward-camera loss triggers the configured auto-revert.
- [ ] Verify storage-full behaviour preserves EFIS operation.

## Gate D - electrical, thermal and EMC pre-compliance

- [ ] Start and operate at representative minimum/nominal/maximum bus voltage.
- [ ] Test controlled brownout, cranking dip and power interruption recovery.
- [ ] Record enclosure, sensor and compute temperatures at ambient extremes.
- [ ] Verify thermal protection produces a warning before stream shutdown.
- [ ] Check conducted/radiated interference against COM, GNSS and magnetometer.
- [ ] Confirm a shorted or failed node is isolated by its individual protection.

## Gate E - mechanical and installation qualification

- [ ] Inspect bracket load paths with the responsible aircraft technician.
- [ ] Verify all fasteners, lens, window and connector have secondary retention.
- [ ] Conduct vibration survey, then qualification using measured spectra.
- [ ] Inspect for focus shift, connector fretting, cracks and water ingress.
- [ ] Test condensation and rapid temperature transition.
- [ ] Reweigh the complete installed assembly and update weight and balance.
- [ ] Obtain the required installation approval before flight operation.

## Evidence package

Each test run stores configuration revision, serial numbers, firmware versions,
calibration data, raw measurements, pass/fail result, operator and UTC time.
Failures remain open until a repeated test demonstrates closure; no failure may
be closed by explanation alone.
