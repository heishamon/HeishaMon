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
#define pgm_read_byte(p) (*(const unsigned char *)(p))
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
    (170, 31, 0x6A, "1"),
    (170, 31, 0x5A, "0"),
    (171, 108, 0x8F, "15"),
    (172, 109, 0x03, "60"),
    (173, 60, 0x5B, "90"),
    (174, 33, 0x76, "25"),
    (175, 33, 0x76, "30"),
    (175, 33, 0x85, "35"),
    (176, 34, 0xA7, "30"),
    (176, 34, 0x64, "15"),
    (177, 35, 0xA6, "1.0"),
    (177, 35, 0x46, "-2.0"),
    (178, 35, 0xA6, "-1.0"),
    (178, 35, 0x89, "0.5"),
    (179, 34, 0xA7, "1"),
    (179, 34, 0x67, "0"),
    (180, 27, 0x55, "0"),
    (180, 27, 0x65, "1"),
    (180, 27, 0x75, "2"),
    (180, 27, 0x95, "0"),
    (181, 27, 0x55, "0"),
    (181, 27, 0x95, "1"),
    (181, 27, 0x65, "0"),
    (182, 31, 0x55, "0"),
    (182, 31, 0x5A, "1"),
    (182, 31, 0x5B, "2"),
    (182, 31, 0x57, "2"),
    (183, 31, 0x5A, "1"),
    (183, 31, 0x5B, "1"),
    (183, 31, 0x57, "0"),
    (183, 31, 0x55, "0"),
    (184, 51, 0x2B, "4.2"),
    (184, 51, 0x06, "0.5"),
    (185, 52, 0x65, "10.0"),
    (186, 53, 0x29, "4.0"),
    (187, 54, 0x2A, "4.1"),
    (188, 55, 0x2B, "4.2"),
    (189, 56, 0x29, "4.0"),
    (190, 57, 0x2C, "4.3"),
    (191, 102, 0x7D, "-3"),
    (191, 102, 0x80, "0"),
    (192, 103, 0x7A, "-6"),
    (192, 103, 0x71, "-15"),
    (193, 36, 0x48, "-2.0"),
    (193, 36, 0xA8, "1.0"),
    (194, 28, 0x95, "1"),
    (194, 28, 0x55, "0"),
    (195, 36, 0x89, "0.5"),
    (195, 36, 0x48, "0.0"),
    (195, 36, 0x82, "-3.0"),
    (196, 37, 0x89, "0.5"),
    (196, 37, 0x88, "0.0"),
    (196, 37, 0x6E, "3.0"),
    (197, 191, 0x08, "7"),
    (197, 191, 0x0D, "12"),
    (198, 192, 0x01, "0"),
    (198, 192, 0x02, "1"),
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
  assert(std::string(getIntMinus1Div10(0).c_str()) == "-0.1");
  assert(std::string(getIntMinus1Div10(101).c_str()) == "10.0");
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
    groups = re.search(r"topicGroup\[\]\s*PROGMEM\s*=\s*\{(.*?)\};", header, re.S)[1]
    groups = re.findall(r"\bTG_[A-Z0-9_]+", groups)
    assert len(groups) == count
    basic = groups.count("TG_BASIC")
    installer = groups.count("TG_INSTALLER")
    checks.append(
        f"""
  // topic groups: the default leaves out the installer settings, basic can't be switched off
  setTopicGroups(TOPIC_GROUPS_DEFAULT);
  assert(topicEnabled(0) && !topicEnabled(170) && !topicEnabled(NUMBER_OF_TOPICS));
  resetlastalldatatime(); client.messages.clear();
  decode_heatpump_data(data, previous, client, log_message, base, 60);
  assert(client.messages.size() == NUMBER_OF_TOPICS - {installer});
  setTopicGroups(0);
  resetlastalldatatime(); client.messages.clear();
  decode_heatpump_data(data, previous, client, log_message, base, 60);
  assert(client.messages.size() == {basic});
  setTopicGroups(TOPIC_GROUPS_DEFAULT); // enabling a group publishes it at the next read
  client.messages.clear();
  decode_heatpump_data(data, previous, client, log_message, base, 60);
  assert(client.messages.size() == NUMBER_OF_TOPICS - {installer});
"""
    )
    header = re.sub(r"^#(include|pragma once).*$", "", header, flags=re.M)
    source = re.sub(r"^#include.*$", "", source, flags=re.M)
    program = (
        SHIM
        + header
        + source
        + """
int main() {
  char data[DATASIZE], previous[DATASIZE] = {}, base[] = "test";
  PubSubClient client;
  setTopicGroups(TOPIC_GROUPS_ALL);
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
