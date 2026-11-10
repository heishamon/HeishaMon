"""Test the new setters and real command dispatch with g++ on the host.

Run: python3 Tools/test_commands.py
"""

import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SETTERS = {
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
    # settings in a byte of their own (no data block needed)
    # installer menu: 0..10 K
    "SetPoolDelta": (
        58,
        [("0", 128), ("7", 135), ("10", 138)],
        ["-1", "11", "-127", "127", "-128", "128"],
    ),
    # 5..34 degC (menu: up to heating OFF - 1, OFF is at most 35), and below the heating OFF temperature
    # (actData[83] = 0x98 = 24 degC) while a data block exists
    "SetHeatingOnOutdoorTemp": (
        108,
        [("15", 143), ("14", 142), ("5", 133), ("23", 151)],
        ["4", "24", "25", "34", "35", "127", "-127", "0", "-128", "128"],
    ),
    # installer menu: 0:30 to 24:00 in 30 min steps
    "SetHeatingOnDelay": (
        109,
        [("30", 2), ("60", 3), ("1440", 49)],
        ["0", "29", "31", "45", "1415", "1441", "1470", "7620", "7650"],
    ),
    # installer menu: 0:20 to 3:00 in 5 min steps
    "SetDHWBoosterDelay": (
        60,
        [("90", 91), ("95", 96), ("20", 21), ("180", 181)],
        ["0", "1", "15", "19", "21", "92", "185", "254", "255", "-1"],
    ),
    # single-byte temperatures: no other field in the byte
    # installer menu limits: on delta 5..15 K, off delta 2..10 K, anti-freeze temperature -20..10 degC
    "SetSolarOnDelta": (
        61,
        [("10", 138), ("11", 139), ("5", 133), ("15", 143)],
        ["4", "16", "0", "-5", "-127", "127", "-128", "128"],
    ),
    "SetSolarOffDelta": (
        62,
        [("5", 133), ("6", 134), ("2", 130), ("10", 138)],
        ["1", "11", "0", "-2", "-127", "127", "-128", "128"],
    ),
    "SetSolarFrostProtection": (
        63,
        [("5", 133), ("6", 134), ("10", 138), ("0", 128), ("-20", 108)],
        ["-21", "11", "-127", "127", "-128", "128"],
    ),
    "SetRoomHolidayShiftTemp": (
        43,
        [("-10", 118), ("-9", 119), ("-127", 1), ("127", 255), ("0", 128)],
        ["-128", "128"],
    ),
}
# Read-modify-write setters for bytes with several fields (and commands that need a state of the data
# block). Each accepted case is (actData overrides, payload, expected value of the byte); the rest of
# the command must stay the empty template. Blocked cases are valid payloads in a state where the
# command must refuse. Optional last element: {"skip": generic payloads that are valid for this command,
# "gate": byte that must not be 0 (default: the written byte)}.
RMW_SETTERS = {
    # solar high limit, byte 64 of its own, 70..90 degC (SM p.182); needs a data block and byte 64 != 0 like the other
    # commands of the thinner-evidence block (the pump reverted a raw write, see MQTT-Topics.md)
    "SetSolarHighLimit": (
        64,
        [({}, "80", 208), ({}, "79", 207), ({}, "70", 198), ({}, "90", 218)],
        [],
        ["69", "91", "0", "-1", "127", "-127", "128", "-128", "255"],
    ),
    # byte 20 bits 3-2 (b01 off, b10 on); only this pair is sent, the rest of the byte is 0 (no change)
    "SetAntiFreezeMode": (
        20,
        [
            ({20: 0x6A}, "0", 0x04),
            ({20: 0x66}, "1", 0x08),
            ({20: 0xEA}, "0", 0x04),
            ({20: 0x2A}, "1", 0x08),
        ],
        [],
        ["2", "-1"],
    ),
    # byte 27 bits 1-0 (b01 disabled, b10 enabled); only this pair is sent
    "SetDemandControlEnable": (
        27,
        [
            ({27: 0x55}, "1", 0x02),
            ({27: 0x56}, "0", 0x01),
            ({27: 0x5D}, "1", 0x02),
            ({27: 0x95}, "1", 0x02),
            ({27: 0xAA}, "0", 0x01),
        ],
        [],
        ["2", "-1"],
    ),
    # byte 30 bits 1-0 (b01 no, b10 yes); only this pair is sent
    "SetDHWDefrost": (
        30,
        [
            ({30: 0x5A}, "0", 0x01),
            ({30: 0x59}, "1", 0x02),
            ({30: 0x5E}, "0", 0x01),
            ({30: 0x1A}, "0", 0x01),
        ],
        [],
        ["2", "-1"],
    ),
    # byte 31 bits 5-4 (b01 no, b10 yes); only this pair is sent
    "SetStaticPressure": (
        31,
        [
            ({31: 0x5A}, "1", 0x20),
            ({31: 0x6A}, "0", 0x10),
            ({31: 0x6A}, "1", 0x20),
            ({31: 0x5B}, "1", 0x20),
            ({31: 0x57}, "0", 0x10),
            ({31: 0xDA}, "1", 0x20),
            ({31: 0x55}, "0", 0x10),
        ],
        [],
        ["2", "-1"],
    ),
    # byte 33 low nibble, minutes / 5 + 1; the high nibble (stage 2) is sent as 0 (no change)
    "SetEfficiencyStage1Time": (
        33,
        [
            ({}, "25", 0x06),
            ({}, "20", 0x05),
            ({}, "0", 0x01),
            ({}, "60", 0x0D),
            ({33: 0x85}, "25", 0x06),
        ],
        [],
        ["-5", "3", "65", "70", "75"],
    ),
    "SetEfficiencyStage2Time": (
        33,
        [
            ({}, "35", 0x80),
            ({}, "30", 0x70),
            ({}, "0", 0x10),
            ({}, "60", 0xD0),
            ({33: 0x76}, "35", 0x80),
        ],
        [],
        ["-5", "3", "65", "70", "75"],
    ),
    # byte 34 low nibble, 0..30 min; the high nibble (anti-stick bits) is sent as 0 like the cloud frame
    # (write 0x07 where the read frame shows 0xA7), but a data block with a nonzero byte is still required
    "SetEfficiencyStage3Time": (
        34,
        [
            ({}, "15", 0x04),
            ({}, "30", 0x07),
            ({}, "0", 0x01),
            ({34: 0x67}, "15", 0x04),
            ({34: 0x67}, "30", 0x07),
            ({34: 0x0F}, "20", 0x05),
        ],
        [],
        ["-5", "3", "35"],
    ),
    # byte 35 nibbles, K / 0.5 + 8; the other nibble is sent as 0 (no change)
    "SetSensorOffsetRemoteTh1": (
        35,
        [
            ({}, "1", 0xA0),
            ({}, "0.5", 0x90),
            ({}, "0", 0x80),
            ({}, "-3", 0x20),
            ({}, "3.0", 0xE0),
            ({}, "-0.5", 0x70),
            ({35: 0x8E}, "1", 0xA0),
            ({35: 0x82}, "-3", 0x20),
        ],
        [],
        ["-3.5", "3.5", "4", "-4", "0.25", "1.", ".5", "1.55", "10"],
        {"skip": {"1.0"}},
    ),
    "SetSensorOffsetOutdoor": (
        35,
        [
            ({}, "-1", 0x06),
            ({}, "2", 0x0C),
            ({}, "0", 0x08),
            ({}, "-3", 0x02),
            ({}, "3", 0x0E),
            ({}, "0.5", 0x09),
            ({35: 0xE6}, "-1.5", 0x05),
            ({35: 0x26}, "3", 0x0E),
        ],
        [],
        ["-3.5", "3.5", "4", "-4", "0.25", "1.", ".5", "1.55", "10"],
        {"skip": {"1.0"}},
    ),
    # byte 26 bits 5-4 (b01 no, b10 yes); needs bits 3-2 = b11 (advanced parallel) and bivalent control = yes (bits 1-0 = b10)
    "SetBivalentAPHeat": (
        26,
        [
            ({26: 0x5E}, "1", 0x20),
            ({26: 0x6E}, "0", 0x10),
            ({26: 0xDE}, "1", 0x20),
        ],
        [
            ({26: 0x5A}, "1"),
            ({26: 0x56}, "1"),
            ({26: 0x52}, "0"),
            ({26: 0x5D}, "1"),  # bivalent control = no
            ({26: 0x5C}, "1"),
        ],
        ["2", "-1"],
    ),
    # byte 27 bits 5-4 (b01 auto, b10 SG ready, b11 intelligent); only this pair is sent; 1 and 2 need bivalent = yes,
    # 1 (SG ready) also SG ready (bits 3-2) off; 0 (auto) is always possible once byte 27 was read
    "SetBivalentOperatingMode": (
        27,
        [
            ({26: 0x5E, 27: 0x55}, "1", 0x20),
            ({26: 0x5E, 27: 0x55}, "2", 0x30),
            ({26: 0x5E, 27: 0x75}, "0", 0x10),
            ({26: 0x5E, 27: 0x95}, "1", 0x20),
            ({26: 0x5E, 27: 0x59}, "0", 0x10),
            ({26: 0x5E, 27: 0x51}, "1", 0x20),  # SG ready not set (b00) is no conflict
            ({26: 0x5E, 27: 0x76}, "0", 0x10),
            ({26: 0x56, 27: 0x55}, "1", 0x20),
            # only the bivalent operating mode "SG ready" (b10) conflicts with SG ready; "intelligent" does not
            ({26: 0x5E, 27: 0x59}, "2", 0x30),
            ({26: 0x5E, 27: 0x5D}, "2", 0x30),
            # "auto" is the way out and works with bivalent control off too (hidden SG ready / intelligent pattern)
            ({26: 0x5D, 27: 0x65}, "0", 0x10),
            ({26: 0x5D, 27: 0x75}, "0", 0x10),
            ({26: 0x5D, 27: 0x69}, "0", 0x10),
            ({26: 0x5C, 27: 0x65}, "0", 0x10),
            ({26: 0x00, 27: 0x65}, "0", 0x10),
        ],
        [
            ({26: 0x5D, 27: 0x55}, "1"), ({26: 0x5C, 27: 0x55}, "1"), ({26: 0x5F, 27: 0x55}, "1"),
            # 1 and 2 still need bivalent control = yes
            ({26: 0x5D, 27: 0x55}, "2"), ({26: 0x5D, 27: 0x65}, "1"), ({26: 0x5D, 27: 0x65}, "2"), ({26: 0x5F, 27: 0x55}, "2"),
            # the bivalent SG ready input and SG ready (menu 16) share one terminal (manual): bivalent "SG ready" is refused while SG ready is on
            ({26: 0x5E, 27: 0x59}, "1"), ({26: 0x5E, 27: 0x5D}, "1"),
        ],
        ["3", "-1"],
    ),
    # byte 27 bits 7-6 (b01 off, b10 on); needs byte 26 bits 3-2 = b01 (alternative) and bivalent = yes
    "SetBivalentExternalPump": (
        27,
        [
            ({26: 0x56, 27: 0x55}, "1", 0x80),
            ({26: 0x56, 27: 0x95}, "0", 0x40),
            ({26: 0x56, 27: 0x5D}, "1", 0x80),
            ({26: 0x56, 27: 0x65}, "1", 0x80),
        ],
        [
            ({26: 0x5E, 27: 0x55}, "1"),
            ({26: 0x5A, 27: 0x55}, "1"),
            ({26: 0x52, 27: 0x55}, "0"),
            ({26: 0x55, 27: 0x55}, "1"),  # alternative, but bivalent control = no
            ({26: 0x54, 27: 0x55}, "1"),
        ],
        ["2", "-1"],
    ),
    # bivalent delays, byte 67 / 69, minutes + 1 in 5 min steps; need bivalent control = yes
    "SetBivalentAPStartDelay": (
        67,
        [
            ({26: 0x5E}, "30", 31),
            ({26: 0x5E}, "35", 36),
            ({26: 0x5E}, "5", 6),
            ({26: 0x5E}, "250", 251),
            ({26: 0x56}, "35", 36),
        ],
        [({26: 0x5D}, "35"), ({26: 0x5C}, "35"), ({26: 0x5F}, "35")],
        ["0", "1", "4", "7", "254", "255", "-5"],
        {"gate": 26},
    ),
    "SetBivalentAPStopDelay": (
        69,
        [
            ({26: 0x5E}, "30", 31),
            ({26: 0x5E}, "35", 36),
            ({26: 0x5E}, "5", 6),
            ({26: 0x5E}, "250", 251),
            ({26: 0x56}, "35", 36),
        ],
        [({26: 0x5D}, "35"), ({26: 0x5C}, "35"), ({26: 0x5F}, "35")],
        ["0", "1", "4", "7", "254", "255", "-5"],
        {"gate": 26},
    ),
    # byte 31 low nibble: 0 -> 5 (not used), 1 -> A (heating), 2 -> B (DHW efficiency) or 7 if already DHW comfort;
    # 0 and 1 send use and mode (0x5, 0xA), 2 sends only the use pair (0x3) and keeps the mode; bits 7-4 are 0;
    # payload 2 (DHW) needs a connected tank (byte 24 bits 1-0 = b10); all payloads need one of the four observed
    # low nibbles 5, A, B, 7
    "SetExtraPump": (
        31,
        [
            ({24: 0x56, 31: 0x5A}, "2", 0x03),
            ({24: 0x56, 31: 0x5B}, "0", 0x05),
            ({24: 0x56, 31: 0x57}, "0", 0x05),
            ({24: 0x56, 31: 0x55}, "1", 0x0A),
            ({24: 0x56, 31: 0x5B}, "1", 0x0A),
            ({24: 0x56, 31: 0x55}, "2", 0x03),
            ({24: 0x56, 31: 0x57}, "2", 0x03),
            ({24: 0x56, 31: 0x5B}, "2", 0x03),
            ({24: 0x96, 31: 0x6A}, "0", 0x05),
            ({24: 0x56, 31: 0x6B}, "1", 0x0A),
            ({24: 0x56, 31: 0xDA}, "2", 0x03),
            ({24: 0x56, 31: 0x1A}, "0", 0x05),
            # "not used" and "heating" do not need a tank (the cloud saved them with tank connection = No)
            ({24: 0x55, 31: 0x5A}, "0", 0x05),
            ({24: 0x55, 31: 0x5A}, "1", 0x0A),
            ({24: 0x57, 31: 0x5A}, "1", 0x0A),
            ({24: 0x55, 31: 0x55}, "1", 0x0A),
            ({24: 0x00, 31: 0x5B}, "0", 0x05),
        ],
        [
            # DHW use needs a connected tank
            ({24: 0x55, 31: 0x5A}, "2"),
            ({24: 0x57, 31: 0x5A}, "2"),
            ({24: 0x00, 31: 0x5A}, "2"),
            # low nibble not one of the four observed values: nothing is sent, in particular not the current nibble again
            ({24: 0x56, 31: 0x5F}, "2"),
            ({24: 0x56, 31: 0x53}, "2"),
            ({24: 0x56, 31: 0x5F}, "0"),
            ({24: 0x56, 31: 0x59}, "1"),
            ({24: 0x56, 31: 0x58}, "2"),
            ({24: 0x56, 31: 0x50}, "0"),
            ({24: 0x56, 31: 0x56}, "1"),
        ],
        ["3", "-1"],
    ),
    # byte 31 bits 3-2 (0 comfort -> 7, 1 efficiency -> B); needs the extra pump on DHW (bits 1-0 = b11) and a tank
    "SetExtraPumpMode": (
        31,
        [
            ({24: 0x56, 31: 0x5B}, "0", 0x04),
            ({24: 0x56, 31: 0x57}, "1", 0x08),
            ({24: 0x56, 31: 0x5B}, "1", 0x08),
            ({24: 0x56, 31: 0x6B}, "0", 0x04),
            ({24: 0x56, 31: 0xDB}, "0", 0x04),
        ],
        [
            ({24: 0x56, 31: 0x5A}, "0"),
            ({24: 0x56, 31: 0x55}, "1"),
            ({24: 0x55, 31: 0x5B}, "0"),
            ({24: 0x56, 31: 0x5F}, "0"),
            ({24: 0x56, 31: 0x53}, "1"),
        ],
        ["2", "-1"],
    ),
    # byte 25 bits 3-2 (b01 3 kW, b10 6 kW, b11 9 kW from the upstream decode table); only this pair is sent
    "SetInternalHeaterCapacity": (
        25,
        [
            ({25: 0x25}, "6", 0x08),
            ({25: 0x29}, "3", 0x04),
            ({25: 0x26}, "6", 0x08),
            ({25: 0x5E}, "3", 0x04),
            ({25: 0x25}, "9", 0x0C),
            ({25: 0x2D}, "3", 0x04),
            ({25: 0x29}, "9", 0x0C),
            ({25: 0x5E}, "9", 0x0C),
            ({25: 0xAA}, "9", 0x0C),
        ],
        [],
        ["0", "4", "5", "7", "8", "10", "12", "15", "-3", "-9"],
    ),
    # byte 21 bits 3-2 (b01 room, b10 pool), only while the byte reads one of the one-zone codes 0x15 / 0x19
    "SetZoneType": (
        21,
        [
            ({21: 0x15}, "1", 0x08),
            ({21: 0x19}, "0", 0x04),
            ({21: 0x15}, "0", 0x04),
            ({21: 0x19}, "1", 0x08),
        ],
        [
            ({21: 0x16}, "0"), ({21: 0x16}, "1"),  # two zones, Z2 room
            ({21: 0x26}, "0"), ({21: 0x26}, "1"),  # two zones, Z2 pool
            ({21: 0x11}, "1"), ({21: 0x55}, "0"), ({21: 0xFF}, "1"), ({21: 0x25}, "0"),
            ({21: 0x14}, "0"), ({21: 0x1A}, "1"), ({21: 0x01}, "1"), ({21: 0x95}, "0"),
        ],
        ["2", "-1", "15", "25", "21"],
    ),
    # byte 28 bits 1-0 (heating) / 3-2 (cooling): b01 compensation curve, b10 direct; only this pair is sent,
    # in particular bits 7-6 (defrost signal, only SetDefrostSignal changes them); the pair must read b01 or b10
    "SetHeatingWaterMode": (
        28,
        [
            ({28: 0x95}, "1", 0x02),
            ({28: 0x96}, "0", 0x01),
            ({28: 0x95}, "0", 0x01),
            ({28: 0x99}, "1", 0x02),
            ({28: 0x55}, "1", 0x02),
            ({28: 0xD5}, "1", 0x02),
            ({28: 0x15}, "1", 0x02),
            ({28: 0x35}, "0", 0x01),
        ],
        [({28: 0x94}, "1"), ({28: 0x97}, "0"), ({28: 0x90}, "1"), ({28: 0xFF}, "0")],
        ["2", "-1"],
    ),
    "SetCoolingWaterMode": (
        28,
        [
            ({28: 0x95}, "1", 0x08),
            ({28: 0x99}, "0", 0x04),
            ({28: 0x95}, "0", 0x04),
            ({28: 0x96}, "1", 0x08),
            ({28: 0x55}, "1", 0x08),
            ({28: 0xD6}, "1", 0x08),
            ({28: 0x25}, "1", 0x08),
        ],
        [({28: 0x91}, "1"), ({28: 0x9D}, "0"), ({28: 0x93}, "1"), ({28: 0xFF}, "0")],
        ["2", "-1"],
    ),
    # byte 20 bits 7-6: b01 water, b10 glycol (inferred); only this pair is sent
    "SetLiquidType": (
        20,
        [
            ({20: 0x6A}, "1", 0x80),
            ({20: 0xAA}, "0", 0x40),
            ({20: 0x6A}, "0", 0x40),
            ({20: 0x7B}, "1", 0x80),
            ({20: 0xEA}, "1", 0x80),
            ({20: 0xEA}, "0", 0x40),
        ],
        [],
        ["2", "-1"],
    ),
    # byte 25 bits 1-0 (b01 internal, b10 external); needs a connected tank (byte 24 bits 1-0 = b10)
    "SetDHWHeaterType": (
        25,
        [
            ({24: 0x56, 25: 0x25}, "1", 0x02),
            ({24: 0x56, 25: 0x26}, "0", 0x01),
            ({24: 0x96, 25: 0x29}, "1", 0x02),
            ({24: 0x56, 25: 0x5E}, "0", 0x01),
            ({24: 0x56, 25: 0x5D}, "1", 0x02),
            ({24: 0x76, 25: 0x2D}, "1", 0x02),
        ],
        [
            ({24: 0x55, 25: 0x25}, "1"), ({24: 0x55, 25: 0x26}, "0"),  # the pump discards the value without a tank
            ({24: 0x00, 25: 0x25}, "1"), ({24: 0x57, 25: 0x25}, "1"), ({24: 0x54, 25: 0x25}, "1"),
        ],
        ["2", "-1"],
        {"gate": 25},
    ),
    # byte 24 bits 1-0 (b01 no tank, b10 tank); only this pair is sent; "no" is refused while the extra pump is on DHW
    "SetDHWInstalled": (
        24,
        [
            ({24: 0x55}, "1", 0x02),
            ({24: 0x56}, "0", 0x01),
            ({24: 0x96}, "0", 0x01),
            ({24: 0x95}, "1", 0x02),
            ({24: 0x75}, "1", 0x02),
            ({24: 0x56, 31: 0x55}, "0", 0x01),
            ({24: 0x56, 31: 0x5A}, "0", 0x01),
            ({24: 0x56, 31: 0x5B}, "1", 0x02),
            ({24: 0x56, 31: 0x57}, "1", 0x02),
        ],
        [
            ({24: 0x56, 31: 0x5B}, "0"), ({24: 0x56, 31: 0x57}, "0"),  # extra pump on DHW needs the tank
            ({24: 0x57}, "1"), ({24: 0x54}, "0"), ({24: 0xFC}, "1"),
        ],
        ["2", "-1"],
    ),
    # byte 24 bits 5-4 (b01 disabled, b10 buffer, b11 DHW); only this pair is sent
    "SetSolarMode": (
        24,
        [
            ({24: 0x55}, "2", 0x30),
            ({24: 0x55}, "1", 0x20),
            ({24: 0x75}, "0", 0x10),
            ({24: 0x65}, "0", 0x10),
            ({24: 0x96}, "2", 0x30),
            ({24: 0x56}, "1", 0x20),
        ],
        [],
        ["3", "-1"],
    ),
    # byte 27 bits 3-2 (b01 off, b10 capacity, b11 power consumption); only this pair is sent; switching it on is refused
    # only while bivalent control is on (byte 26 bits 1-0 = b10) and the bivalent operating mode (bits 5-4) is SG ready (b10)
    "SetSGReadyMode": (
        27,
        [
            ({27: 0x55}, "1", 0x08),
            ({27: 0x55}, "2", 0x0C),
            ({27: 0x5D}, "0", 0x04),
            ({27: 0x59}, "2", 0x0C),
            ({27: 0x59}, "0", 0x04),
            ({27: 0x96}, "1", 0x08),
            ({27: 0x56}, "2", 0x0C),
            ({27: 0x69}, "0", 0x04),  # switching off is always possible
            ({27: 0x45}, "1", 0x08),  # bivalent operating mode not set (b00) is no conflict
            ({27: 0x55}, "0", 0x04),
            # the conflict exists only with bivalent control = yes (byte 26 bits 1-0 = b10) and bivalent operating mode SG ready (b10)
            ({26: 0x5D, 27: 0x65}, "1", 0x08),  # bivalent control off, hidden b10
            ({26: 0x5D, 27: 0x65}, "2", 0x0C),
            ({26: 0x5C, 27: 0xA5}, "1", 0x08),
            ({26: 0x5F, 27: 0x65}, "1", 0x08),
            ({26: 0x5E, 27: 0x75}, "1", 0x08),  # bivalent "intelligent" is no conflict
            ({26: 0x5E, 27: 0x75}, "2", 0x0C),
            ({26: 0x5D, 27: 0x75}, "2", 0x0C),
            ({26: 0x5E, 27: 0x69}, "0", 0x04),  # off is possible in every state
            ({26: 0x5E, 27: 0x6D}, "0", 0x04),
            ({26: 0x5E, 27: 0x65}, "0", 0x04),
        ],
        [({26: 0x5E, 27: 0x65}, "1"), ({26: 0x5E, 27: 0x65}, "2"), ({26: 0x5E, 27: 0xA5}, "1"), ({26: 0x56, 27: 0x65}, "1")],
        ["3", "-1"],
    ),
    # byte 36 nibbles (like byte 35): high nibble = offset of the return sensor, K / 0.5 + 8; the other nibble is sent as 0
    "SetSensorOffsetReturn": (
        36,
        [
            ({}, "1", 0xA0),
            ({}, "0.5", 0x90),
            ({}, "0", 0x80),
            ({}, "-3", 0x20),
            ({}, "3.0", 0xE0),
            ({}, "-0.5", 0x70),
            ({}, "-2", 0x40),
            ({36: 0x8E}, "1", 0xA0),
            ({36: 0x82}, "-3", 0x20),
        ],
        [],
        ["-3.5", "3.5", "4", "-4", "0.25", "1.", ".5", "1.55", "10"],
        {"skip": {"1.0"}},
    ),
    # byte 36 low nibble = offset of the DHW tank sensor, byte 37 low nibble = offset of the flow sensor (same encoding);
    # the other nibble is sent as 0 (no change)
    "SetSensorOffsetDhwTank": (
        36,
        [
            ({}, "1", 0x0A),
            ({}, "0.5", 0x09),
            ({}, "0", 0x08),
            ({}, "-3", 0x02),
            ({}, "3.0", 0x0E),
            ({}, "-0.5", 0x07),
            ({}, "-2", 0x04),
            ({36: 0xE8}, "1", 0x0A),
            ({36: 0x28}, "-3", 0x02),
        ],
        [],
        ["-3.5", "3.5", "4", "-4", "0.25", "1.", ".5", "1.55", "10"],
        {"skip": {"1.0"}},
    ),
    "SetSensorOffsetFlow": (
        37,
        [
            ({}, "1", 0x0A),
            ({}, "0.5", 0x09),
            ({}, "0", 0x08),
            ({}, "-3", 0x02),
            ({}, "3.0", 0x0E),
            ({}, "-0.5", 0x07),
            ({}, "-2", 0x04),
            ({37: 0xE8}, "1", 0x0A),
            ({37: 0x28}, "-3", 0x02),
        ],
        [],
        ["-3.5", "3.5", "4", "-4", "0.25", "1.", ".5", "1.55", "10"],
        {"skip": {"1.0"}},
    ),
    # byte 22 low nibble = zone 1 sensor type, payload 0..3 -> nibble 1..4; the zone 2 nibble is sent as 0
    "SetZ1SensorSettings": (
        22,
        [
            ({22: 0x11}, "2", 0x03),
            ({22: 0x13}, "0", 0x01),
            ({22: 0x11}, "1", 0x02),
            ({22: 0x11}, "3", 0x04),
            ({22: 0x21}, "2", 0x03),
            ({22: 0x41}, "0", 0x01),
        ],
        [],
        ["4", "-1"],
    ),
    # byte 28 bits 7-6 = defrost signal (b01 no, b10 yes); only this pair is sent.
    # No check against bivalent control: the heat pump resolves that conflict itself
    "SetDefrostSignal": (
        28,
        [
            ({28: 0x95}, "0", 0x40),
            ({28: 0x95}, "1", 0x80),
            ({28: 0x55}, "1", 0x80),
            ({28: 0x55}, "0", 0x40),
            ({28: 0x96}, "0", 0x40),
            ({28: 0x96}, "1", 0x80),
            ({28: 0x99}, "0", 0x40),
            ({28: 0x99}, "1", 0x80),
            ({28: 0x5A}, "1", 0x80),
            ({28: 0x35}, "1", 0x80),
            ({28: 0xD5}, "0", 0x40),
            ({28: 0x15}, "1", 0x80),
        ],
        [],
        ["2", "-1"],
    ),
}
# SG ready limits: bytes 51..57 are (kW * 10 + 1) and need SG ready = power consumption (byte 27 bits 3-2 = b11);
# bytes 102 / 103 are degC + 128 and need SG ready on (b10 or b11)
for _name, _byte in (
    ("SetSGReadyDHWPower1", 51), ("SetSGReadyHeatPower1", 52), ("SetSGReadyCoolPower1", 53),
    ("SetSGReadyDHWPower2", 54), ("SetSGReadyHeatPower2", 55), ("SetSGReadyCoolPower2", 56),
    ("SetSGReadyStopPower", 57),
):
    RMW_SETTERS[_name] = (
        _byte,
        [
            ({27: 0x5D}, "4.2", 43),
            ({27: 0x5D}, "4", 41),
            ({27: 0x5D}, "0.5", 6),
            ({27: 0x5D}, "0.6", 7),
            ({27: 0x5D}, "9.9", 100),
            ({27: 0x5D}, "10", 101),
            ({27: 0x5D}, "10.0", 101),
            ({27: 0x5D}, "1.0", 11),
            ({27: 0x9D}, "3.6", 37),
        ],
        [
            ({27: 0x55}, "4.2"), ({27: 0x59}, "4.2"), ({27: 0x51}, "4"), ({27: 0xD5}, "4.2"), ({27: 0x00}, "4.2"),
        ],
        ["0", "0.4", "0.0", "10.1", "11", "100", "0.55", "4.", ".5", "-1", "-0.5", "4,2", "4.2.1", "1e1", "4.25", "010.5"],
        {"gate": 27, "extra_gates": (_byte,), "skip": {"1.0"}},
    )
