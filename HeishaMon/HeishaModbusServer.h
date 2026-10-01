#pragma once
#ifdef ESP32
#include <Arduino.h>
#include "ModbusServerTCPasync.h"

// Modbus TCP server. The eModbus callbacks run in the AsyncTCP task, not in loop().
// They therefore never touch shared heat pump state directly: reads are served from a
// snapshot that loop() refreshes, and writes are queued for loop() to execute.
class HeishaModbusServer {
public:
    void setup(bool isOptionalPCB, bool isS0Enabled, bool allowWrites);
    // Call from the main loop: refreshes the data snapshot and executes queued writes.
    void loop(bool isS0Enabled, bool extraDataBlockAvailable);
    static bool registerRow(uint16_t index, String &html);

private:
    // eModbus callbacks (must be static)
    static ModbusMessage FC_01(ModbusMessage request);
    static ModbusMessage FC_03(ModbusMessage request);
    static ModbusMessage FC_05(ModbusMessage request);
    static ModbusMessage FC_06(ModbusMessage request);
    static ModbusMessage FC_16(ModbusMessage request);

private:
    ModbusServerTCPasync _mbServer;
};
#endif
