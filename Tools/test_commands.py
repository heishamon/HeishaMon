"""Test the new setters and real command dispatch with g++ on the host.

Run: python3 Tools/test_commands.py
"""

import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# The byte every main command sends for the payloads below: (byte, [value per payload]). None means the
# payload is refused and nothing is sent. 0 is sent as a datagram that changes nothing. These values were
# recorded from the per-command functions the table in commands.h replaced, so that table has to keep
# producing exactly the same datagrams. SetCurves takes JSON and is not covered here.
PAYLOADS = ["", "0", "1", "2", "3", "6", "-5", "21", "60", "75", "127", "255", "300", "1.0", "21.5", " 1", "1x", "abc"]
RECORDED = {
    "SetHeatpump": (4, [1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 2, 1]),
    "SetPump": (4, [16, 16, 32, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 32, 16, 32, 32, 16]),
    "SetMaxPumpDuty": (45, [1, 1, 2, 3, 4, 7, 252, 22, 61, 76, 128, 0, 45, 2, 22, 2, 2, 1]),
    "SetQuietMode": (7, [8, 8, 16, 24, 32, 56, 224, 176, 232, 96, 0, 0, 104, 16, 176, 16, 16, 8]),
    "SetZ1HeatRequestTemperature": (38, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetZ1CoolRequestTemperature": (39, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetZ2HeatRequestTemperature": (40, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetZ2CoolRequestTemperature": (41, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetForceDHW": (4, [64, 64, 128, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 128, 64, 128, 128, 64]),
    "SetForceDefrost": (8, [0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 2, 0]),
    "SetForceSterilization": (8, [0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 4, 4, 0]),
    "SetSterilizationTemp": (100, [None, None, None, None, None, None, None, None, 188, 203, None, None, None, None, None, None, None, None]),
    "SetSterilizationMaxTime": (101, [None, None, None, None, None, None, None, None, 61, None, None, None, None, None, None, None, None, None]),
    "SetForceHeater": (5, [4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 4, 8, 8, 4]),
    "SetHolidayMode": (5, [16, 16, 32, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 32, 16, 32, 32, 16]),
    "SetPowerfulMode": (7, [1, 1, 2, 3, 4, 7, 4, 6, 5, 4, 0, 0, 5, 2, 6, 2, 2, 1]),
    "SetOperationMode": (6, [18, 18, 19, 24, 33, 40, 0, 0, 0, 0, 0, 0, 0, 19, 0, 19, 19, 18]),
    "SetDHWTemp": (42, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetZones": (6, [64, 64, 128, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 128, 64]),
    "SetFloorHeatDelta": (84, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetFloorCoolDelta": (94, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetDHWHeatDelta": (99, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetReset": (8, [0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0]),
    "SetHeaterDelayTime": (104, [1, 1, 2, 3, 4, 7, 252, 22, 61, 76, 128, 0, 45, 2, 22, 2, 2, 1]),
    "SetHeaterStartDelta": (105, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetHeaterStopDelta": (106, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetMainSchedule": (5, [64, 64, 128, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 128, 64, 128, 128, 64]),
    "SetAltExternalSensor": (20, [16, 16, 32, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 32, 16, 32, 32, 16]),
    "SetExternalPadHeater": (25, [16, 16, 32, 48, 16, 16, 16, 16, 16, 16, 16, 16, 16, 32, 16, 32, 32, 16]),
    "SetBufferDelta": (59, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetBuffer": (24, [4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 4, 8, 8, 4]),
    "SetHeatingOffOutdoorTemp": (83, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetExternalControl": (23, [1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 2, 1]),
    "SetExternalError": (23, [16, 16, 32, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 32, 16, 32, 32, 16]),
    "SetExternalCompressorControl": (23, [64, 64, 128, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 128, 64, 128, 128, 64]),
    "SetExternalHeatCoolControl": (23, [4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 4, 8, 8, 4]),
    "SetBivalentControl": (26, [1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 2, 1]),
    "SetBivalentMode": (26, [4, 4, 8, 12, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 4, 8, 8, 4]),
    "SetBivalentStartTemp": (65, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetBivalentAPStartTemp": (66, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetBivalentAPStopTemp": (68, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetHeatingControl": (30, [4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 4, 8, 8, 4]),
    "SetSmartDHW": (24, [64, 64, 128, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 128, 64, 128, 128, 64]),
    "SetQuietModePriority": (11, [16, 16, 32, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 32, 16, 32, 32, 16]),
    "SetPumpFlowrateMode": (29, [16, 16, 32, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 32, 16, 32, 32, 16]),
    "SetDHWSensorSelection": (11, [1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 2, 1]),
    "SetDHWHeaterState": (9, [4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 4, 8, 8, 4]),
    "SetRoomHeaterState": (9, [1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 2, 1]),
    "SetHeaterOnOutdoorTemp": (85, [128, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, 127, 172, 129, 149, 129, 129, 128]),
    "SetHeatToCoolTemp": (95, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetCoolToHeatTemp": (96, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetRoomOperationMaxTime": (97, [None, None, None, None, None, None, None, None, 3, None, None, None, 11, None, None, None, None, None]),
    "SetDHWHeatupMaxTime": (98, [None, None, 2, 3, 4, 7, None, 22, 61, 76, 128, None, None, None, None, None, None, None]),
    "SetQuietModeSchedule": (7, [None, 64, 128, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
}

# Checked commands: (byte, [(payload, value)], [refused payloads])
CHECKED = {
    "SetHeatToCoolTemp": (
        95,
        [("21", 149), ("-127", 1), ("127", 255), ("0", 128)],
        ["-128", "128"],
    ),
    "SetCoolToHeatTemp": (
        96,
        [("12", 140), ("-127", 1), ("127", 255)],
        ["-128", "128"],
    ),
    "SetRoomOperationMaxTime": (
        97,
        [("90", 4), ("30", 2), ("7620", 255)],
        ["0", "29", "31", "7650"],
    ),
    "SetDHWHeatupMaxTime": (
        98,
        [("105", 106), ("1", 2), ("254", 255)],
        ["0", "255", "-1"],
    ),
    "SetQuietModeSchedule": (7, [("0", 64), ("1", 128)], ["2", "-1"]),
    "SetSterilizationTemp": (100, [("55", 183), ("75", 203)], ["54", "76", "-55"]),
    "SetSterilizationMaxTime": (101, [("5", 6), ("60", 61)], ["0", "4", "7", "65"]),
}
REFUSED = ["", "abc", "1x", "1.0", " 1", "+1", "1 ", "-", "999999999999999"]

# CMD_REPORTED commands: (readback topic, payloads). Against all 256 values of the topic's byte, such a command is
# not sent exactly when the decoded topic equals the payload. These must be the commands flagged CMD_REPORTED.
REPORTED = {
    "SetHeatpump": (0, "01"),
    "SetMaxPumpDuty": (95, ["0", "64", "254"]),
    "SetQuietMode": (18, "0123"),
    "SetZ1HeatRequestTemperature": (27, ["-5", "0", "35"]),
    "SetZ1CoolRequestTemperature": (28, ["-5", "18"]),
    "SetZ2HeatRequestTemperature": (34, ["5", "40"]),
    "SetZ2CoolRequestTemperature": (35, ["-1", "20"]),
    "SetSterilizationTemp": (70, ["55", "75"]),
    "SetSterilizationMaxTime": (71, ["5", "60"]),
    "SetOperationMode": (4, "01345"),
    "SetDHWTemp": (9, ["40", "48", "75"]),
    "SetZones": (94, "012"),
    "SetFloorHeatDelta": (23, ["1", "15"]),
    "SetFloorCoolDelta": (24, ["1", "15"]),
    "SetDHWHeatDelta": (22, ["-12", "-2"]),
    "SetHeaterDelayTime": (96, ["0", "30"]),
    "SetHeaterStartDelta": (97, ["-5", "1"]),
    "SetHeaterStopDelta": (98, ["2", "10"]),
    "SetMainSchedule": (13, "01"),
    "SetAltExternalSensor": (108, "01"),
    "SetExternalPadHeater": (114, "012"),
    "SetBufferDelta": (113, ["0", "10"]),
    "SetBuffer": (99, "01"),
    "SetHeatingOffOutdoorTemp": (77, ["5", "35"]),
    "SetExternalControl": (119, "01"),
    "SetExternalError": (121, "01"),
    "SetExternalCompressorControl": (122, "01"),
    "SetExternalHeatCoolControl": (120, "01"),
    "SetBivalentControl": (129, "01"),
    "SetBivalentMode": (130, "012"),
    "SetBivalentStartTemp": (131, ["-15", "35"]),
    "SetBivalentAPStartTemp": (134, ["-15", "0"]),
    "SetBivalentAPStopTemp": (135, ["10", "35"]),
    "SetHeatingControl": (139, "01"),
    "SetSmartDHW": (140, "01"),
    "SetQuietModePriority": (141, "01"),
    "SetPumpFlowrateMode": (106, "01"),
    "SetDHWSensorSelection": (143, "01"),
    "SetDHWHeaterState": (58, "01"),
    "SetRoomHeaterState": (59, "01"),
    "SetHeaterOnOutdoorTemp": (78, ["-15", "20"]),
    "SetHeatToCoolTemp": (79, ["-127", "21", "127"]),
    "SetCoolToHeatTemp": (80, ["12"]),
    "SetRoomOperationMaxTime": (158, ["30", "90", "7620"]),
    "SetDHWHeatupMaxTime": (159, ["1", "254"]),
    "SetQuietModeSchedule": (3, "01"),
}
# always sent while the topic reads back the payload: Auto and Auto + DHW are reported with another code, force DHW
# is an action
NOT_REPORTED = {"SetOperationMode": (4, "26"), "SetForceDHW": (2, "01")}

SHIM = r"""
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
using byte = unsigned char;
#define PROGMEM
#define memcpy_P memcpy
#define snprintf_P snprintf
#define PSTR(x) x
struct String {
  std::string s;
  String(const char *c) : s(c ? c : "") {}
  String(int n) : s(std::to_string(n)) {}
  String(float n, int digits) { char buf[64]; snprintf(buf, sizeof(buf), "%.*f", digits, n); s = buf; }
  long toInt() const { return atol(s.c_str()); }
  const char *c_str() const { return s.c_str(); }
};
std::vector<byte> sent;
int send_count;
bool send_command(byte* data, int length) {
  ++send_count; sent.assign(data, data + length); return true;
}
void log_message(char*) {}
"""

# one command through the real dispatcher: value None = refused, nothing sent and the buffer left untouched
CASE = r"""
  {{ char topic[] = "{topic}", payload[] = "{payload}";
    sent.clear(); send_count = 0;
    send_heatpump_command(topic, payload, send_command, log_message, false);
    check(topic, payload, {address}, {value});
  }}
"""

MAIN = r"""
static const cmdStruct *find(const char *topic) {
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    if (strcmp(commands[i].name, topic) == 0) return &commands[i];
  return nullptr;
}

static void fail(const char *topic, const char *payload, const char *why) {
  printf("FAIL %s '%s': %s\n", topic, payload, why);
  exit(1);
}

static void check(const char *topic, char *payload, int address, int value) {
  const cmdStruct *command = find(topic);
  if (!command) fail(topic, payload, "not in commands[]");
  if (value < 0) {
    if (send_count != 0) fail(topic, payload, "sent, expected refused");
    byte buffer[256]; memset(buffer, 0xa5, sizeof(buffer));
    char log[256];
    if (heatpump_command(command, payload, buffer, log) != 0) fail(topic, payload, "length not 0");
    for (byte b : buffer) if (b != 0xa5) fail(topic, payload, "buffer touched");
    return;
  }
  if (send_count != 1 || sent.size() != sizeof(panasonicSendQuery)) fail(topic, payload, "not sent once");
  for (unsigned int i = 0; i < sent.size(); ++i) {
    byte expected = (int)i == address ? value : panasonicSendQuery[i];
    if (sent[i] != expected) {
      printf("FAIL %s '%s': byte %u is 0x%02X, expected 0x%02X\n", topic, payload, i, sent[i], expected);
      exit(1);
    }
  }
}

int main() {
"""

# skip_settings_command() is in HeishaMon.ino; here 'reported' is the fresh main data block, nullptr = none
SKIP = r"""
const char *reported = nullptr;
bool skip_settings_command(const cmdStruct *command, unsigned char *cmd, unsigned int len) {
  return reported && heatpump_command_unchanged(command, cmd, len, reported);
}
"""

# one payload against every value of its readback byte: compared, and sent or not through the real dispatcher
UNCHANGED_CASE = r"""
  {{ char topic[] = "{topic}", payload[] = "{payload}";
    sent.clear(); send_count = 0; reported = nullptr;
    send_heatpump_command(topic, payload, send_command, log_message, false);
    std::vector<byte> cmd = sent;
    char data[DATASIZE]; memset(data, 0x55, sizeof(data)); data[0] = 0x71;
    reported = data;
    for (int b = 0; b < 256; ++b) {{
      data[{address}] = b;
      bool reports = std::string({decoder}(b).c_str()) == "{payload}";
      bool unchanged = heatpump_command_unchanged(find(topic), cmd.data(), cmd.size(), data);
      if ({compared} ? unchanged != reports : unchanged && reports) fail(topic, payload, "wrong comparison");
      send_count = 0;
      send_heatpump_command(topic, payload, send_command, log_message, false);
      if (send_count != (unchanged ? 0 : 1)) fail(topic, payload, "sent although unchanged, or not sent");
      data[0] = 0;
      if (heatpump_command_unchanged(find(topic), cmd.data(), cmd.size(), data)) fail(topic, payload, "no data block");
      data[0] = 0x71;
    }}
    reported = nullptr;
  }}
"""

# an installer command is refused until installer commands are allowed, a null payload never crashes
GATE = r"""
  { const cmdStruct installer = { "SetTest", CMD_INSTALLER, CMD_ONOFF(4, 0) };
    byte buffer[256]; char log[256]; char payload[] = "1";
    memset(buffer, 0xa5, sizeof(buffer));
    assert(heatpump_command(&installer, payload, buffer, log) == 0);
    for (byte b : buffer) assert(b == 0xa5);
    setInstallerCommandsAllowed(true);
    assert(heatpump_command(&installer, payload, buffer, log) == sizeof(panasonicSendQuery) && buffer[4] == 2);
    setInstallerCommandsAllowed(false);
    assert(heatpump_command(&installer, payload, buffer, log) == 0);
  }
  // payloads with one decimal: a sensor offset in a high nibble (0.5 K steps) and a whole byte in 0.1 steps
  { const cmdStruct offset = { "SetOffset", 0, CMD_DECIMAL(35, 4, -35, 35, 5, 5, 8) };
    const cmdStruct power = { "SetPower", 0, CMD_DECIMAL(51, 0, 1, 254, 1, 1, 1) };
    struct { const cmdStruct *command; const char *payload; int value; } cases[] = {
      { &offset, "-3.5", 0x10 }, { &offset, "-0.5", 0x70 }, { &offset, "0", 0x80 }, { &offset, "0.0", 0x80 },
      { &offset, "0.5", 0x90 }, { &offset, "2", 0xC0 }, { &offset, "3.5", 0xF0 }, { &offset, "-0", 0x80 },
      { &offset, "3.6", -1 }, { &offset, "4", -1 }, { &offset, "-4", -1 }, { &offset, "0.25", -1 },
      { &offset, "1.", -1 }, { &offset, ".5", -1 }, { &offset, "-", -1 }, { &offset, "-.5", -1 },
      { &offset, "", -1 }, { &offset, "abc", -1 }, { &offset, "1.5x", -1 }, { &offset, " 1", -1 },
      { &offset, "1,5", -1 }, { &offset, "0.5.0", -1 }, { &offset, "99999999", -1 },
      { &power, "0.1", 2 }, { &power, "4", 41 }, { &power, "4.2", 43 }, { &power, "25.4", 255 },
      { &power, "0", -1 }, { &power, "0.0", -1 }, { &power, "25.5", -1 }, { &power, "-1", -1 },
    };
    for (auto &c : cases) {
      byte buffer[256]; char log[256]; char payload[16];
      memset(buffer, 0xa5, sizeof(buffer)); strcpy(payload, c.payload);
      unsigned int length = heatpump_command(c.command, payload, buffer, log);
      if (c.value < 0) {
        if (length != 0) fail(c.command->name, c.payload, "length not 0");
        for (byte b : buffer) if (b != 0xa5) fail(c.command->name, c.payload, "buffer touched");
      } else if (length != sizeof(panasonicSendQuery) || buffer[c.command->address] != c.value) {
        printf("FAIL %s '%s': 0x%02X, expected 0x%02X\\n", c.command->name, c.payload, buffer[c.command->address], c.value);
        exit(1);
      }
    }
    assert(heatpump_command(&offset, nullptr, nullptr, (char *)memset(new char[256], 0, 256)) == 0);
  }
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if (commands[i].type == CMD_FUNC) continue;
    byte buffer[256]; char log[256];
    memset(buffer, 0xa5, sizeof(buffer));
    unsigned int length = heatpump_command(&commands[i], nullptr, buffer, log);
    if (commands[i].type == CMD_STRICT || commands[i].type == CMD_TENTHS) { assert(length == 0); for (byte b : buffer) assert(b == 0xa5); }
    else assert(length == sizeof(panasonicSendQuery));
    assert(commands[i].address >= 4 && commands[i].address < sizeof(panasonicSendQuery));
    assert(commands[i].divisor > 0 && commands[i].step > 0);
  }
"""


def main():
    header = (ROOT / "HeishaMon/commands.h").read_text()
    source = (ROOT / "HeishaMon/commands.cpp").read_text()
    docs = (ROOT / "MQTT-Topics.md").read_text()
    names = re.findall(r'^  \{ "(Set\w+)", ', header[: header.index("struct optCmdStruct")], re.M)
    assert set(names) == set(RECORDED) | {"SetCurves"}, set(names) ^ (set(RECORDED) | {"SetCurves"})
    for name in names:
        assert f"| {name} |" in docs or f"`{name}`" in docs or name in docs, name

    template = re.search(r"byte panasonicSendQuery\[\].*?\};", source, re.S)[0]
    encoder = source[
        source.index("static bool installerCommandsAllowed") : source.index(
            "//start of optional pcb commands"
        )
    ]
    # SetCurves needs ArduinoJson, which the host build doesn't have
    encoder = (
        encoder[: encoder.index("unsigned int set_curves(")]
        + "unsigned int set_curves(char *, unsigned char *, char *) { return 0; }\n"
        + encoder[encoder.index("unsigned int set_zones(") :]
    )
    stubs = [
        f"unsigned int {name}({args}) {{ return 0; }}"
        for name, args in re.findall(
            r"unsigned int (set_\w+)\((char \*msg, char \*log_msg)\);", header
        )
    ]
    dispatcher = source[
        source.index("void send_heatpump_command(") : source.index(
            "// The optional PCB state is saved"
        )
    ]
    header = re.sub(r"^#include.*$", "", header, flags=re.M)
    reported = re.findall(r'^  \{ "(Set\w+)", CMD_REPORTED, ', header, re.M)
    assert set(reported) == set(REPORTED), set(reported) ^ set(REPORTED)
    decode_header = (ROOT / "HeishaMon/decode.h").read_text()
    decode_source = (ROOT / "HeishaMon/decode.cpp").read_text()
    readback = {}
    for table in ("topicBytes", "topicFunctions"):
        body = re.search(r"\b" + table + r"\[\].*?=\s*\{(.*?)\};", decode_header, re.S)[1]
        readback[table] = [line.split("//")[0].strip().rstrip(",") for line in body.splitlines() if line.split("//")[0].strip()]
    decoders = decode_source[decode_source.index("String getRawByte(") : decode_source.index("String getModel(")]

    checks = []
    for topic, (address, values) in RECORDED.items():
        for payload, value in zip(PAYLOADS, values):
            checks.append(
                CASE.format(topic=topic, payload=payload, address=address, value=-1 if value is None else value)
            )
    for topic, (address, accepted, refused) in CHECKED.items():
        for payload, value in accepted:
            checks.append(CASE.format(topic=topic, payload=payload, address=address, value=value))
        for payload in refused + REFUSED:
            checks.append(CASE.format(topic=topic, payload=payload, address=address, value=-1))
    unchanged = 0
    for table, compared in ((REPORTED, "true"), (NOT_REPORTED, "false")):
        for topic, (readback_topic, payloads) in table.items():
            address = readback["topicBytes"][readback_topic]
            decoder = readback["topicFunctions"][readback_topic]
            for payload in payloads:
                checks.append(UNCHANGED_CASE.format(topic=topic, payload=payload, address=address, decoder=decoder, compared=compared))
                unchanged += 1

    program = SHIM + header + template + encoder + "\n".join(stubs) + SKIP + dispatcher + decoders
    program += MAIN + "".join(checks) + GATE + "\n}\n"
    with tempfile.TemporaryDirectory() as directory:
        cpp = Path(directory) / "test.cpp"
        binary = Path(directory) / "test"
        cpp.write_text(program)
        subprocess.run(["g++", "-std=c++17", str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print(f"PASS: {len(checks)} command cases for {len(names) - 1} commands (recorded bytes, boundaries, refused payloads, {unchanged} unchanged values)")


if __name__ == "__main__":
    main()