for _name, _byte in (("SetSGReadyCoolCapacity1", 102), ("SetSGReadyCoolCapacity2", 103)):
    RMW_SETTERS[_name] = (
        _byte,
        [
            ({27: 0x5D}, "-3", 125),
            ({27: 0x59}, "-6", 122),
            ({27: 0x5D}, "0", 128),
            ({27: 0x5D}, "-15", 113),
            ({27: 0x59}, "-1", 127),
        ],
        [({27: 0x55}, "-3"), ({27: 0x51}, "-3"), ({27: 0x45}, "0")],
        ["-16", "1", "15", "10", "128", "-128", "255"],
        {"gate": 27, "extra_gates": (_byte,)},
    )
# commands that must not exist: the pump ignores an anti-stick write, bytes above 109 cannot be written, concrete drying
REMOVED_COMMANDS = ("SetAntiStickMode", "SetAntiStick", "SetDryConcrete", "SetConcreteDrying", "SetBackupHeaterEnable", "SetCoolModeEnable")

SHIM = r"""
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
using byte = unsigned char;
#define PROGMEM
#define memcpy_P memcpy
#define snprintf_P snprintf
#define PSTR(x) x
struct String {
  long v;
  String(const char *s) : v(s ? atol(s) : 0) {}
  long toInt() const { return v; }
};
std::vector<byte> sent;
int send_count;
bool send_command(byte* data, int length) {
  if (length <= 0) return false;
  ++send_count; sent.assign(data, data + length);
  return true;
}
void log_message(char*) {}
"""


