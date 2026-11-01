# Third-party notices

HeishaMon itself is licensed under the [Mozilla Public License 2.0](LICENSE). The firmware also contains, or is linked with, the third-party code listed below. Each part keeps its own license, and those licenses have to be respected when you redistribute HeishaMon source code or firmware binaries.

## Code included in this repository

| Files | Author | License |
| ---- | ---- | ---- |
| `HeishaMon/rules.*`, `HeishaMon/src/rules/**`, `HeishaMon/src/common/*` (unless listed below) | CurlyMo | MPL-2.0 (see file headers) |
| `HeishaMon/src/opentherm/*` | Ihor Melnyk, https://github.com/ihormelnyk/OpenTherm | MIT |
| `HeishaMon/src/common/sha1.*` | CTrabant (Teeny SHA-1) | MIT |
| `HeishaMon/src/common/base64.*` | Apple Computer, Inc. / The Apache Group / Adam Rudd | Apple Public Source License 2.0 and Apache License 1.1 (see file header) |

`base64.cpp` requires the following acknowledgment in redistributions:

> This product includes software developed by the Apache Group for use in the Apache HTTP server project (http://www.apache.org/).

## Libraries linked into the firmware binaries

These are not part of this repository. The build (see [LIBSUSED.md](LIBSUSED.md) and `.github/workflows/main.yml`) downloads them and links them statically into the firmware binary.

| Library | License |
| ---- | ---- |
| ESP8266 Arduino core (small board) | LGPL-2.1 |
| ESP32 Arduino core and ESP-IDF (large board) | LGPL-2.1-or-later (core), Apache-2.0 (ESP-IDF) |
| PubSubClient by Nick O'Leary | MIT |
| ArduinoJson by Benoît Blanchon | MIT |
| DallasTemperature by Miles Burton | MIT |
| OneWire by Jim Studt, Paul Stoffregen et al. | MIT-style |
| Adafruit NeoPixel (large board only) | LGPL-3.0 |
| Async TCP by ESP32Async (large board only) | LGPL-3.0 |
| eModbus by Michael Harwerth, Bert Melis and contributors (large board only) | MIT |

The LGPL libraries let anyone who receives a firmware binary rebuild it against a modified version of those libraries. The complete HeishaMon source code and build instructions in this repository allow exactly that. If you distribute a modified firmware binary, also publish the matching source code.
