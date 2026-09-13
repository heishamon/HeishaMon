# AGENTS.md

This file provides guidance when working with code in this repository. Before writing or editing any rules-engine file (`rules.txt`, `src/rules/`), read the `heishamon-rules` skill at `Examples/Rules/SKILL.md` first.

## What this is

HeishaMon is Arduino/C++ firmware for ESP8266 (small PCB) and ESP32-S3 (large PCB) boards that speaks the Panasonic Aquarea (H/J/K/L series) heat pump's proprietary serial (CN-CNT) protocol, decodes it, and republishes it as MQTT topics and a JSON HTTP endpoint. It also lets you *send* commands back to the heat pump, run a small onboard rules engine to automate the heat pump without relying on external automation software, and (ESP32 only) proxy/pass-through the original Panasonic CZ-TAW1 controller.

The whole firmware lives in one Arduino sketch directory: `HeishaMon/`. There is no separate backend/frontend split — the web UI is served from PROGMEM HTML strings embedded in the firmware itself.

## Build commands

This is an Arduino sketch, built with `arduino-cli` (not `make`/`cmake`). Helper scripts wrap the exact `arduino-cli compile` invocations used in CI:

```bash
scripts/build_esp8266.sh   # builds HeishaMon/HeishaMon.ino for the small (Wemos D1 mini / ESP8266) PCB
scripts/build_esp32s3.sh   # builds HeishaMon/HeishaMon.ino for the large (ESP32-S3) PCB
```

Both scripts `cd HeishaMon` first and output the compiled binary/map alongside the sketch. Required libraries (installed via `arduino-cli lib install`, see `LIBSUSED.md` and `.github/workflows/main.yml`): `ringbuffer`, `pubsubclient`, `arduinojson`, `dallastemperature`, `onewire`, `Adafruit NeoPixel`. Board cores needed: `esp8266:esp8266` and `esp32:esp32@3.0.7`.

There is a devcontainer (`.devcontainer/`) preconfigured with `arduino-cli` and these dependencies (image `ghcr.io/the78mole/heishamon-dev`) — prefer developing/building inside it if available, since board cores and libs are already installed there.

There is no unit test suite for the firmware itself. Verification is: does it compile for both boards, and (when possible) manual testing against real hardware or logged serial captures. `Tools/chksumChecker.js` is a standalone Node script for computing/verifying checksums of raw heat pump command packets documented in the README — run with plain `node Tools/chksumChecker.js`.

The **rules engine and example rulesets do have tests**: `Examples/Rules/run_tests.sh` builds a host-side harness (`Examples/Rules/harness/`, plain `g++`, no Arduino toolchain) that compiles the real rules engine for Linux and runs each example's `tests/` scenarios against it, asserting the exact `@Set…` command stream. Run it after any change to `src/rules/`, `src/common/timerqueue.cpp`, the decode/command tables, or an example ruleset.

### Pre-commit / formatting

`.pre-commit-config.yaml` runs: trailing-whitespace, end-of-file-fixer, check-yaml, byte-order-marker fix, mixed-line-ending fix, `black`+`isort` (for the rare Python file), and `clang-format` on `*.c`/`*.cpp`/`*.h`/`*.hpp`. Run `pre-commit run --all-files` before committing C/C++ changes so formatting matches CI expectations.

## Architecture

### Two hardware targets, one codebase

`HeishaMon.ino` and the other `.cpp`/`.h` files branch heavily on `#ifdef ESP8266` / `#ifdef ESP32`. Key differences to keep in mind when editing shared code:

- **ESP8266** ("small" board): heat pump serial is the primary `Serial`; single-threaded `loop()`; no proxy port; TLS is opt-in.
- **ESP32-S3** ("large" board): heat pump serial is `Serial1`; a dedicated FreeRTOS task (`serialTXTask`, started via `xTaskCreatePinnedToCore` in `setupConditionals()`) owns all outbound serial writes to the heat pump, communicating with the main loop via `pcbQueue`/`cmdQueue`/`logQueue`; has a second UART (`proxySerial`) to transparently forward/inspect traffic to/from the original CZ-TAW1 controller (`readProxy()`); has an onboard NeoPixel status LED and optional W5500 Ethernet (`setupETH()`); TLS is always on.

When adding a feature that touches serial I/O or settings, check both branches compile — the build scripts above cover exactly that.

### Data flow

1. **`readSerial()` / `readProxy()`** (`HeishaMon.ino`) read raw bytes from the heat pump (and, on ESP32, the proxy port), validate the header/length/checksum (`calcChecksum`, `isValidReceiveChecksum`), and assemble one of three fixed-size datagrams: full data block (`DATASIZE`=203, header byte `0x10`), extra data block (header byte `0x21`, K/L-series only, gated by `extraDataBlockAvailable`), or the optional-PCB ack (`OPTDATASIZE`=20).
2. **`decode.cpp`/`decode.h`** turn a raw datagram into individual values and publish each one to its own MQTT subtopic (`decode_heatpump_data`, `decode_heatpump_data_extra`, `decode_optional_heatpump_data`). Each decoded field is described by a table-driven topic list (`xtopics`, `optTopics`, per-byte decoder functions like `getIntMinus1`, `getOpMode`, `getBit1and2`, etc.) — adding a newly-discovered protocol field is primarily a matter of extending these tables, not writing new parsing logic. `ProtocolByteDecrypt.md` documents the raw byte layout this table encodes.
3. **`commands.cpp`/`commands.h`** go the other direction: each settable heat pump parameter has a `set_*` function that builds a raw command byte array from a string value (MQTT payload or web form field), keyed by name in the `commands[]`/`optionalCommands[]` tables. `send_command()` (or the ESP32 `cmdQueue`) then transmits it; the checksum is (re)computed automatically.
4. **MQTT and web/REST** are just two front ends onto the same decode/command tables: `mqtt_callback()` in `HeishaMon.ino` dispatches inbound MQTT messages by topic prefix (`commands/`, `gpio/`, `opentherm/`, `s0/...`, raw-send) to the same handlers the web `/command` route uses (`webserver_cb()` route 100).

