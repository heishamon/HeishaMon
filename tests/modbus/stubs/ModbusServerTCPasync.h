#pragma once
#include "Arduino.h"
#include <map>
#include <vector>

constexpr uint8_t ILLEGAL_FUNCTION = 1;
constexpr uint8_t ILLEGAL_DATA_ADDRESS = 2;
constexpr uint8_t ILLEGAL_DATA_VALUE = 3;
constexpr uint8_t SERVER_DEVICE_BUSY = 6;
constexpr uint8_t READ_COIL = 1;
constexpr uint8_t READ_HOLD_REGISTER = 3;
constexpr uint8_t WRITE_COIL = 5;
constexpr uint8_t WRITE_HOLD_REGISTER = 6;
constexpr uint8_t WRITE_MULT_REGISTERS = 16;

class ModbusMessage {
public:
  std::vector<uint8_t> bytes;
  template<class T> void add(T value) {
    for (int i = sizeof(T) - 1; i >= 0; --i) bytes.push_back((value >> (8 * i)) & 255);
  }
  template<class T, class... Rest> void add(T value, Rest... rest) {
    add(value); add(rest...);
  }
  uint16_t get(uint16_t position) const { return position; }
  template<class T, class... Rest> uint16_t get(uint16_t position, T &value, Rest &...rest) const {
    value = 0;
    for (unsigned i = 0; i < sizeof(T); ++i) value = (value << 8) | bytes.at(position++);
    return get(position, rest...);
  }
  uint8_t getServerID() const { return bytes.at(0); }
  uint8_t getFunctionCode() const { return bytes.at(1); }
  size_t size() const { return bytes.size(); }
  void clear() { bytes.clear(); }
  void setError(uint8_t unit, uint8_t function, uint8_t error) {
    bytes = {unit, uint8_t(function | 0x80), error};
  }
};

inline const ModbusMessage ECHO_RESPONSE;
class ModbusServerTCPasync {
public:
  using Handler = ModbusMessage (*)(ModbusMessage);
  inline static std::map<uint8_t, Handler> workers;
  void registerWorker(uint8_t, uint8_t function, Handler handler) { workers[function] = handler; }
  void start(int, int, int) {}
};
