#include <cassert>
#include <limits>
#include <set>
#include "../../HeishaMon/HeishaModbusServer.cpp"
#include "generated_stubs.h"
#include "../../HeishaMon/s0data.cpp"

char actData[DATASIZE] = {};
char actDataExtra[DATASIZE] = {};
char actOptData[OPTDATASIZE] = {};
String readings[3][1000];
volatile s0DataStruct actS0Data[NUM_S0_COUNTERS];
volatile s0SettingsStruct actS0Settings[NUM_S0_COUNTERS];
std::string lastCommand, lastPayload;
bool relays[2] = {};
String getDataValue(char *, unsigned int index) { return readings[0][index]; }
String getDataValueExtra(char *, unsigned int index) { return readings[1][index]; }
String getOptDataValue(char *, unsigned int index) { return readings[2][index]; }
bool send_command(byte *, int) { return true; }
void log_message(char *) {}
void setRelay1(bool state) { relays[0] = state; }
void setRelay2(bool state) { relays[1] = state; }
bool getRelay1() { return relays[0]; }
bool getRelay2() { return relays[1]; }
void send_heatpump_command(char *topic, char *payload, bool (*)(byte *, int), void (*)(char *), bool) {
  lastCommand = topic; lastPayload = payload;
}

ModbusMessage call(uint8_t fc, uint16_t address, uint16_t value) {
  ModbusMessage request;
  request.add(uint8_t(1), fc, address, value);
  return ModbusServerTCPasync::workers.at(fc)(request);
}

uint16_t read(uint16_t address) {
  auto response = call(3, address, 1);
  assert(response.bytes.size() == 5 && response.bytes[1] == 3);
  uint16_t value;
  response.get(3, value);
  return value;
}

