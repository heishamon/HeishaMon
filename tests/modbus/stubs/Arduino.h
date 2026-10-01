#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

using byte = unsigned char;
#define PROGMEM
#define FPSTR(value) (value)
#define PSTR(value) (value)
#define pgm_read_ptr(value) (*(value))
#define memcpy_P std::memcpy
#define strcmp_P std::strcmp
#define snprintf_P std::snprintf
constexpr int INPUT_PULLUP = 2;
constexpr int OUTPUT = 1;

class String {
  std::string value;
public:
  String() = default;
  String(const char *text) : value(text) {}
  String(const std::string &text) : value(text) {}
  template<class T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
  String(T number) : value(std::to_string(number)) {}
  size_t length() const { return value.length(); }
  char charAt(size_t index) const { return value.at(index); }
  String substring(size_t start) const { return value.substr(start); }
  int indexOf(char c) const {
    const auto position = value.find(c);
    return position == std::string::npos ? -1 : static_cast<int>(position);
  }
  long toInt() const { return std::stol(value); }
  float toFloat() const { return std::stof(value); }
  const char *c_str() const { return value.c_str(); }
  String &operator+=(const String &other) { value += other.value; return *this; }
};
