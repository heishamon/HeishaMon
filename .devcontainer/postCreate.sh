#!/usr/bin/env bash
set -e

pre-commit install

# Libraries needed by the Modbus TCP server (ESP32). eModbus is not in the Arduino registry (commit of tag v1.7.5stable).
arduino-cli lib install "Async TCP"
ARDUINO_LIBRARY_ENABLE_UNSAFE_INSTALL=true arduino-cli lib install --git-url https://github.com/eModbus/eModbus.git#8cdbcc9c28efad2ad7fbb140af8fa6c6e69bf0fa
