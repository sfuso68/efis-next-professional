# EFIS CAM V1

Status: design baseline for prototype, not approved flight hardware.

## Purpose

EFIS CAM V1 is a repairable, software-controlled camera node for EFIS Next. It
replaces a large commercial IP camera with a compact sensor, local encoder and
wired network interface. Video remains an aid to situational awareness and is
never the sole source of primary flight information.

## Product variants

| Variant | Sensor baseline | Intended position | Priority |
| --- | --- | --- | --- |
| CAM-HDR | Sony IMX662 STARVIS 2, rolling shutter | left, right, rear/belly | P0 |
| CAM-GS | 2 MP colour global shutter, AR0234 or OG02B10 | forward/propeller view | P0 |
| CAM-GIMBAL | CAM-HDR remote sensor head | movable inspection view | P2 |

The prototype processing node is a Raspberry Pi Compute Module 4 with eMMC.
The sensor-to-compute MIPI connection stays inside the enclosure. Aircraft
wiring carries power and shielded Ethernet only.

## Required performance

- 1920 x 1080 at 25 or 30 frames/s in operational mode.
- Optional 50/60 frames/s engineering mode.
- H.264 low-latency stream over RTSP/RTP; H.265 is optional, never mandatory.
- Glass-to-glass latency target <= 180 ms; warning above 250 ms.
- Horizontal field of view selectable between 90 and 115 degrees.
- Fixed focus from 2 m to infinity after mechanical locking.
- HDR profile for simultaneous sky and terrain visibility.
- Four simultaneous streams on wired Gigabit Ethernet.
- Target encoded rate 4-8 Mbit/s per stream, configurable.
- PTP/NTP time synchronisation and monotonic frame timestamps.
- Local hardware watchdog and automatic stream recovery.
- Node health every second: temperature, voltage, uptime, dropped frames,
  encoder state and synchronisation quality.

## Mechanical and environmental targets

- Target complete mass <= 180 g excluding aircraft harness and bracket.
- Target envelope <= 60 x 60 x 90 mm for fixed cameras.
- CNC aluminium heat-spreading body with replaceable optical window.
- M12 lens with locking ring and secondary retention.
- Captive or positively locked aircraft connector and independent safety wire.
- Condensation vent; optional 2-5 W window heater.
- Design targets: -20 to +60 degrees C operating, IP65 minimum, IP67 objective.
- No installation on primary structure without an approved structural design.

These are engineering targets. Environmental, EMC, vibration and installation
tests are required before any airborne operational use.

## Safety behaviour

- Loss, freezing or excessive latency of video raises VIDEO DEGRADED.
- The FPV page exposes stream age and never hides stale frames.
- No camera failure may reset, overload or remove power from EFIS Core/PFD.
- Camera power is individually protected and remotely switchable.
- At the operational latency limit the UI automatically reverts to the PFD if
  the pilot has selected auto-revert.
- Video and overlays are recorded as separate data so original imagery remains
  available to the Black Box.

## Network allocation

| Node | Role | Reserved address | Stream name |
| --- | --- | --- | --- |
| CAM-01 | forward global shutter | 10.77.20.11 | forward |
| CAM-02 | left HDR | 10.77.20.12 | left |
| CAM-03 | right HDR | 10.77.20.13 | right |
| CAM-04 | rear/belly HDR | 10.77.20.14 | rear |
| VIDEO-CORE | decode, mosaic, record | 10.77.20.2 | n/a |

The video VLAN is isolated from flight-critical sensor traffic. Wi-Fi is not an
operational video transport; it is permitted only for maintenance.

## Acceptance gate for V1

V1 passes only when four streams operate for two continuous hours, no node
exceeds its thermal limit, stream latency stays within the declared envelope,
loss of any one node does not disturb the other three, and recorded video can
be correlated with EFIS telemetry to within 100 ms.