float readFloat(uint16_t address) {
  const auto response = call(3, address, 2);
  assert(response.bytes.size() == 7 && response.bytes[1] == 3);
  uint32_t bits;
  response.get(3, bits);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void expectError(uint8_t fc, uint16_t address, uint16_t value, uint8_t error) {
  auto response = call(fc, address, value);
  assert(response.bytes == std::vector<uint8_t>({1, uint8_t(fc | 128), error}));
}

// FC16 request: start address plus the given data words.
ModbusMessage call16(uint16_t start, const std::vector<uint16_t> &words) {
  ModbusMessage request;
  request.add(uint8_t(1), uint8_t(16), start, uint16_t(words.size()), uint8_t(words.size() * 2));
  for (uint16_t word : words) request.add(word);
  return ModbusServerTCPasync::workers.at(16)(request);
}

std::vector<uint16_t> floatWords(float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return {uint16_t(bits >> 16), uint16_t(bits & 0xFFFF)};
}

void expectError16(uint16_t start, const std::vector<uint16_t> &words, uint8_t error) {
  assert(call16(start, words).bytes == std::vector<uint8_t>({1, 16 | 128, error}));
}

int main() {
  HeishaModbusServer server;
  server.setup(true, false, true);
  server.loop(false, true);
  for (auto &group : readings) for (auto &value : group) value = "1";
  assert(read(32000) == 3);

  // Every actual topic has exactly one integer and one two-word float mapping.
  std::set<uint16_t> addresses;
  for (const auto &range : kTopicRanges) {
    for (uint16_t i = 0; i < range.count; ++i) {
      auto integer = uint16_t(range.baseAddress + i);
      auto floating = ModbusMap::floatAddress(integer);
      assert(addresses.insert(integer).second);
      assert(addresses.insert(floating).second && addresses.insert(floating + 1).second);
      TopicSource source;
      uint16_t decoded;
      bool high;
      assert(decodeTopicAddress(integer, source, decoded) && source == range.source && decoded == i);
      assert(decodeFloatTopicAddress(floating, source, decoded, high) && high && source == range.source && decoded == i);
      assert(decodeFloatTopicAddress(floating + 1, source, decoded, high) && !high && decoded == i);
      read(integer);
      auto pair = call(3, floating, 2);
      assert(pair.bytes == std::vector<uint8_t>({1, 3, 4, 0x3f, 0x80, 0, 0})); // float 1.0
    }
    expectError(3, range.baseAddress + range.count, 1, ILLEGAL_DATA_ADDRESS);
    expectError(3, ModbusMap::floatAddress(range.baseAddress + range.count), 1, ILLEGAL_DATA_ADDRESS);
  }
  // Growing any group to its full capacity never moves/overlaps another block.
  addresses.clear();
  for (uint16_t base : {0, 1000, 2000, 3000}) {
    for (uint16_t i = 0; i < 1000; ++i) {
      auto start = ModbusMap::floatAddress(base + i);
      assert(addresses.insert(base + i).second);
      assert(addresses.insert(start).second && addresses.insert(start + 1).second);
      uint16_t index; bool high;
      assert(ModbusMap::decodeRange(start + 1, ModbusMap::floatAddress(base), 1000, 2, index, high));
      assert(index == i && !high);
    }
  }
  assert(ModbusMap::floatAddress(139) == 10278);
  assert(ModbusMap::floatAddress(1000) == 12000);
  assert(ModbusMap::floatAddress(2000) == 14000);

  // Source-aware fixed scaling: extra/optional index 1 must not inherit Pump_Flow x100.
  readings[0][14] = "-5.25";
  readings[0][67] = "2.2";
  readings[1][1] = "800";
  readings[2][1] = "2";
  assert(int16_t(read(14)) == -525 && read(67) == 220);
  assert(read(1001) == 800 && read(2001) == 2);
  readings[0][44] = "H74";
  assert(read(44) == 8074);
  readings[0][14] = "999";
  assert(read(14) == 32767);

  // S0 values use independent fixed port blocks; disabled/uninitialized inputs are zero.
  for (uint16_t base : {3000, 3100}) {
    for (uint16_t field = 0; field < 6; ++field) {
      assert(read(base + field) == 0);
      assert(readFloat(ModbusMap::floatAddress(base + field)) == 0);
    }
  }
  server.loop(true, true);
  assert(read(3005) == 0); // initialization has not assigned a GPIO yet
  actS0Settings[0].gpiopin = 1;
  actS0Settings[0].ppkwh = 2000;
  actS0Data[0].watt = 2500;
  actS0Data[0].pulsesTotal = 10001;
  actS0Data[0].pulses = 17;
  actS0Data[0].lastReportWatthour = 0.5f;
  actS0Data[0].goodPulses = 99;
  actS0Data[0].badPulses = 25;
  actS0Data[0].avgPulseWidth = 37;
  actS0Settings[1].gpiopin = 2;
  actS0Settings[1].ppkwh = 1000;
  actS0Data[1].watt = 42000;
  actS0Data[1].pulsesTotal = 4000000000U;
  actS0Data[1].lastReportWatthour = 12.25f;
  actS0Data[1].avgPulseWidth = 50;
  const float expectedS0[2][6] = {{2500, 5000.5f, 0.5f, 80, 37, 1},
                                 {42000, 4000000000.0f, 12.25f, 100, 50, 1}};
  for (uint16_t port = 0; port < 2; ++port) {
    for (uint16_t field = 0; field < 6; ++field) {
      const uint16_t integer = 3000 + port * 100 + field;
      assert(readFloat(ModbusMap::floatAddress(integer)) == expectedS0[port][field]);
      const float value = expectedS0[port][field];
      assert(read(integer) == (value >= 32767 ? 32767 : uint16_t(value)));
      expectError(6, integer, 0, ILLEGAL_DATA_ADDRESS); // read-only
    }
    expectError(3, 3006 + port * 100, 1, ILLEGAL_DATA_ADDRESS);
    expectError(3, 16012 + port * 200, 1, ILLEGAL_DATA_ADDRESS);
    auto block = call(3, 16000 + port * 200, 12);
    assert(block.bytes.size() == 27); // all six floats in one request
  }
  assert(actS0Data[0].pulses == 17 && actS0Data[0].pulsesTotal == 10001);
  assert(actS0Data[0].lastReportWatthour == 0.5f); // reading never resets an interval
  S0Reading snapshot[NUM_S0_COUNTERS];
  readS0Readings(true, snapshot);
  actS0Data[0].watt = 1234;
  uint16_t high, low;
  assert(s0ToRegisterValue(16000, snapshot, high) && s0ToRegisterValue(16001, snapshot, low));
  uint32_t bits = (uint32_t(high) << 16) | low;
  float original;
  std::memcpy(&original, &bits, sizeof(original));
  assert(original == 2500); // both words use the captured sample
  actS0Settings[1].ppkwh = 0;
  assert(read(3105) == 0 && readFloat(16202) == 0); // no divide by zero
  server.loop(false, true);
  assert(readFloat(16000) == 0 && read(3005) == 0);

  // Commands dispatch by stable IDs and signed payload, never by table position.
  // Writes are only queued by the Modbus callbacks; loop() executes them.
  addresses.clear();
  for (const auto &command : commands) {
    bool mapped = false;
    for (const auto &entry : ModbusMap::MAIN_COMMANDS) mapped |= std::strcmp(command.name, entry.name) == 0;
    assert(mapped); // every command has a permanent Modbus ID
  }
  for (const auto &command : ModbusMap::MAIN_COMMANDS) {
    assert(command.id > 0 && command.id <= 1000);
    const auto address = ModbusMap::commandAddress(command.id);
    assert(addresses.insert(address).second);
    bool exists = false;
    for (const auto &upstream : commands) exists |= std::strcmp(command.name, upstream.name) == 0;
    assert(exists);
    if (std::strcmp(command.name, "SetCurves") == 0) {
      expectError(6, address, 0, ILLEGAL_DATA_VALUE);
    } else {
      lastCommand.clear();
      // Temperature commands are x100 like the readings and only take whole degrees.
      call(6, address, command.scale100 ? uint16_t(-500) : uint16_t(-5));
      assert(lastCommand.empty()); // not executed in the Modbus callback
      server.loop(false, true);
      assert(lastCommand == command.name && lastPayload == "-5");
      if (command.scale100) {
        expectError(6, address, 150, ILLEGAL_DATA_VALUE);
        call(6, address, 4500);
        server.loop(false, true);
        assert(lastPayload == "45");
      }
    }
  }
  assert(ModbusMap::commandAddress(1) == 5000);
  assert(ModbusMap::commandAddress(100) == 7000);
  for (const auto &command : ModbusMap::OPTIONAL_COMMANDS) {
    assert(command.id < 1000);
    auto address = uint16_t(6000 + command.id);
    assert(addresses.insert(address).second);
    bool exists = false;
    for (const auto &upstream : optionalCommands) exists |= std::strcmp(command.name, upstream.name) == 0;
    assert(exists);
    call(6, address, 7);
    server.loop(false, true);
    // Temperatures are written like they are read: x100.
    assert(lastCommand == command.name && lastPayload == (command.scale100 ? "0.07" : "7"));
    call(6, address, uint16_t(-525));
    server.loop(false, true);
    assert(lastPayload == (command.scale100 ? "-5.25" : "-525"));
    call(6, address, 2150);
    server.loop(false, true);
    assert(lastPayload == (command.scale100 ? "21.50" : "2150"));
  }
  // float32 writes (FC16, MSW first) mirror the int16 command block and are never scaled.
  assert(ModbusMap::floatCommandAddress(5000) == 20000);
  assert(ModbusMap::floatCommandAddress(5010) == 20020); // SetDHWTemp
  assert(ModbusMap::floatCommandAddress(6006) == 22012); // SetPoolTemp
  assert(ModbusMap::floatCommandAddress(7000) == 24000); // SetReset
  for (const auto &command : ModbusMap::MAIN_COMMANDS) {
    const uint16_t floatRegister = ModbusMap::floatCommandAddress(ModbusMap::commandAddress(command.id));
    if (std::strcmp(command.name, "SetCurves") == 0) {
      expectError16(floatRegister, floatWords(1), ILLEGAL_DATA_VALUE);
      continue;
    }
    lastCommand.clear();
    auto response = call16(floatRegister, floatWords(7));
    assert(response.bytes == std::vector<uint8_t>({1, 16, uint8_t(floatRegister >> 8), uint8_t(floatRegister), 0, 2}));
    assert(lastCommand.empty()); // queued only
    server.loop(false, true);
    assert(lastCommand == command.name && lastPayload == "7");
    if (command.scale100) { // whole degrees only, no truncation
      expectError16(floatRegister, floatWords(7.5f), ILLEGAL_DATA_VALUE);
    }
  }
  for (const auto &command : ModbusMap::OPTIONAL_COMMANDS) {
    const uint16_t floatRegister = ModbusMap::floatCommandAddress(6000 + command.id);
    call16(floatRegister, floatWords(7));
    server.loop(false, true);
    assert(lastCommand == command.name && lastPayload == (command.scale100 ? "7.00" : "7"));
    if (command.scale100) {
      call16(floatRegister, floatWords(21.5f));
      server.loop(false, true);
      assert(lastPayload == "21.50");
    } else {
      expectError16(floatRegister, floatWords(0.5f), ILLEGAL_DATA_VALUE);
    }
  }
  call16(20020, floatWords(-5)); // SetDHWTemp
  server.loop(false, true);
  assert(lastCommand == "SetDHWTemp" && lastPayload == "-5");
  expectError16(20020, floatWords(std::numeric_limits<float>::quiet_NaN()), ILLEGAL_DATA_VALUE);
  expectError16(20020, floatWords(std::numeric_limits<float>::infinity()), ILLEGAL_DATA_VALUE);
  expectError16(20020, floatWords(1e9f), ILLEGAL_DATA_VALUE);
  expectError16(20021, floatWords(45), ILLEGAL_DATA_ADDRESS);           // low word is not a start address
  expectError16(20020, {0x4234}, ILLEGAL_DATA_VALUE);                    // one register is not a float
  expectError16(20020, {0, 0, 0, 0}, ILLEGAL_DATA_VALUE);               // one command per request
  expectError16(20000 + 2 * 999, floatWords(1), ILLEGAL_DATA_ADDRESS);   // no such command
  expectError16(20000 + 2 * 3000, floatWords(1), ILLEGAL_DATA_ADDRESS);
  expectError16(65534, floatWords(1), ILLEGAL_DATA_ADDRESS);
  expectError16(3000, {1}, ILLEGAL_DATA_ADDRESS);                        // read-only registers
  expectError16(5000, {}, ILLEGAL_DATA_VALUE);
  // FC16 with a single int16 register behaves like FC06, including x100 temperatures.
  call16(5004, {uint16_t(-500)}); // SetZ1HeatRequestTemperature
  server.loop(false, true);
  assert(lastCommand == "SetZ1HeatRequestTemperature" && lastPayload == "-5");
  expectError16(5004, {150}, ILLEGAL_DATA_VALUE);
  expectError16(5004, {1, 2}, ILLEGAL_DATA_VALUE);
  expectError(6, 20020, 45, ILLEGAL_DATA_ADDRESS); // FC06 cannot write floats
  expectError(6, 1001, 1, ILLEGAL_DATA_ADDRESS);
  expectError(6, 2000, 1, ILLEGAL_DATA_ADDRESS);
  expectError(6, 5999, 1, ILLEGAL_DATA_ADDRESS);
  expectError(6, 6100, 1, ILLEGAL_DATA_ADDRESS);
  expectError(3, 65535, 2, ILLEGAL_DATA_ADDRESS);
  expectError(3, 0, 0, ILLEGAL_DATA_VALUE);
  expectError(3, 0, 126, ILLEGAL_DATA_VALUE);
  call(5, 30000, 0xFF00);
  assert(!relays[0]); // queued, not switched in the callback
  server.loop(false, true);
  assert(relays[0] && !relays[1]);
  call(5, 30001, 0xFF00);
  call(5, 30000, 0);
  server.loop(false, true);
  assert(!relays[0] && relays[1]);
  expectError(5, 30002, 0, ILLEGAL_DATA_ADDRESS);
  expectError(5, 30001, 1, ILLEGAL_DATA_VALUE);

  // Relay state can be read back: FC01 (coils 30000/30001, same as FC05) and registers 32010/32011.
  assert(call(1, 30000, 2).bytes == std::vector<uint8_t>({1, 1, 1, 2}));  // relay 2 on, relay 1 off
  assert(call(1, 30001, 1).bytes == std::vector<uint8_t>({1, 1, 1, 1}));
  assert(read(32010) == 0 && read(32011) == 1);
  call(5, 30000, 0xFF00);
  server.loop(false, true);
  assert(call(1, 30000, 2).bytes == std::vector<uint8_t>({1, 1, 1, 3}));
  assert(read(32010) == 1 && read(32011) == 1);
  relays[1] = false;  // e.g. switched over MQTT: the next loop() picks it up
  server.loop(false, true);
  assert(call(1, 30000, 2).bytes == std::vector<uint8_t>({1, 1, 1, 1}));
  expectError(1, 30002, 1, ILLEGAL_DATA_ADDRESS);
  expectError(1, 30001, 2, ILLEGAL_DATA_ADDRESS);
  expectError(1, 30000, 0, ILLEGAL_DATA_VALUE);
  expectError(1, 30000, 2001, ILLEGAL_DATA_VALUE);
  expectError(3, 32012, 1, ILLEGAL_DATA_ADDRESS);
  call(5, 30000, 0);
  server.loop(false, true);

  // A full write queue answers with a busy exception instead of dropping requests silently.
  for (int i = 0; i < 16; ++i) call(6, 5000, 1);
  expectError(6, 5000, 1, SERVER_DEVICE_BUSY);
  expectError(5, 30000, 0xFF00, SERVER_DEVICE_BUSY);
  server.loop(false, true);
  call(6, 5000, 1);
  server.loop(false, true);

  // Extra data block missing: extra registers raise exceptions instead of returning zeros.
  server.loop(false, false);
  expectError(3, 1001, 1, ILLEGAL_DATA_ADDRESS);
  expectError(3, 12002, 2, ILLEGAL_DATA_ADDRESS);
  assert(read(14) != 0 && read(2001) == 2); // main and optional registers are unaffected
  server.loop(false, true);
  assert(read(1001) == 800);

  // Optional PCB disabled: no optional registers, and optional commands are rejected.
  server.setup(false, false, true);
  server.loop(false, true);
  expectError(3, 2001, 1, ILLEGAL_DATA_ADDRESS);
  expectError(3, 14002, 2, ILLEGAL_DATA_ADDRESS);
  expectError(6, 6006, 2150, ILLEGAL_DATA_ADDRESS);
  expectError16(22012, floatWords(21.5f), ILLEGAL_DATA_ADDRESS); // optional PCB disabled
  lastCommand.clear();
  call(6, 5000, 1); // main commands still work
  server.loop(false, true);
  assert(lastCommand == "SetHeatpump");

  // Writes are refused unless explicitly allowed; reads keep working.
  server.setup(true, false, false);
  server.loop(false, true);
  lastCommand.clear();
  relays[0] = false;
  expectError(6, 5000, 1, ILLEGAL_FUNCTION);
  expectError(6, 7000, 1, ILLEGAL_FUNCTION);
  expectError(5, 30000, 0xFF00, ILLEGAL_FUNCTION);
  expectError16(20000, floatWords(1), ILLEGAL_FUNCTION);
  expectError16(5000, {1}, ILLEGAL_FUNCTION);
  server.loop(false, true);
  assert(lastCommand.empty() && !relays[0]);
  assert(read(32000) == 3);
  server.setup(true, false, true);
  server.loop(false, true);

  // The web table must enumerate every topic, command, coil and version entry once.
  unsigned count = 0;
  String row;
  while (HeishaModbusServer::registerRow(count, row)) {
    assert(std::string(row.c_str()).find("<tr ") == 0);
    ++count;
    assert(count < 1000);
  }
  assert(count == NUMBER_OF_TOPICS + NUMBER_OF_TOPICS_EXTRA + NUMBER_OF_OPT_TOPICS +
                  arraySize(MAIN_COMMANDS) + arraySize(OPTIONAL_COMMANDS) + 5 + NUM_S0_COUNTERS * S0_FIELD_COUNT);
  std::puts("PASS: complete map, expansion, scaling, S0 values, command dispatch, coils, boundaries and register page");
}
