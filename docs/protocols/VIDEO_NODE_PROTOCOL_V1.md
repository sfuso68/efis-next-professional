# Video Node Protocol V1

## Transport separation

- Video: RTSP control with RTP/UDP primary and RTP/TCP fallback.
- Health and configuration: HTTPS JSON API.
- Discovery during maintenance: mDNS; operational discovery uses the aircraft
  configuration and fixed addresses.
- Time: PTP preferred, NTP fallback. Every access unit carries capture time.

## Stream endpoints

```text
rtsp://<node>/live/main   1080p25/30 operational stream
rtsp://<node>/live/low    640x360 maintenance/mosaic fallback
https://<node>/api/v1/status
https://<node>/api/v1/config
https://<node>/api/v1/actions/restart-stream
```

The main stream uses H.264 High or Main profile, no B-frames, a one-second or
shorter keyframe interval, fixed frame cadence and bounded encoder buffering.

## Status contract

```json
{
  "schema": "efis.video.status/1",
  "nodeId": "CAM-01",
  "role": "forward",
  "state": "streaming",
  "captureUtc": "2026-08-19T10:15:30.125Z",
  "clockOffsetMs": 1.7,
  "sensorTemperatureC": 48.2,
  "boardTemperatureC": 55.1,
  "inputVoltageV": 13.8,
  "fps": 29.97,
  "bitrateKbps": 5980,
  "droppedFrames": 3,
  "streamSequence": 49281,
  "profile": "flight-hdr"
}
```

## Quality states

| State | Condition | EFIS action |
| --- | --- | --- |
| NOMINAL | age <= 180 ms and healthy | normal presentation |
| CAUTION | age 181-250 ms or packet loss >= 1% | amber latency/quality flag |
| DEGRADED | age > 250 ms, clock invalid or repeated freeze | prominent VIDEO DEGRADED |
| LOST | no valid frame for 1000 ms | remove imagery, retain instruments |

Frame freeze detection combines receive age, RTP sequence progress and image
hash change. A static scene alone must not be classified as frozen.

## Configuration ownership

EFIS setup owns role, address, operational profile and orientation. The camera
node owns sensor tuning defaults and hardware limits. Flight mode configuration
is read-only to the pilot; maintenance mode requires authenticated access and
records every change in the Black Box configuration audit.

## Versioning

Unknown additive fields are ignored. A different major schema version is
rejected and reported as VIDEO CONFIG INCOMPATIBLE. Secrets and credentials are
never stored in the aircraft profile JSON.
