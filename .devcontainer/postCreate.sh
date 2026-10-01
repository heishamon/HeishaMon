#!/usr/bin/env bash
set -e

pre-commit install

# Libraries needed by the Modbus TCP server (ESP32). eModbus is not in the Arduino registry.
arduino-cli lib install "Async TCP"
ARDUINO_LIBRARY_ENABLE_UNSAFE_INSTALL=true arduino-cli lib install --git-url https://github.com/eModbus/eModbus.git#v1.7.5stable