### Web server

There's no framework — `webfunctions.cpp` implements a small custom HTTP server on top of `src/common/webserver.*` via a single callback `webserver_cb()` (declared in `HeishaMon.ino`) that switches on `client->route` (an integer assigned per-URL in the `WEBSERVER_CLIENT_REQUEST_URI` case) and `client->step` (request-method / URI / args / header / write phases). All served HTML is static PROGMEM strings in `htmlcode.h`; there is no templating engine — dynamic values are injected with `sprintf`-style formatting straight into the response body. Settings persistence goes through `settingsStruct` (defined in `webfunctions.h`) serialized to `config.json` on LittleFS via ArduinoJson (`settingsToJson`/`loadSettings`).

### Rules engine (`src/rules/`, plus root `rules.cpp`/`rules.h`)

A small custom scripting language (syntax documented in `README.md` under "Rules functionality", and in the `heishamon-rules` skill at `Examples/Rules/SKILL.md`) that lets the device react to heat pump/thermostat values and timers without external automation. `src/rules/rules.cpp` is the parser/interpreter; `src/rules/function.cpp` and `src/rules/operator.cpp` dispatch built-in functions and operators; `src/rules/functions/*.cpp` implement individual built-ins (`coalesce`, `min`, `max`, `isset`, `round`, `floor`, `ceil`, `print`, `concat`, `gpio`, `settimer`), each a small self-contained file — add new rule functions by dropping in a matching pair here and registering it in `function.cpp`. Rulesets are validated before being applied (invalid rulesets are rejected and the previous ruleset kept); a ruleset that crashes the device is auto-disabled on next boot to avoid boot loops. Root-level `rules.cpp`/`rules.h` glue the engine into the main sketch (loading/saving `rules.txt` on LittleFS, wiring heat pump/OpenTherm/1-wire/S0/GPIO values into the engine's variable namespaces described in the README: `#` globals, `$` locals, `@` heat pump params, `%` datetime, `?` thermostat, `ds18b20#...`, `s0#...`). `Examples/Rules/` holds real, worked example rulesets — before writing or editing any `rules.txt`, load the `heishamon-rules` skill.

Rulesets can be validated and behavior-tested **off-device**: `Examples/Rules/harness/` compiles this engine for the host PC (`./build.sh`, needs only `g++`/`python3`) and runs the same `rule_initialize()` parse/validation as an on-device upload, including `@Name` checks against the real `decode.h`/`commands.h` tables. Its scenario mode simulates topic changes, timers (faithful to `timerqueue.cpp` — `setTimer(id, 0)` cancels, it never fires), and `%hour`/`%minute` schedules, with `expectcmd`/`expectnone` assertions on the emitted commands. Each example ruleset keeps self-checking regression scenarios in its `tests/` directory; `Examples/Rules/run_tests.sh` runs them all. Always run the tests after editing a ruleset, and add/extend scenarios when adding rule behavior — see `Examples/Rules/harness/README.md` for the scenario language.

### Other subsystems

- **`dallas.cpp`/`dallas.h`**: DS18B20 1-wire temperature sensors on GPIO4, aliasing support, periodic MQTT resend.
- **`s0.cpp`/`s0.h`**: S0 kWh-meter pulse counting on GPIO12/GPIO14, with persisted running totals (restorable from a retained MQTT value on boot).
- **`gpio.cpp`/`gpio.h`**: generic extra GPIO configuration/control, also reachable from rules via the `gpio()` function and from MQTT (`gpio/` topic prefix).
- **`HeishaOT.cpp`/`HeishaOT.h`**: OpenTherm thermostat integration (`?`-prefixed rule variables, `mqttOTCallback`).
- **`src/common/`**: small vendored/shared utilities (base64, sha1, string helpers, a memory pool (`mem.cpp`) sized via `MEMPOOL_SIZE` for the rules engine, `timerqueue` for the rules engine's `setTimer`, and the custom `webserver.*` HTTP layer used by `webfunctions.cpp`).

### Versioning and releases

`HeishaMon/version.h` holds `HEISHAMON_VERSION`; CI (`.github/workflows/main.yml`) overwrites it based on ref (tag → `Release`/tag name, scheduled/dispatch → `Nightly-<sha>`, otherwise `Alpha-<sha>`) and never runs a plain build on a bare push to `main`. Compiled binaries are named `HeishaMon_ESP8266-<type>-<md5>.bin` / `HeishaMon_ESP32-<type>-<md5>.bin`. `binaries/` in this repo holds released firmware for end users, split into `model-type-small` (ESP8266) and `model-type-large` (ESP32-S3).

## Reference docs worth knowing about

- `README.md` (+ `README_DE.md`/`README_NL.md`/`README_FI.md` translations) — user-facing setup/usage and the full rules-engine syntax reference.
- `MQTT-Topics.md` / `Manage-Topics.md` — full list of published/settable MQTT topics.
- `ProtocolByteDecrypt.md` / `ProtocolByteDecrypt-extra.md` — raw serial protocol byte-level documentation that `decode.cpp`'s tables implement.
- `HeatPumpType.md` — which heat pump models/series are confirmed supported.
- `Integrations/` — example configs for Domoticz, Home Assistant, Node-RED, openHAB (two submodules under here: `Integrations/Domoticz/HeishamonMQTT` and `Integrations/NodeRed`).
