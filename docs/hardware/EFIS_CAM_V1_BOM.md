# EFIS CAM V1 prototype bill of materials

Status: purchasing shortlist. Exact manufacturer part numbers are frozen only
after one HDR and one global-shutter module pass bench evaluation.

## One CAM-HDR node

| Item | Baseline | Qty | Selection note |
| --- | --- | ---: | --- |
| Sensor | VEYE GX-MIPI-IMX662 | 1 | integrated ISP, MIPI YUV output, M12 option |
| Compute | CM4, 2 GB RAM, 16 GB eMMC, no wireless | 1 | predictable storage and no operational radio |
| Prototype carrier | compact CM4 carrier with CSI and Gigabit Ethernet | 1 | development only; custom carrier follows |
| Lens | low-distortion M12, 2.8-3.2 mm, F/1.8 or faster, IR cut | 2 | evaluate 90-100 and 105-115 degree options |
| Power | isolated 9-32 V DC/DC or IEEE 802.3af PoE module | 1 | choose common aircraft architecture after test |
| Thermal | machined aluminium spreader and thermal pad | 1 | conduct heat to enclosure |
| Enclosure | prototype aluminium body, optical glass, O-ring, vent | 1 | IP testing follows design freeze |
| Connector | shielded locking Ethernet/power connector | 1 | no exposed RJ45 in final pod |
| Retention | lens lock, fastener locking and safety-wire point | 1 set | mandatory before vibration test |

## One CAM-GS node

Use the same compute, power, network and enclosure architecture. Substitute a
colour AR0234 or OG02B10 global-shutter M12 module. Buy both sensor candidates
only if driver and image-quality evidence cannot select one before purchase.

## Shared prototype equipment

- Managed Gigabit switch with four individually controlled PoE ports or four
  separately fused outputs.
- Shielded bench harness and representative aircraft-length cables.
- Calibrated latency target: LED plus photodiode/display measurement or a
  high-frame-rate reference camera.
- Temperature probes, current logger and adjustable 9-32 V supply.
- Vibration fixture with a mechanically retained test enclosure.
- Neutral-density and high-contrast test charts for sky/terrain HDR tuning.

## Custom carrier requirements after proof of concept

- CM4 board-to-board connectors and one CSI-2 input.
- Gigabit Ethernet PHY/magnetics or PoE powered-device front end.
- Protected 9-32 V input option: reverse polarity, surge, transient and ESD.
- Hardware watchdog independent of Linux.
- Board temperature and supply-voltage monitoring.
- One service UART, one sync input and one sync output.
- Controlled shutdown input and boot-status output.
- No HDMI, USB-A, audio or wireless components on production hardware.

## Procurement gate

The first purchase is limited to one IMX662 kit, one global-shutter kit, two
CM4 modules/carriers, two lens options per sensor and bench power/network
hardware. The remaining two camera nodes are purchased only after Gate B in the
test plan, preventing a four-unit commitment before latency and image quality
are proven.
