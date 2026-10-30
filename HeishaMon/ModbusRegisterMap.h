#pragma once

#include <stdint.h>

// Map v3 (v2 addresses, temperature writes x100): never derive block bases from the number of currently known topics.
// Topic numbers and command IDs are permanent; append new IDs, never reuse them.
//
// Layout, low to high (every block has reserved room to grow):
//   int16 measurements (FC03)                 0 -  4999  main, extra, optional PCB, S0
//   int16 commands (FC06/FC16)             5000 -  9999  heat pump, optional PCB, system
//   float32 measurements (FC03)           10000 - 19999  2 registers per int16 address
//   float32 commands (FC16)               20000 - 29999  2 registers per int16 command
//   coils (FC01/FC05, own address space)   30000 - 31999  relays
//   device information (FC03)             32000 - 32999  version, relay state
namespace ModbusMap {
constexpr uint16_t VERSION = 3;
constexpr uint16_t TOPIC_CAPACITY = 1000;
constexpr uint16_t MAIN_TOPIC_BASE = 0;
constexpr uint16_t EXTRA_TOPIC_BASE = 1000;
constexpr uint16_t OPTIONAL_TOPIC_BASE = 2000;
constexpr uint16_t S0_TOPIC_BASE = 3000;
constexpr uint16_t S0_PORT_STRIDE = 100;
constexpr uint16_t S0_FIELD_COUNT = 6;
constexpr uint16_t VERSION_REGISTER = 32000;
constexpr uint16_t RELAY_STATE_REGISTER = 32010;  // 32010 = relay 1, 32011 = relay 2 (0/1)
constexpr uint16_t FLOAT_BASE = 10000;
constexpr uint16_t COMMAND_BASE = 5000;
constexpr uint16_t OPTIONAL_COMMAND_BASE = 6000;
constexpr uint16_t SYSTEM_COMMAND_BASE = 7000;
constexpr uint16_t FLOAT_COMMAND_BASE = 20000;
constexpr uint16_t RESET_COMMAND_ID = 100;
constexpr uint16_t COIL_BASE = 30000;  // relay 1 = coil 30000, relay 2 = coil 30001
constexpr uint16_t RELAY_COUNT = 2;

constexpr uint16_t floatAddress(uint16_t integerAddress) {
  return FLOAT_BASE + 2 * integerAddress;
}

constexpr uint16_t commandAddress(uint16_t id) {
  return id == RESET_COMMAND_ID ? SYSTEM_COMMAND_BASE : COMMAND_BASE + id - 1;
}

// float32 write address of a command: two registers (MSW first) per command, laid out like the
// int16 command block. Example: SetDHWTemp int16 5010, float32 20020 / 20021.
constexpr uint16_t floatCommandAddress(uint16_t commandRegister) {
  return FLOAT_COMMAND_BASE + 2 * (commandRegister - COMMAND_BASE);
}

// Only populated entries are valid. Reserved space must not alias another block.
inline bool decodeRange(uint16_t address, uint16_t base, uint16_t count,
                        uint16_t stride, uint16_t &index, bool &highWord) {
  if ((stride != 1 && stride != 2) || count > TOPIC_CAPACITY || address < base ||
      uint32_t(address) >= uint32_t(base) + uint32_t(count) * stride) {
    return false;
  }
  const uint16_t offset = address - base;
  index = offset / stride;
  highWord = (offset % stride) == 0;
  return true;
}

// Modbus IDs are kept here, separate from the command tables in commands.h, so the
// command parsing code does not need to know about Modbus. Commands are matched by
// name, which keeps addresses stable even if the command table is reordered.
struct MainCommand {
  uint16_t id;
  const char *name;
  // Temperatures and deltas are written like they are read: as an int16 with two implied
  // decimals (2150 = 21.50). The heat pump commands only take whole degrees, so the value
  // must be a multiple of 100 (2100 = 21).
  bool scale100;
};

constexpr MainCommand MAIN_COMMANDS[] = {
  {1, "SetHeatpump", false},
  {2, "SetHolidayMode", false},
  {3, "SetQuietMode", false},
  {4, "SetPowerfulMode", false},
  {5, "SetZ1HeatRequestTemperature", true},
  {6, "SetZ1CoolRequestTemperature", true},
  {7, "SetZ2HeatRequestTemperature", true},
  {8, "SetZ2CoolRequestTemperature", true},
  {9, "SetOperationMode", false},
  {10, "SetForceDHW", false},
  {11, "SetDHWTemp", true},
  {12, "SetForceDefrost", false},
  {13, "SetForceSterilization", false},
  {14, "SetPump", false},
  {15, "SetMaxPumpDuty", false},
  {16, "SetCurves", false},
  {17, "SetZones", false},
  {18, "SetFloorHeatDelta", true},
  {19, "SetFloorCoolDelta", true},
  {20, "SetDHWHeatDelta", true},
  {21, "SetHeaterDelayTime", false},
  {22, "SetHeaterStartDelta", true},
  {23, "SetHeaterStopDelta", true},
  {24, "SetMainSchedule", false},
  {25, "SetAltExternalSensor", false},
  {26, "SetExternalPadHeater", false},
  {27, "SetBufferDelta", true},
  {28, "SetBuffer", false},
  {29, "SetHeatingOffOutdoorTemp", true},
  {30, "SetExternalControl", false},
  {31, "SetExternalError", false},
  {32, "SetExternalCompressorControl", false},
  {33, "SetExternalHeatCoolControl", false},
  {34, "SetBivalentControl", false},
  {35, "SetBivalentMode", false},
  {36, "SetBivalentStartTemp", true},
  {37, "SetBivalentAPStartTemp", true},
  {38, "SetBivalentAPStopTemp", true},
  {39, "SetForceHeater", false},
  {40, "SetHeatingControl", false},
  {41, "SetSmartDHW", false},
  {42, "SetQuietModePriority", false},
  {43, "SetPumpFlowrateMode", false},
  {44, "SetDHWSensorSelection", false},
  {45, "SetDHWHeaterState", false},
  {46, "SetRoomHeaterState", false},
  {47, "SetHeaterOnOutdoorTemp", true},
  {48, "SetSterilizationTemp", true},
  {49, "SetSterilizationMaxTime", false},
  {RESET_COMMAND_ID, "SetReset", false},
};

struct OptionalCommand {
  uint16_t id;
  const char *name;
  // Same x100 convention as MainCommand; the optional PCB also accepts decimals (2150 = 21.50).
  bool scale100;
};

constexpr OptionalCommand OPTIONAL_COMMANDS[] = {
  {0, "SetHeatCoolMode", false},
  {1, "SetCompressorState", false},
  {2, "SetSmartGridMode", false},
  {3, "SetExternalThermostat1State", false},
  {4, "SetExternalThermostat2State", false},
  {5, "SetDemandControl", false},
  {6, "SetPoolTemp", true},
  {7, "SetBufferTemp", true},
  {8, "SetZ1RoomTemp", true},
  {9, "SetZ1WaterTemp", true},
  {10, "SetZ2RoomTemp", true},
  {11, "SetZ2WaterTemp", true},
  {12, "SetSolarTemp", true},
  {13, "SetOptPCBByte9", false},
};
}  // namespace ModbusMap