# last data block from the heat pump, used by the shared-byte setters for the unchanged other bits
ACT_DATA = """
char actData[DATASIZE] = { 0x71, (char)0xc8, 0x01, 0x10 };
void reset_act_data() {
  memset(actData + 4, 0, sizeof(actData) - 4);
  actData[0] = 0x71; actData[1] = (char)0xc8; actData[2] = 0x01; actData[3] = 0x10;
  actData[20] = 0x6A; actData[30] = 0x5A; actData[83] = 0x98;
  actData[24] = 0x55; actData[25] = 0x25; actData[26] = 0x5D; actData[27] = 0x55; actData[31] = 0x5A;
  actData[33] = 0x75; actData[34] = (char)0xA7; actData[35] = (char)0x86;
  actData[21] = 0x15; actData[22] = 0x11; actData[28] = (char)0x95; actData[36] = (char)0x88; actData[37] = (char)0x88;
  // SG ready limits (51..57), solar high limit (64), SG ready cooling offsets (102 / 103): the reference unit reports them
  actData[51] = 0x2B; actData[52] = 0x2B; actData[53] = 0x29; actData[54] = 0x29; actData[55] = 0x29; actData[56] = 0x29;
  actData[57] = 0x2B; actData[64] = (char)0xD0; actData[102] = 0x7E; actData[103] = 0x7C;
}
struct ActDataInit {
  ActDataInit() { reset_act_data(); }
} actDataInit;
"""
RUN_HELPER = r"""
static bool run_command(const char *topicName, const char *payloadText) {
  char topic[64], payload[64];
  strcpy(topic, topicName); strcpy(payload, payloadText);
  sent.clear(); send_count = 0;
  send_heatpump_command(topic, payload, send_command, log_message, false);
  return send_count == 1;
}
// the web console (HeishaMon.ino) and the rules engine (rules.cpp) do not use send_heatpump_command():
// they look the setter up in commands[] and call send_command(cmd, len) themselves
static bool run_direct(const char *name, const char *payloadText) {
  char payload[64];
  strcpy(payload, payloadText);
  sent.clear(); send_count = 0;
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    cmdStruct tmp;
    memcpy_P(&tmp, &commands[i], sizeof(tmp));
    if (strcmp(name, tmp.name) == 0) {
      byte cmd[256] = { 0 };
      char log[256] = { 0 };
      unsigned int len = tmp.func(payload, cmd, log);
      send_command(cmd, len);
    }
  }
  return send_count == 1;
}
// a main data block arrives: decode_heatpump_data() replaces actData
static void deliver_block(int a0 = -1, int v0 = 0, int a1 = -1, int v1 = 0) {
  if (a0 >= 0) actData[a0] = (char)v0;
  if (a1 >= 0) actData[a1] = (char)v1;
}
"""
GENERIC_REJECTED = ["", "abc", "1x", "1.0", " 1", "+1", "1 ", "-", "999999999999999"]


