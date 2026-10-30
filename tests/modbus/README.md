# Modbus map regression tests

Run `python3 tests/modbus/run_tests.py` with `g++` on Linux, or with MSVC C++ tools
on Windows. If MSVC is incomplete, the runner can use the installed .NET
WebAssembly workload and its bundled Node runtime instead. Build products stay
in `tests/modbus/build/` (git-ignored).

The test compiles the real `HeishaModbusServer.cpp`, real topic units and command
tables. Small host shims replace Arduino, network transport, telemetry decoding
and heat-pump execution, so tests cannot operate hardware. It checks every
implemented topic/command, float word order, fixed scaling, expansion to full
block capacity, invalid addresses/counts, signed writes, relay selection and
the register page's row enumeration, plus both S0 inputs (scaling, report energy,
status, invalid configuration and non-destructive reads). It also checks that
writes are queued and only executed by `loop()`, that writes are refused unless
allowed, x100 optional PCB temperature writes, exceptions for unavailable extra
and optional PCB registers, and the Loxone template.

These tests complement the ESP32 firmware build; they do not validate real
network timing or operation with a heat pump.
