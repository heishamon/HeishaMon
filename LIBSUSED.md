## libs used

**Note:**

This is just a user friendly display of commands used in build action https://github.com/heishamon/HeishaMon/blob/main/.github/workflows/main.yml

Only actual code in build action is relevant! The build installs the latest version of each library from the Arduino Library Manager.

| lib | license | download |
| ---- | ---- | ---- |
|pubsubclient by nick o'leary | MIT | https://github.com/knolleary/pubsubclient |
|arduinojson by benoit blanchon | MIT | https://github.com/bblanchon/ArduinoJson |
|dallastemperature by miles burton | MIT | https://github.com/milesburton/Arduino-Temperature-Control-Library |
|onewire | MIT-style | https://www.pjrc.com/teensy/td_libs_OneWire.html |
|Adafruit NeoPixel (large board only) | LGPL-3.0 | https://github.com/adafruit/Adafruit_NeoPixel |
|Async TCP by ESP32Async (ESP32 only, `arduino-cli lib install "Async TCP"`) | LGPL-3.0 | https://github.com/ESP32Async/AsyncTCP |
|eModbus (ESP32 only, not in the Arduino registry: `ARDUINO_LIBRARY_ENABLE_UNSAFE_INSTALL=true arduino-cli lib install --git-url https://github.com/eModbus/eModbus.git#v1.7.5stable`) | MIT | https://github.com/eModbus/eModbus |

Board cores: `esp8266:esp8266` (LGPL-2.1) and `esp32:esp32` (LGPL-2.1-or-later), see the build action for the exact versions.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for all third-party licenses.