def main():
    header = (ROOT / "HeishaMon/commands.h").read_text()
    source = (ROOT / "HeishaMon/commands.cpp").read_text()
    docs = (ROOT / "MQTT-Topics.md").read_text()
    additions = source[
        source.index("static unsigned int set_integer_field") : source.index(
            "unsigned int set_sterilization_temp"
        )
    ]
    def upstream(name):
        i = source.index("unsigned int " + name + "(char *msg")
        return source[i : source.index("\n}\n", i) + 3]

    # the existing upstream setters that write the same shared bytes (20, 24, 25, 26, 30)
    additions += "".join(
        upstream(n)
        for n in (
            "set_bivalent_control",
            "set_bivalent_mode",
            "set_alt_external_sensor",
            "set_external_pad_heater",
            "set_heatingcontrol",
            "set_smart_dhw",
        )
    )
    real_functions = set(re.findall(r"unsigned int (\w+)\(", additions))
    stubs = []
    for name, args in re.findall(r"unsigned int (set_\w+)\(([^;]+)\);", header):
        if name not in real_functions:
            stubs.append(f"unsigned int {name}({args}) {{ return 0; }}")
    template = re.search(r"byte panasonicSendQuery\[\].*?\};", source, re.S)[0]
    dispatcher = source[
        source.index("void send_heatpump_command(") : source.index(
            "bool saveOptionalPCB("
        )
    ]
    header = re.sub(r"^#include.*$", "", header, flags=re.M)
    checks = []
    cases = 0
    for topic, (address, accepted, rejected, *skip) in SETTERS.items():
        assert f"| {topic} |" in docs
        function = re.search(r'\{\s*"' + topic + r'",\s*(\w+)\s*\}', header)[1]
        for payload, encoded in accepted:
            checks.append(
                f"""
  {{ char topic[] = "{topic}", payload[] = "{payload}";
    sent.clear(); send_count = 0;
    send_heatpump_command(topic, payload, send_command, log_message, false);
    assert(send_count == 1 && sent.size() == sizeof(panasonicSendQuery));
    for (unsigned int i = 0; i < sent.size(); ++i)
      assert(sent[i] == (i == {address} ? {encoded} : panasonicSendQuery[i]));
  }}
"""
            )
            cases += 1
        for payload in rejected + [
            p for p in GENERIC_REJECTED if p not in (skip[0] if skip else ())
        ]:
            checks.append(
                f"""
  {{ char topic[] = "{topic}", payload[] = "{payload}";
    sent.clear(); send_count = 0;
    send_heatpump_command(topic, payload, send_command, log_message, false);
    assert(send_count == 0 && sent.empty());
    byte command[256]; memset(command, 0xa5, sizeof(command));
    char log[256];
    assert({function}(payload, command, log) == 0);
    for (byte value : command) assert(value == 0xa5);
  }}
"""
            )
            cases += 1
        checks.append(
            f"""
  {{ byte command[256]; memset(command, 0xa5, sizeof(command)); char log[256];
    assert({function}(nullptr, command, log) == 0);
    for (byte value : command) assert(value == 0xa5);
  }}
"""
        )
        cases += 1
    def setup(overrides):
        return "reset_act_data(); " + " ".join(
            f"actData[{b}] = (char){v};" for b, v in overrides.items()
        )

    for topic, (address, accepted, blocked, rejected, *options) in RMW_SETTERS.items():
        options = options[0] if options else {}
        gate = options.get("gate", address)
        skip_generic = options.get("skip", set())
        assert f"| {topic} |" in docs
        function = re.search(r'\{\s*"' + topic + r'",\s*(\w+)\s*\}', header)[1]
        for overrides, payload, encoded in accepted:
            checks.append(
                f"""
  {{ char topic[] = "{topic}", payload[] = "{payload}";
    {setup(overrides)}
    sent.clear(); send_count = 0;
    send_heatpump_command(topic, payload, send_command, log_message, false);
    assert(send_count == 1 && sent.size() == sizeof(panasonicSendQuery));
    for (unsigned int i = 0; i < sent.size(); ++i)
      assert(sent[i] == (i == {address} ? {encoded} : panasonicSendQuery[i]));
    reset_act_data();
  }}
"""
            )
            cases += 1
        for overrides, payload in blocked:
            checks.append(
                f"""
  {{ char topic[] = "{topic}", payload[] = "{payload}";
    {setup(overrides)}
    sent.clear(); send_count = 0;
    send_heatpump_command(topic, payload, send_command, log_message, false);
    assert(send_count == 0 && sent.empty());
    byte command[256]; memset(command, 0xa5, sizeof(command));
    char log[256];
    assert({function}(payload, command, log) == 0);
    for (byte value : command) assert(value == 0xa5);
    reset_act_data();
  }}
"""
            )
            cases += 1
        valid_state = accepted[0][0]
        for payload in rejected + [p for p in GENERIC_REJECTED if p not in skip_generic]:
            checks.append(
                f"""
  {{ char topic[] = "{topic}", payload[] = "{payload}";
    {setup(valid_state)}
    sent.clear(); send_count = 0;
    send_heatpump_command(topic, payload, send_command, log_message, false);
    assert(send_count == 0 && sent.empty());
    byte command[256]; memset(command, 0xa5, sizeof(command));
    char log[256];
    assert({function}(payload, command, log) == 0);
    for (byte value : command) assert(value == 0xa5);
    reset_act_data();
  }}
"""
            )
            cases += 1
        checks.append(
            f"""
  {{ byte command[256]; memset(command, 0xa5, sizeof(command)); char log[256];
    {setup(valid_state)}
    assert({function}(nullptr, command, log) == 0);
    for (byte value : command) assert(value == 0xa5);
    reset_act_data();
  }}
"""
        )
        cases += 1
        # no data block yet, or the byte itself still 0: refuse instead of guessing the other bits
        payload = accepted[0][1]
        for broken in ("actData[0] = 0;", f"actData[{gate}] = 0;") + tuple(
            f"actData[{g}] = 0;" for g in options.get("extra_gates", ())
        ):
            checks.append(
                f"""
  {{ char topic[] = "{topic}", payload[] = "{payload}";
    {setup(valid_state)} {broken}
    sent.clear(); send_count = 0;
    send_heatpump_command(topic, payload, send_command, log_message, false);
    assert(send_count == 0 && sent.empty());
    byte command[256]; memset(command, 0xa5, sizeof(command));
    char log[256];
    assert({function}(payload, command, log) == 0);
    for (byte value : command) assert(value == 0xa5);
    reset_act_data();
  }}
"""
            )
            cases += 1
    # --- removed commands, numbering of Modbus IDs / SET numbers / register table -----------------
    modbus_map = (ROOT / "HeishaMon/ModbusRegisterMap.h").read_text()
    modbus_doc = (ROOT / "Modbus-Register-Mapping.md").read_text()
    for name in REMOVED_COMMANDS:
        for text in (header, docs, modbus_map, modbus_doc, source):
            assert name not in text, f"{name} must not appear in the draft"
        checks.append(f'  assert(!run_command("{name}", "0") && !run_command("{name}", "1"));\n')
        cases += 1
    ids = [(int(i), n) for i, n in re.findall(r'\{(\d+), "(\w+)", (?:true|false)\}', modbus_map)]
    new_ids = [(i, n) for i, n in ids if i >= 55 and i != 100]
    # ID 100 is reserved for SetReset (register 7000): the new IDs run 55..99, then 101 onwards
    assert [i for i, _ in new_ids] == [i for i in range(55, 55 + len(new_ids) + 1) if i != 100], "Modbus IDs must be contiguous"
    command_names = set(re.findall(r'\{\s*"(Set\w+)",\s*\w+\s*\}', header))
    assert {n for _, n in new_ids} <= command_names, "every Modbus entry needs a command"
    for i, name in new_ids:
        assert f"SET{i + 1} | {name} |" in docs, f"SET{i + 1} row missing or misnumbered for {name}"
        assert f"| {4999 + i} | `{name}` |" in modbus_doc, f"register row missing for {name}"
    assert len(re.findall(r"^SET(?:5[6-9]|[6-9]\d|\d{3}) \|", docs, re.M)) == len(new_ids)
    assert f"| {5000 + new_ids[-1][0]} |" not in modbus_doc, "no register row without a command"
    cases += 3 + len(new_ids)

    # --- sequences: commands in a row, checks against other settings ------------------------------
    sequences = r"""
  { // commands in a row on one byte: each sends only its own field, actData is never changed
    reset_act_data();
    char before[sizeof(actData)]; memcpy(before, actData, sizeof(before));
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x06);
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x80);
    assert(run_command("SetSensorOffsetOutdoor", "-1") && sent[35] == 0x06);
    assert(run_command("SetSensorOffsetRemoteTh1", "1") && sent[35] == 0xA0);
    assert(run_command("SetStaticPressure", "1") && sent[31] == 0x20);
    assert(run_command("SetExtraPump", "1") && sent[31] == 0x0A);
    assert(run_command("SetHeatingWaterMode", "1") && sent[28] == 0x02);
    assert(run_command("SetDefrostSignal", "0") && sent[28] == 0x40);
    assert(run_command("SetCoolingWaterMode", "0") && sent[28] == 0x04);
    assert(memcmp(before, actData, sizeof(before)) == 0);
    for (unsigned int i = 0; i < sent.size(); ++i) if (i != 28) assert(sent[i] == panasonicSendQuery[i]);
  }
  { // the web console and the rules engine call the setter directly: same frame
    reset_act_data();
    assert(run_direct("SetEfficiencyStage1Time", "25") && sent[33] == 0x06);
    assert(run_direct("SetExtraPump", "0") && sent[31] == 0x05);
  }
  { // checks against other settings use the last data block: a parent switched on a moment ago counts once it is read back
    reset_act_data();
    assert(!run_command("SetDHWHeaterType", "1"));
    assert(run_command("SetDHWInstalled", "1") && sent[24] == 0x02);
    assert(!run_command("SetDHWHeaterType", "1"));      // the data block still reports no tank
    deliver_block(24, 0x56);
    assert(run_command("SetDHWHeaterType", "1") && sent[25] == 0x02);
    // extra pump on DHW needs the tank; the tank cannot be removed while the data block reports the extra pump on DHW
    assert(run_command("SetExtraPump", "2") && sent[31] == 0x03);
    assert(run_command("SetDHWInstalled", "0") && sent[24] == 0x01); // still allowed: the block shows heating
    deliver_block(31, 0x5B);
    assert(!run_command("SetDHWInstalled", "0"));
    assert(run_command("SetExtraPumpMode", "0") && sent[31] == 0x04);
    assert(run_command("SetExtraPump", "1") && sent[31] == 0x0A);
    deliver_block(31, 0x5A);
    assert(run_command("SetDHWInstalled", "0") && sent[24] == 0x01);
    assert(!run_command("SetExtraPumpMode", "1"));      // needs the extra pump on DHW
    reset_act_data();
  }
  { // zone type: only the two one-zone values; a rejected or foreign state sends nothing
    reset_act_data();
    assert(run_command("SetZoneType", "1") && sent[21] == 0x08);
    assert(run_command("SetZoneType", "0") && sent[21] == 0x04);
    reset_act_data(); actData[21] = (char)0x16;
    assert(!run_command("SetZoneType", "0") && !run_command("SetZoneType", "1"));
    actData[21] = (char)0x26;
    assert(!run_command("SetZoneType", "0") && !run_command("SetZoneType", "1"));
    reset_act_data(); actData[0] = 0;
    assert(!run_command("SetZoneType", "1") && !run_command("SetZoneType", "0"));
    reset_act_data(); actData[21] = 0;
    assert(!run_command("SetZoneType", "1"));
    reset_act_data();
  }
  { // water modes: the pair that is set must read b01 or b10
    reset_act_data(); actData[28] = (char)0x51;
    assert(!run_command("SetCoolingWaterMode", "1"));   // bits 3-2 read b00
    assert(run_command("SetHeatingWaterMode", "1") && sent[28] == 0x02);
    actData[28] = (char)0x5F;
    assert(!run_command("SetHeatingWaterMode", "0") && !run_command("SetCoolingWaterMode", "0"));
    reset_act_data();
  }
  { // SG ready: the limits need the mode; SG ready and the bivalent operating mode exclude each other
    reset_act_data();
    assert(!run_command("SetSGReadyHeatPower1", "4.2") && !run_command("SetSGReadyCoolCapacity1", "-3"));
    assert(run_command("SetSGReadyMode", "2") && sent[27] == 0x0C);
    deliver_block(27, 0x5D);
    assert(run_command("SetSGReadyHeatPower1", "4.2") && sent[52] == 43);
    assert(run_command("SetSGReadyCoolCapacity1", "-3") && sent[102] == 125);
    deliver_block(27, 0x59);
    assert(!run_command("SetSGReadyHeatPower1", "4.2")); // capacity mode: no power limits
    assert(run_command("SetSGReadyCoolCapacity2", "-6") && sent[103] == 122);
    deliver_block(27, 0x55);
    assert(!run_command("SetSGReadyCoolCapacity2", "-6"));
    reset_act_data(); actData[26] = (char)0x5E; actData[27] = (char)0x65;
    assert(!run_command("SetSGReadyMode", "1") && !run_command("SetSGReadyMode", "2"));
    assert(run_command("SetSGReadyMode", "0") && sent[27] == 0x04);
    assert(run_command("SetBivalentOperatingMode", "0") && sent[27] == 0x10);
    reset_act_data(); actData[26] = (char)0x5E; actData[27] = (char)0x5D;
    assert(!run_command("SetBivalentOperatingMode", "1"));
    assert(run_command("SetBivalentOperatingMode", "2") && sent[27] == 0x30); // intelligent does not conflict
    reset_act_data(); actData[26] = (char)0x5D; actData[27] = (char)0x65;
    assert(run_command("SetSGReadyMode", "1") && sent[27] == 0x08);   // bivalent control off: hidden pattern does not matter
    assert(!run_command("SetBivalentOperatingMode", "1"));             // needs bivalent control = yes
    reset_act_data();
  }
  { // no data block, or the own byte 0: nothing is sent
    reset_act_data(); actData[0] = 0;
    assert(!run_command("SetEfficiencyStage3Time", "15") && !run_command("SetBivalentAPStartDelay", "35"));
    assert(!run_command("SetAntiFreezeMode", "0") && !run_command("SetDHWDefrost", "0"));
    assert(!run_command("SetSolarHighLimit", "80"));
    reset_act_data(); actData[34] = 0; actData[35] = 0; actData[33] = 0; actData[36] = 0; actData[37] = 0; actData[22] = 0;
    assert(!run_command("SetEfficiencyStage3Time", "15") && !run_command("SetSensorOffsetOutdoor", "1"));
    assert(!run_command("SetSensorOffsetRemoteTh1", "1") && !run_command("SetEfficiencyStage1Time", "25"));
    assert(!run_command("SetSensorOffsetDhwTank", "1") && !run_command("SetSensorOffsetFlow", "1"));
    assert(!run_command("SetZ1SensorSettings", "1"));
    reset_act_data(); actData[64] = 0;
    assert(!run_command("SetSolarHighLimit", "80"));
    reset_act_data(); actData[27] = (char)0x5D; actData[52] = 0; actData[102] = 0;
    assert(!run_command("SetSGReadyHeatPower1", "4.2") && !run_command("SetSGReadyCoolCapacity1", "-3"));
    assert(run_command("SetSGReadyDHWPower1", "4.2") && sent[51] == 43 && run_command("SetSGReadyCoolCapacity2", "-6") && sent[103] == 122);
    reset_act_data();
  }
"""
    checks.append(sequences)
    cases += 8

    # structure of the production code that the host harness cannot compile (HeishaMon.ino, rules.cpp)
    ino = (ROOT / "HeishaMon/HeishaMon.ino").read_text()
    rules = (ROOT / "HeishaMon/rules.cpp").read_text()
    # no pending state: the send path in HeishaMon.ino is untouched and the setters keep no memory
    for text in (ino, source, header):
        assert "rmw_" not in text and "effective_byte" not in text
    assert "xQueueSend(cmdQueue, &cmd, 0);" in ino
    # every writer (MQTT/Modbus via send_heatpump_command, web console, rules) ends in send_command(cmd, len)
    assert re.search(r"if \(len > 0\) send_command\(cmd, len\);", source)
    assert re.search(r"len = tmp\.func\(cpy, cmd, log_msg\);.*?send_command\(cmd, len\);", ino, re.S)
    assert re.search(r"tmp\.func\(payload, cmd, log_msg\);.*?send_command\(cmd, len\);", rules, re.S)
    assert "send_heatpump_command(topicName, payload, send_command" in (ROOT / "HeishaMon/HeishaModbusServer.cpp").read_text()
    cases += 6

    program = (
        SHIM
        + header
        + ACT_DATA
        + template
        + additions
        + "\n".join(stubs)
        + dispatcher
        + RUN_HELPER
    )
    program += "\nint main() {\n" + "".join(checks) + "\n}\n"
    # the firmware runs with a 32-bit unsigned long (ESP8266/ESP32); make the host build do the same
    program = "#include <cstdint>\n" + program.replace("unsigned long", "uint32_t")
    with tempfile.TemporaryDirectory() as directory:
        cpp = Path(directory) / "test.cpp"
        binary = Path(directory) / "test"
        cpp.write_text(program)
        subprocess.run(["g++", "-std=c++17", str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print(f"PASS: {cases} setter/dispatch cases (bytes, boundaries, rejected payloads)")


if __name__ == "__main__":
    main()
