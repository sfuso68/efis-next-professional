# ADR-001: C++20 e CMake

- Stato: accettata
- Data: 2026-08-17

## Decisione

Il core usa C++20 e CMake. Le API pubbliche del core non dipendono da Qt, QML,
LVGL, OpenGL, MAVLink o hardware specifico.

## Conseguenze

I provider traducono protocolli e hardware nei modelli EFIS. I renderer consumano
snapshot e view model senza diventare proprietari dello stato avionico.
