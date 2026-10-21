# R1-04 — Engine channels and initial fuel quantity (N7/N8)

2026-10-21, candidate software, no deployment or flight approval.
Requirements: CORE-002, EMS-001, EMS-003, VER-001.
Branch `fix/engine-channel-validity-v1` is stacked on PR #53, source
`3004218d8ed3e6bc9edc9c3b0c4aa3aad5adc12a`. That PR is not assumed merged.
The independent weather PRs #54/#55 are not included.

## Implemented contract

- Shared core helpers accept only usable, finite engine samples. Unavailable,
  stale, out-of-range, disagree and nonfinite residuals become NaN at the
  presentation boundary, never zero or a previous plausible reading. Degraded
  and explicitly simulated usable samples remain distinguishable by source
  state. The transport/arbitration model is unchanged.
- Both native controllers expose ten scalar `*Valid` properties and mask all
  ten scalar readings plus every CHT/EGT array element. Each cylinder remains
  independent: a missing CHT does not hide the other seven temperature values.
- Gauges draw a dash and remove the needle for invalid readings. Full EMS and
  PFD/Canvas deck honor engine state, scalar validity and numeric finiteness.
  Fuel sides and electrical readings are independently gated; totals require
  both tanks. Missing/stale source data cannot be made usable by opacity alone.
- The fuel baseline waits for two usable, finite, nonnegative tank samples,
  finite/nonnegative calibrated results and matching real/SIM provenance.
  Invalid startup is retried on subsequent updates. An existing baseline or
  explicit pilot entry is not overwritten by later measurements.
- Capturing a new measured baseline resets the corresponding consumption
  accumulator and integration time, avoiding subtraction of fuel already used
  before that baseline. Unknown baseline is exposed as unavailable, not zero.
- Automatic SIM quantities are not persisted as pilot/real fuel settings and
  are invalidated when leaving the SIM tank domain. Saved baseline values must
  parse as finite values in the pre-existing 0–100 gallon setting range.
  This does not retroactively prove provenance of legacy saved settings: those
  still require pilot reconfirmation during migration/acceptance.
- Leak monitoring resets its derivative chain on missing tank/RPM/flow inputs;
  invalid sensor values cannot become calibration points. Lean assist refuses
  missing EGT channels or stale data. Trend gaps are not substituted by zero.
  CAS/checklist uses channel validity; missing channels do not assert SYSTEM
  NORMAL, but a valid critical alarm retains priority over missing-channel
  advice. Existing advisory/action wording is not approved flight procedure.

## Evidence and limits

`../status/evidence/r1-04-engine-20261021/verification.json` records actual
results. Core tests include invalid-first/valid-later fuel initialization,
single-channel failure, finite checks, mixed provenance and legitimate zero.
Two negative mutations remove the channel/baseline guard and must fail.
Runtime Qt Quick tests instantiate full EMS and the PFD deck, inspect actual
text/needle items and retain the independent flight/engine-state regression.
They run from source, not from an installed ARM64 application.

No new real EDP consumer is added to the PFD; that remains R1-07. No real
sensor, calibration, physical display, CAN/EDP end-to-end loss/recovery or
power qualification is claimed. Full native controller integration and
persistent-state migration acceptance remain open. Golden baseline images
are not regenerated or silently accepted by this change.

Next: CI/review of the stacked changes, then R1-05 (METAR/TAF freshness and
observational radar age). Node activation stays blocked pending power gates.
