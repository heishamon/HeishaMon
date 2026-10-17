"""Compile the real decoder with a small host shim and test MQTT output.

Run with python3 Tools/test_decode.py (requires g++).
"""

import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SHIM = r"""
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <type_traits>
using byte = unsigned char;
class String {
  std::string value;
public:
  String() {}
  String(const char* s): value(s) {}
  template<class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
  String(T n): value(std::to_string(n)) {}
  String(float n, int digits) {
    char buf[64]; snprintf(buf, sizeof(buf), "%.*f", digits, n); value = buf;
  }
  const char* c_str() const { return value.c_str(); }
  int toInt() const { return atoi(c_str()); }
  String operator+(const char* rhs) const { return String((value + rhs).c_str()); }
  bool operator!=(const String& rhs) const { return value != rhs.value; }
};
#define PROGMEM
#define PSTR(x) x
#define _F(x) x
#define memcpy_P memcpy
#define sprintf_P sprintf
unsigned long millis() { return 1; }
unsigned int word(char high, char low) {
  return (static_cast<byte>(high) << 8) | static_cast<byte>(low);
}
struct Publication { std::string topic, value; bool retained; };
class PubSubClient {
public:
  std::vector<Publication> messages;
  bool publish(const char* topic, const char* value, bool retained) {
    messages.push_back({topic, value, retained}); return true;
  }
};
constexpr int DATASIZE = 203, OPTDATASIZE = 20;
byte optionalPCBQuery[20];
const char* mqtt_topic_values = "main";
const char* mqtt_topic_xvalues = "extra";
const char* mqtt_topic_pcbvalues = "optional";
void websocket_write_all(char*, uint16_t) {}
void rules_event_cb(const char*, const char*) {}
void log_message(char*) {}
"""

# Captured/documented encodings, including independent fields sharing bytes.
CASES = [
    (144, 5, 0x56, "1"),
    (145, 9, 0x25, "1"),
    (146, 21, 0x26, "38"),
    (147, 25, 0x5E, "9"),
    (148, 25, 0x5E, "1"),
    (149, 27, 0x09, "1"),
    (150, 27, 0x06, "1"),
    (151, 30, 0x06, "1"),
    (152, 46, 0x99, "25"),
    (153, 58, 0x85, "5"),
    (154, 71, 0x79, "120"),
    (155, 72, 0x79, "120"),
    (156, 73, 0x8D, "140"),
    (157, 74, 0x8D, "140"),
    (158, 97, 0x05, "120"),
    (159, 98, 0x65, "100"),
    (160, 110, 0x95, "1"),
    (161, 110, 0x65, "1"),
    (162, 110, 0x59, "1"),
    (163, 110, 0x56, "1"),
    (164, 112, 0x95, "1"),
    (165, 117, 0x06, "1"),
    (166, 117, 0x05, "1"),
    (167, 120, 0x29, "1"),
    (168, 120, 0x19, "1"),
    (169, 152, 0x7B, "-5"),
]


def main():
    header = (ROOT / "HeishaMon/decode.h").read_text()
    source = (ROOT / "HeishaMon/decode.cpp").read_text()
    docs = (ROOT / "MQTT-Topics.md").read_text()
    count = int(re.search(r"#define NUMBER_OF_TOPICS (\d+)", header)[1])
    tables = {}
    for name in ("topics", "topicBytes", "topicFunctions", "topicDescription"):
        body = re.search(r"\b" + name + r"\[\].*?=\s*\{(.*?)\};", header, re.S)[1]
        tables[name] = [
            line.split("//")[0].strip().rstrip(",")
            for line in body.splitlines()
            if line.split("//")[0].strip()
        ]
        assert len(tables[name]) == count, name
    assert len(set(tables["topics"])) == count
    assert all(len(name.strip('"')) < 42 for name in tables["topics"])
    checks = []
    for topic, address, raw, expected in CASES:
        name = tables["topics"][topic].strip('"')
        assert f"TOP{topic} | main/{name} |" in docs
        checks.append(
            f"""
  memset(data, 0x55, sizeof(data));
  data[{address}] = {raw};
  assert(std::string(getDataValue(data, {topic}).c_str()) == "{expected}");
  resetlastalldatatime(); client.messages.clear();
  decode_heatpump_data(data, previous, client, log_message, base, 60);
  assert(client.messages.size() == NUMBER_OF_TOPICS);
  assert(client.messages[{topic}].topic == "test/main/{name}");
  assert(client.messages[{topic}].value == "{expected}");
  assert(client.messages[{topic}].retained);
  client.messages.clear();
  decode_heatpump_data(data, previous, client, log_message, base, 60);
  assert(client.messages.empty());
"""
        )
    checks.append(
        """
  data[152] = 0x80;
  client.messages.clear();
  decode_heatpump_data(data, previous, client, log_message, base, 60);
  assert(client.messages.size() == 1);
  assert(client.messages[0].topic == "test/main/Heat_Request_Temp_With_Buffer");
  assert(client.messages[0].value == "0");
  assert(getHeaterCapacity(0).toInt() == -1);
  assert(getHeaterCapacity(4).toInt() == 3);
  assert(getHeaterCapacity(8).toInt() == 6);
  assert(getIntMinus1Times30(0).toInt() == -30);
  assert(getIntMinus1Times30(255).toInt() == 7620);
  for (int n = 0; n < 4; ++n) {
    assert(getZone1Active(n).toInt() == (n & 1));
    assert(getZone2Active(n).toInt() == ((n >> 1) & 1));
  }
  assert(getBit1and2(0).toInt() == -1);
  assert(getBit3and4(0).toInt() == -1);
  assert(getBit5and6(0).toInt() == -1);
  assert(getBit7and8(0).toInt() == -1);
"""
    )
    header = re.sub(r"^#include.*$", "", header, flags=re.M)
    source = re.sub(r"^#include.*$", "", source, flags=re.M)
    program = (
        SHIM
        + header
        + source
        + """
int main() {
  char data[DATASIZE], previous[DATASIZE] = {}, base[] = "test";
  PubSubClient client;
"""
        + "".join(checks)
        + "\n}\n"
    )
    with tempfile.TemporaryDirectory() as directory:
        cpp = Path(directory) / "test.cpp"
        binary = Path(directory) / "test"
        cpp.write_text(program)
        subprocess.run(
            [
                os.environ.get("CXX", "g++"),
                "-std=c++17",
                "-funsigned-char",
                "-Wno-narrowing",
                str(cpp),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    print(
        f"PASS: {len(CASES)} decoder/MQTT cases, tables, documentation and boundaries"
    )


if __name__ == "__main__":
    main()
