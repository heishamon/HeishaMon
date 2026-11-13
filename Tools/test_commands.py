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
    # installer menu settings, recorded from their per-command functions before they became table rows
    "SetAntiFreezeMode": (20, [None, 4, 8, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetPoolDelta": (58, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetDemandControlEnable": (27, [None, 1, 2, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetDHWDefrost": (30, [None, 1, 2, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetStaticPressure": (31, [None, 16, 32, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetHeatingOnOutdoorTemp": (108, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetHeatingOnDelay": (109, [None, None, None, None, None, None, None, None, 3, None, None, None, 11, None, None, None, None, None]),
    "SetDHWBoosterDelay": (60, [None, None, 2, 3, 4, 7, None, 22, 61, 76, 128, None, None, None, None, None, None, None]),
    "SetEfficiencyStage1Time": (33, [None, 1, None, None, None, None, None, None, 13, None, None, None, None, None, None, None, None, None]),
    "SetEfficiencyStage2Time": (33, [None, 16, None, None, None, None, None, None, 208, None, None, None, None, None, None, None, None, None]),
    "SetEfficiencyStage3Time": (34, [None, 1, None, None, None, None, None, None, 13, None, None, None, None, None, None, None, None, None]),
    "SetSensorOffsetRemoteTh1": (35, [None, 128, 160, 192, 224, None, None, None, None, None, None, None, None, 160, None, None, None, None]),
    "SetSensorOffsetOutdoor": (35, [None, 8, 10, 12, 14, None, None, None, None, None, None, None, None, 10, None, None, None, None]),
    "SetBivalentAPHeat": (26, [None, 16, 32, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetBivalentOperatingMode": (27, [None, 16, 32, 48, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetBivalentExternalPump": (27, [None, 64, 128, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetBivalentAPStartDelay": (67, [None, None, 2, 3, 4, 7, None, 22, 61, 76, 128, None, None, None, None, None, None, None]),
    "SetBivalentAPStopDelay": (69, [None, None, 2, 3, 4, 7, None, 22, 61, 76, 128, None, None, None, None, None, None, None]),
    "SetExtraPump": (31, [None, 5, 10, 3, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetExtraPumpMode": (31, [None, 4, 8, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetInternalHeaterCapacity": (25, [None, None, None, None, 4, 8, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetSolarOnDelta": (61, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetSolarOffDelta": (62, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetSolarFrostProtection": (63, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetRoomHolidayShiftTemp": (43, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetZoneType": (21, [None, 4, 8, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetHeatingWaterMode": (28, [None, 1, 2, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetCoolingWaterMode": (28, [None, 4, 8, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetLiquidType": (20, [None, 64, 128, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetDHWHeaterType": (25, [None, 1, 2, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetDHWInstalled": (24, [None, 1, 2, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetSolarMode": (24, [None, 16, 32, 48, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetSolarHighLimit": (64, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetSGReadyMode": (27, [None, 4, 8, 12, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetSGReadyDHWPower1": (51, [None, None, 11, 21, 31, 61, None, 211, None, None, None, None, None, 11, 216, None, None, None]),
    "SetSGReadyHeatPower1": (52, [None, None, 11, 21, 31, 61, None, 211, None, None, None, None, None, 11, 216, None, None, None]),
    "SetSGReadyCoolPower1": (53, [None, None, 11, 21, 31, 61, None, 211, None, None, None, None, None, 11, 216, None, None, None]),
    "SetSGReadyDHWPower2": (54, [None, None, 11, 21, 31, 61, None, 211, None, None, None, None, None, 11, 216, None, None, None]),
    "SetSGReadyHeatPower2": (55, [None, None, 11, 21, 31, 61, None, 211, None, None, None, None, None, 11, 216, None, None, None]),
    "SetSGReadyCoolPower2": (56, [None, None, 11, 21, 31, 61, None, 211, None, None, None, None, None, 11, 216, None, None, None]),
    "SetSGReadyStopPower": (57, [None, None, 11, 21, 31, 61, None, 211, None, None, None, None, None, 11, 216, None, None, None]),
    "SetSGReadyCoolCapacity1": (102, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetSGReadyCoolCapacity2": (103, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetSensorOffsetReturn": (36, [None, 128, 160, 192, 224, None, None, None, None, None, None, None, None, 160, None, None, None, None]),
    "SetZ1SensorSettings": (22, [None, 1, 2, 3, 4, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetDefrostSignal": (28, [None, 64, 128, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetSensorOffsetDhwTank": (36, [None, 8, 10, 12, 14, None, None, None, None, None, None, None, None, 10, None, None, None, None]),
    "SetSensorOffsetFlow": (37, [None, 8, 10, 12, 14, None, None, None, None, None, None, None, None, 10, None, None, None, None]),
    # added later; expected bytes from the encoding of the row, not recorded from an older function
    "SetSGReadyHeatCapacity1": (71, [None, 1, 2, 3, 4, 7, None, 22, 61, 76, 128, None, None, None, None, None, None, None]),
    "SetSGReadyDHWCapacity1": (72, [None, 1, 2, 3, 4, 7, None, 22, 61, 76, 128, None, None, None, None, None, None, None]),
    "SetSGReadyHeatCapacity2": (73, [None, 1, 2, 3, 4, 7, None, 22, 61, 76, 128, None, None, None, None, None, None, None]),
    "SetSGReadyDHWCapacity2": (74, [None, 1, 2, 3, 4, 7, None, 22, 61, 76, 128, None, None, None, None, None, None, None]),
    "SetBivalentAPDHW": (26, [None, 64, 128, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetOptionalPCB": (20, [None, 1, 2, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
    "SetDHWHolidayShiftTemp": (44, [None, 128, 129, 130, 131, 134, 123, 149, 188, 203, 255, None, None, None, None, None, None, None]),
    "SetDryConcrete": (5, [None, 1, 2, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None]),
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
    "SetSensorOffsetRemoteTh1": (35, [("-3.5", 0x10), ("-0.5", 0x70), ("0", 0x80), ("1.5", 0xB0), ("3.5", 0xF0)], ["4", "-4", "0.25", "3.6"]),
    "SetSensorOffsetFlow": (37, [("-3.5", 0x01), ("0.0", 0x08), ("3.5", 0x0F)], ["4", "0.3", "1,5"]),
    "SetSGReadyHeatPower2": (55, [("0.1", 2), ("4.2", 43), ("10", 101), ("25.4", 255)], ["0", "25.5", "-1"]),
    "SetEfficiencyStage2Time": (33, [("0", 0x10), ("5", 0x20), ("70", 0xF0)], ["3", "75", "-5"]),
    "SetHeatingOnDelay": (109, [("30", 2), ("7620", 255)], ["0", "45", "7650"]),
    "SetInternalHeaterCapacity": (25, [("3", 0x04), ("6", 0x08), ("9", 0x0C)], ["0", "7", "12"]),
    "SetExtraPump": (31, [("0", 0x05), ("1", 0x0A), ("2", 0x03)], ["3", "-1"]),
    "SetExtraPumpMode": (31, [("0", 0x04), ("1", 0x08)], ["2", "-1"]),
    "SetSolarMode": (24, [("0", 0x10), ("1", 0x20), ("2", 0x30)], ["3", "-1"]),
    "SetZ1SensorSettings": (22, [("0", 1), ("3", 4)], ["4", "-1"]),
}
DECIMAL = {"SetSensorOffsetRemoteTh1", "SetSensorOffsetFlow", "SetSGReadyHeatPower2"}
REFUSED = ["", "abc", "1x", "1.0", " 1", "+1", "1 ", "-", "999999999999999"]

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
  long toInt() const { return atol(s.c_str()); }
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

    checks = []
    for topic, (address, values) in RECORDED.items():
        for payload, value in zip(PAYLOADS, values):
            checks.append(
                CASE.format(topic=topic, payload=payload, address=address, value=-1 if value is None else value)
            )
    for topic, (address, accepted, refused) in CHECKED.items():
        for payload, value in accepted:
            checks.append(CASE.format(topic=topic, payload=payload, address=address, value=value))
        # a payload with one decimal ("1.0") is valid for the commands that take tenths
        for payload in refused + [p for p in REFUSED if not (p == "1.0" and topic in DECIMAL)]:
            checks.append(CASE.format(topic=topic, payload=payload, address=address, value=-1))

    program = SHIM + header + template + encoder + "\n".join(stubs) + dispatcher
    program += MAIN + "".join(checks) + GATE + "\n}\n"
    with tempfile.TemporaryDirectory() as directory:
        cpp = Path(directory) / "test.cpp"
        binary = Path(directory) / "test"
        cpp.write_text(program)
        subprocess.run(["g++", "-std=c++17", str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print(f"PASS: {len(checks)} command cases for {len(names) - 1} commands (recorded bytes, boundaries, refused payloads)")


if __name__ == "__main__":
    main()
