# Treviso open obstacle layer

`liph-open-obstacles.json` is an offline OpenStreetMap/Overpass extract for
45.59,12.10,45.72,12.29. It contains mapped towers, masts, chimneys and power
towers and is licensed under ODbL.

This layer is for visual simulation and software development only. Completeness,
position, height and update status are not guaranteed. It must never replace
current authorised AIP AD 2.10 or eTOD obstacle data.

Rendering is disabled by default and may only be enabled during development
with `EFIS_EXPERIMENTAL_OBSTACLES=1`. It will remain disabled operationally
until obstacles, terrain and runway share the same verified 3D camera transform.
