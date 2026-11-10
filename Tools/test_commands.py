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
    # byte 20 bits 3-2 (b01 off, b10 on); the other bits stay as read
    "SetAntiFreezeMode": (
        20,
        [
            ({20: 0x6A}, "0", 0x66),
            ({20: 0x66}, "1", 0x6A),
            ({20: 0xEA}, "0", 0xE6),
            ({20: 0x2A}, "1", 0x2A),
        ],
        [],
        ["2", "-1"],
    ),
    # byte 27 bits 1-0 (b01 disabled, b10 enabled); SG ready bits 3-2, bivalent bits 7-4 stay as read
    "SetDemandControlEnable": (
        27,
        [
            ({27: 0x55}, "1", 0x56),
            ({27: 0x56}, "0", 0x55),
            ({27: 0x5D}, "1", 0x5E),
            ({27: 0x95}, "1", 0x96),
            ({27: 0xAA}, "0", 0xA9),
        ],
        [],
        ["2", "-1"],
    ),
    # byte 30 bits 1-0 (b01 no, b10 yes); the other bits stay as read
    "SetDHWDefrost": (
        30,
        [
            ({30: 0x5A}, "0", 0x59),
            ({30: 0x59}, "1", 0x5A),
            ({30: 0x5E}, "0", 0x5D),
            ({30: 0x1A}, "0", 0x19),
        ],
        [],
        ["2", "-1"],
    ),
    # byte 31 bits 5-4 (b01 no, b10 yes); bits 7-6 are sent as 0 (cloud frame 0x2A for read 0x6A), low nibble as read
    "SetStaticPressure": (
        31,
        [
            ({31: 0x5A}, "1", 0x2A),
            ({31: 0x6A}, "0", 0x1A),
            ({31: 0x6A}, "1", 0x2A),
            ({31: 0x5B}, "1", 0x2B),
            ({31: 0x57}, "0", 0x17),
            ({31: 0xDA}, "1", 0x2A),
            ({31: 0x55}, "0", 0x15),
        ],
        [],
        ["2", "-1"],
    ),
    # byte 33 low nibble, minutes / 5 + 1; the high nibble (stage 2) comes from the data block and must be 1..15
    "SetEfficiencyStage1Time": (
        33,
        [
            ({}, "25", 0x76),
            ({}, "20", 0x75),
            ({}, "0", 0x71),
            ({}, "60", 0x7D),
            ({33: 0x85}, "25", 0x86),
        ],
        [({33: 0x05}, "25"), ({33: 0x0F}, "20")],
        ["-5", "3", "65", "70", "75"],
    ),
    "SetEfficiencyStage2Time": (
        33,
        [
            ({}, "35", 0x85),
            ({}, "30", 0x75),
            ({}, "0", 0x15),
            ({}, "60", 0xD5),
            ({33: 0x76}, "35", 0x86),
        ],
        [({33: 0x70}, "35"), ({33: 0x70}, "0")],
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
    # byte 35 nibbles, K / 0.5 + 8; the other nibble comes from the data block and must be 2..14
    "SetSensorOffsetRemoteTh1": (
        35,
        [
            ({}, "1", 0xA6),
            ({}, "0.5", 0x96),
            ({}, "0", 0x86),
            ({}, "-3", 0x26),
            ({}, "3.0", 0xE6),
            ({}, "-0.5", 0x76),
            ({35: 0x8E}, "1", 0xAE),
            ({35: 0x82}, "-3", 0x22),
        ],
        [({35: 0x80}, "1"), ({35: 0x81}, "1"), ({35: 0x8F}, "1")],
        ["-3.5", "3.5", "4", "-4", "0.25", "1.", ".5", "1.55", "10"],
        {"skip": {"1.0"}},
    ),
    "SetSensorOffsetOutdoor": (
        35,
        [
            ({}, "-1", 0x86),
            ({}, "2", 0x8C),
            ({}, "0", 0x88),
            ({}, "-3", 0x82),
            ({}, "3", 0x8E),
            ({}, "0.5", 0x89),
            ({35: 0xE6}, "-1.5", 0xE5),
            ({35: 0x26}, "3", 0x2E),
        ],
        [({35: 0x06}, "1"), ({35: 0x16}, "1"), ({35: 0xF6}, "1")],
        ["-3.5", "3.5", "4", "-4", "0.25", "1.", ".5", "1.55", "10"],
        {"skip": {"1.0"}},
    ),
    # byte 26 bits 5-4 (b01 no, b10 yes); needs bits 3-2 = b11 (advanced parallel) and bivalent control = yes (bits 1-0 = b10)
    "SetBivalentAPHeat": (
        26,
        [
            ({26: 0x5E}, "1", 0x6E),
            ({26: 0x6E}, "0", 0x5E),
            ({26: 0xDE}, "1", 0xEE),
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
    # byte 27 bits 5-4 (b01 auto, b10 SG ready, b11 intelligent); bits 1-0, 3-2 and 7-6 stay as read; 1 and 2 need bivalent = yes,
    # 1 (SG ready) also SG ready (bits 3-2) off; 0 (auto) is always possible once byte 27 was read
    "SetBivalentOperatingMode": (
        27,
        [
            ({26: 0x5E, 27: 0x55}, "1", 0x65),
            ({26: 0x5E, 27: 0x55}, "2", 0x75),
            ({26: 0x5E, 27: 0x75}, "0", 0x55),
            ({26: 0x5E, 27: 0x95}, "1", 0xA5),
            ({26: 0x5E, 27: 0x59}, "0", 0x59),
            ({26: 0x5E, 27: 0x51}, "1", 0x61),  # SG ready not set (b00) is no conflict
            ({26: 0x5E, 27: 0x76}, "0", 0x56),
            ({26: 0x56, 27: 0x55}, "1", 0x65),
            # only the bivalent operating mode "SG ready" (b10) conflicts with SG ready; "intelligent" does not
            ({26: 0x5E, 27: 0x59}, "2", 0x79),
            ({26: 0x5E, 27: 0x5D}, "2", 0x7D),
            # "auto" is the way out and works with bivalent control off too (hidden SG ready / intelligent pattern)
            ({26: 0x5D, 27: 0x65}, "0", 0x55),
            ({26: 0x5D, 27: 0x75}, "0", 0x55),
            ({26: 0x5D, 27: 0x69}, "0", 0x59),
            ({26: 0x5C, 27: 0x65}, "0", 0x55),
            ({26: 0x00, 27: 0x65}, "0", 0x55),
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
            ({26: 0x56, 27: 0x55}, "1", 0x95),
            ({26: 0x56, 27: 0x95}, "0", 0x55),
            ({26: 0x56, 27: 0x5D}, "1", 0x9D),
            ({26: 0x56, 27: 0x65}, "1", 0xA5),
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
    # bits 5-4 stay as read, bits 7-6 are sent as 0 (cloud frames 0x1A/0x1B/0x17/0x15 for reads 0x5A/0x5B/0x57/0x55);
    # payload 2 (DHW) needs a connected tank (byte 24 bits 1-0 = b10); all payloads need one of the four observed
    # low nibbles 5, A, B, 7
    "SetExtraPump": (
        31,
        [
            ({24: 0x56, 31: 0x5A}, "2", 0x1B),
            ({24: 0x56, 31: 0x5B}, "0", 0x15),
            ({24: 0x56, 31: 0x57}, "0", 0x15),
            ({24: 0x56, 31: 0x55}, "1", 0x1A),
            ({24: 0x56, 31: 0x5B}, "1", 0x1A),
            ({24: 0x56, 31: 0x55}, "2", 0x1B),
            ({24: 0x56, 31: 0x57}, "2", 0x17),
            ({24: 0x56, 31: 0x5B}, "2", 0x1B),
            ({24: 0x96, 31: 0x6A}, "0", 0x25),
            ({24: 0x56, 31: 0x6B}, "1", 0x2A),
            ({24: 0x56, 31: 0xDA}, "2", 0x1B),
            ({24: 0x56, 31: 0x1A}, "0", 0x15),
            # "not used" and "heating" do not need a tank (the cloud saved them with tank connection = No)
            ({24: 0x55, 31: 0x5A}, "0", 0x15),
            ({24: 0x55, 31: 0x5A}, "1", 0x1A),
            ({24: 0x57, 31: 0x5A}, "1", 0x1A),
            ({24: 0x55, 31: 0x55}, "1", 0x1A),
            ({24: 0x00, 31: 0x5B}, "0", 0x15),
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
            ({24: 0x56, 31: 0x5B}, "0", 0x17),
            ({24: 0x56, 31: 0x57}, "1", 0x1B),
            ({24: 0x56, 31: 0x5B}, "1", 0x1B),
            ({24: 0x56, 31: 0x6B}, "0", 0x27),
            ({24: 0x56, 31: 0xDB}, "0", 0x17),
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
    # byte 25 bits 3-2 (b01 3 kW, b10 6 kW, b11 9 kW from the upstream decode table); bits 1-0 (tank heater type) and 7-4 stay as read
    "SetInternalHeaterCapacity": (
        25,
        [
            ({25: 0x25}, "6", 0x29),
            ({25: 0x29}, "3", 0x25),
            ({25: 0x26}, "6", 0x2A),
            ({25: 0x5E}, "3", 0x56),
            ({25: 0x25}, "9", 0x2D),
            ({25: 0x2D}, "3", 0x25),
            ({25: 0x29}, "9", 0x2D),
            ({25: 0x5E}, "9", 0x5E),
            ({25: 0xAA}, "9", 0xAE),
        ],
        [],
        ["0", "4", "5", "7", "8", "10", "12", "15", "-3", "-9"],
    ),
    # byte 21 zone configuration code: only 0x15 (one zone, Z1 room) <-> 0x19 (one zone, Z1 pool), nothing else is touched
    "SetZoneType": (
        21,
        [
            ({21: 0x15}, "1", 0x19),
            ({21: 0x19}, "0", 0x15),
            ({21: 0x15}, "0", 0x15),
            ({21: 0x19}, "1", 0x19),
        ],
        [
            ({21: 0x16}, "0"), ({21: 0x16}, "1"),  # two zones, Z2 room
            ({21: 0x26}, "0"), ({21: 0x26}, "1"),  # two zones, Z2 pool
            ({21: 0x11}, "1"), ({21: 0x55}, "0"), ({21: 0xFF}, "1"), ({21: 0x25}, "0"),
            ({21: 0x14}, "0"), ({21: 0x1A}, "1"), ({21: 0x01}, "1"), ({21: 0x95}, "0"),
        ],
        ["2", "-1", "15", "25", "21"],
    ),
    # byte 28 bits 1-0 (heating) / 3-2 (cooling): b01 compensation curve, b10 direct; bits 7-4 and the other pair stay as read,
    # in particular bits 7-6 (defrost signal, only SetDefrostSignal changes them); the pair must read b01 or b10
    "SetHeatingWaterMode": (
        28,
        [
            ({28: 0x95}, "1", 0x96),
            ({28: 0x96}, "0", 0x95),
            ({28: 0x95}, "0", 0x95),
            ({28: 0x99}, "1", 0x9A),
            ({28: 0x55}, "1", 0x56),
            ({28: 0xD5}, "1", 0xD6),
            ({28: 0x15}, "1", 0x16),
            ({28: 0x35}, "0", 0x35),
        ],
        [({28: 0x94}, "1"), ({28: 0x97}, "0"), ({28: 0x90}, "1"), ({28: 0xFF}, "0")],
        ["2", "-1"],
    ),
    "SetCoolingWaterMode": (
        28,
        [
            ({28: 0x95}, "1", 0x99),
            ({28: 0x99}, "0", 0x95),
            ({28: 0x95}, "0", 0x95),
            ({28: 0x96}, "1", 0x9A),
            ({28: 0x55}, "1", 0x59),
            ({28: 0xD6}, "1", 0xDA),
            ({28: 0x25}, "1", 0x29),
        ],
        [({28: 0x91}, "1"), ({28: 0x9D}, "0"), ({28: 0x93}, "1"), ({28: 0xFF}, "0")],
        ["2", "-1"],
    ),
    # byte 20 bits 7-6: b01 water, b10 glycol (inferred); bits 5-0 stay as read
    "SetLiquidType": (
        20,
        [
            ({20: 0x6A}, "1", 0xAA),
            ({20: 0xAA}, "0", 0x6A),
            ({20: 0x6A}, "0", 0x6A),
            ({20: 0x7B}, "1", 0xBB),
            ({20: 0xEA}, "1", 0xAA),
            ({20: 0xEA}, "0", 0x6A),
        ],
        [],
        ["2", "-1"],
    ),
    # byte 25 bits 1-0 (b01 internal, b10 external); needs a connected tank (byte 24 bits 1-0 = b10)
    "SetDHWHeaterType": (
        25,
        [
            ({24: 0x56, 25: 0x25}, "1", 0x26),
            ({24: 0x56, 25: 0x26}, "0", 0x25),
            ({24: 0x96, 25: 0x29}, "1", 0x2A),
            ({24: 0x56, 25: 0x5E}, "0", 0x5D),
            ({24: 0x56, 25: 0x5D}, "1", 0x5E),
            ({24: 0x76, 25: 0x2D}, "1", 0x2E),
        ],
        [
            ({24: 0x55, 25: 0x25}, "1"), ({24: 0x55, 25: 0x26}, "0"),  # the pump discards the value without a tank
            ({24: 0x00, 25: 0x25}, "1"), ({24: 0x57, 25: 0x25}, "1"), ({24: 0x54, 25: 0x25}, "1"),
        ],
        ["2", "-1"],
        {"gate": 25},
    ),
    # byte 24 bits 1-0 (b01 no tank, b10 tank); bits 7-2 stay as read (capacity, solar, buffer); "no" is refused while the extra pump is on DHW
    "SetDHWInstalled": (
        24,
        [
            ({24: 0x55}, "1", 0x56),
            ({24: 0x56}, "0", 0x55),
            ({24: 0x96}, "0", 0x95),
            ({24: 0x95}, "1", 0x96),
            ({24: 0x75}, "1", 0x76),
            ({24: 0x56, 31: 0x55}, "0", 0x55),
            ({24: 0x56, 31: 0x5A}, "0", 0x55),
            ({24: 0x56, 31: 0x5B}, "1", 0x56),
            ({24: 0x56, 31: 0x57}, "1", 0x56),
        ],
        [
            ({24: 0x56, 31: 0x5B}, "0"), ({24: 0x56, 31: 0x57}, "0"),  # extra pump on DHW needs the tank
            ({24: 0x57}, "1"), ({24: 0x54}, "0"), ({24: 0xFC}, "1"),
        ],
        ["2", "-1"],
    ),
    # byte 24 bits 5-4 (b01 disabled, b10 buffer, b11 DHW); bits 7-6 and 3-0 stay as read
    "SetSolarMode": (
        24,
        [
            ({24: 0x55}, "2", 0x75),
            ({24: 0x55}, "1", 0x65),
            ({24: 0x75}, "0", 0x55),
            ({24: 0x65}, "0", 0x55),
            ({24: 0x96}, "2", 0xB6),
            ({24: 0x56}, "1", 0x66),
        ],
        [],
        ["3", "-1"],
    ),
    # byte 27 bits 3-2 (b01 off, b10 capacity, b11 power consumption); bits 7-4 and 1-0 stay as read; switching it on is refused
    # only while bivalent control is on (byte 26 bits 1-0 = b10) and the bivalent operating mode (bits 5-4) is SG ready (b10)
    "SetSGReadyMode": (
        27,
        [
            ({27: 0x55}, "1", 0x59),
            ({27: 0x55}, "2", 0x5D),
            ({27: 0x5D}, "0", 0x55),
            ({27: 0x59}, "2", 0x5D),
            ({27: 0x59}, "0", 0x55),
            ({27: 0x96}, "1", 0x9A),
            ({27: 0x56}, "2", 0x5E),
            ({27: 0x69}, "0", 0x65),  # switching off is always possible
            ({27: 0x45}, "1", 0x49),  # bivalent operating mode not set (b00) is no conflict
            ({27: 0x55}, "0", 0x55),
            # the conflict exists only with bivalent control = yes (byte 26 bits 1-0 = b10) and bivalent operating mode SG ready (b10)
            ({26: 0x5D, 27: 0x65}, "1", 0x69),  # bivalent control off, hidden b10
            ({26: 0x5D, 27: 0x65}, "2", 0x6D),
            ({26: 0x5C, 27: 0xA5}, "1", 0xA9),
            ({26: 0x5F, 27: 0x65}, "1", 0x69),
            ({26: 0x5E, 27: 0x75}, "1", 0x79),  # bivalent "intelligent" is no conflict
            ({26: 0x5E, 27: 0x75}, "2", 0x7D),
            ({26: 0x5D, 27: 0x75}, "2", 0x7D),
            ({26: 0x5E, 27: 0x69}, "0", 0x65),  # off is possible in every state
            ({26: 0x5E, 27: 0x6D}, "0", 0x65),
            ({26: 0x5E, 27: 0x65}, "0", 0x65),
        ],
        [({26: 0x5E, 27: 0x65}, "1"), ({26: 0x5E, 27: 0x65}, "2"), ({26: 0x5E, 27: 0xA5}, "1"), ({26: 0x56, 27: 0x65}, "1")],
        ["3", "-1"],
    ),
    # byte 36 nibbles (like byte 35): high nibble = offset of the return sensor, K / 0.5 + 8; the other nibble must be 2..14
    "SetSensorOffsetReturn": (
        36,
        [
            ({}, "1", 0xA8),
            ({}, "0.5", 0x98),
            ({}, "0", 0x88),
            ({}, "-3", 0x28),
            ({}, "3.0", 0xE8),
            ({}, "-0.5", 0x78),
            ({}, "-2", 0x48),
            ({36: 0x8E}, "1", 0xAE),
            ({36: 0x82}, "-3", 0x22),
        ],
        [({36: 0x80}, "1"), ({36: 0x81}, "1"), ({36: 0x8F}, "1")],
        ["-3.5", "3.5", "4", "-4", "0.25", "1.", ".5", "1.55", "10"],
        {"skip": {"1.0"}},
    ),
    # byte 36 low nibble = offset of the DHW tank sensor, byte 37 low nibble = offset of the flow sensor (same encoding);
    # the other nibble must be 2..14 and is sent as read
    "SetSensorOffsetDhwTank": (
        36,
        [
            ({}, "1", 0x8A),
            ({}, "0.5", 0x89),
            ({}, "0", 0x88),
            ({}, "-3", 0x82),
            ({}, "3.0", 0x8E),
            ({}, "-0.5", 0x87),
            ({}, "-2", 0x84),
            ({36: 0xE8}, "1", 0xEA),
            ({36: 0x28}, "-3", 0x22),
        ],
        [({36: 0x08}, "1"), ({36: 0x18}, "1"), ({36: 0xF8}, "1")],
        ["-3.5", "3.5", "4", "-4", "0.25", "1.", ".5", "1.55", "10"],
        {"skip": {"1.0"}},
    ),
    "SetSensorOffsetFlow": (
        37,
        [
            ({}, "1", 0x8A),
            ({}, "0.5", 0x89),
            ({}, "0", 0x88),
            ({}, "-3", 0x82),
            ({}, "3.0", 0x8E),
            ({}, "-0.5", 0x87),
            ({}, "-2", 0x84),
            ({37: 0xE8}, "1", 0xEA),
            ({37: 0x28}, "-3", 0x22),
        ],
        [({37: 0x08}, "1"), ({37: 0x18}, "1"), ({37: 0xF8}, "1")],
        ["-3.5", "3.5", "4", "-4", "0.25", "1.", ".5", "1.55", "10"],
        {"skip": {"1.0"}},
    ),
    # byte 22 low nibble = zone 1 sensor type, payload 0..3 -> nibble 1..4; the zone 2 nibble must be 1..4
    "SetZ1SensorSettings": (
        22,
        [
            ({22: 0x11}, "2", 0x13),
            ({22: 0x13}, "0", 0x11),
            ({22: 0x11}, "1", 0x12),
            ({22: 0x11}, "3", 0x14),
            ({22: 0x21}, "2", 0x23),
            ({22: 0x41}, "0", 0x41),
        ],
        [({22: 0x01}, "1"), ({22: 0x0F}, "1"), ({22: 0x51}, "1"), ({22: 0xF1}, "1")],
        ["4", "-1"],
    ),
    # byte 28 bits 7-6 = defrost signal (b01 no, b10 yes); bits 5-0 (water temperature modes and the unassigned bits 5-4) stay as read.
    # No check against bivalent control: the heat pump resolves that conflict itself
    "SetDefrostSignal": (
        28,
        [
            ({28: 0x95}, "0", 0x55),
            ({28: 0x95}, "1", 0x95),
            ({28: 0x55}, "1", 0x95),
            ({28: 0x55}, "0", 0x55),
            ({28: 0x96}, "0", 0x56),
            ({28: 0x96}, "1", 0x96),
            ({28: 0x99}, "0", 0x59),
            ({28: 0x99}, "1", 0x99),
            ({28: 0x5A}, "1", 0x9A),
            ({28: 0x35}, "1", 0xB5),
            ({28: 0xD5}, "0", 0x55),
            ({28: 0x15}, "1", 0x95),
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
void rmw_note_command_sent(const unsigned char *cmd, int length);
void rmw_set_resending(bool resending);
void rmw_note_command_queued(const unsigned char *cmd, int length, bool ring);
std::vector<byte> sent;
int send_count;
// Models the hand-over in the two real send_command() versions (HeishaMon.ino): a command that is not
// handed over (listen-only, ESP32 queue full) is not reported to the pending-byte memory; a command
// that is buffered (ESP8266, answer of the previous command still pending) is, and reports false.
// queued models the ESP32 send_command(): every accepted frame goes into cmdQueue first.
bool listen_only = false, queue_full = false, buffered = false, queued = false;
bool send_command(byte* data, int length) {
  if (length <= 0) return false;
  if (listen_only || queue_full) return false;
  ++send_count; sent.assign(data, data + length);
  if (queued) rmw_note_command_queued(data, length, true);
  else if (buffered) rmw_note_command_queued(data, length, false);
  else rmw_note_command_sent(data, length);
  return !buffered;
}
void log_message(char*) {}
unsigned long fake_millis = 0;
unsigned long millis() { return fake_millis; }
"""


# last data block from the heat pump, used by the shared-byte setters for the unchanged other bits
ACT_DATA = """
char actData[DATASIZE] = { 0x71, (char)0xc8, 0x01, 0x10 };
void reset_act_data() {
  fake_millis += 100000; // everything remembered by earlier commands has expired
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
// a main data block arrives (readSerial(): decode_heatpump_data() replaces actData, then the hook)
static void deliver_block(unsigned long after_ms, int a0 = -1, int v0 = 0, int a1 = -1, int v1 = 0, unsigned int frames_waiting = 0) {
  fake_millis += after_ms;
  if (a0 >= 0) actData[a0] = (char)v0;
  if (a1 >= 0) actData[a1] = (char)v1;
  rmw_data_block_received(frames_waiting);
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

    # --- sequences (shadow of the bytes sent by shared-byte commands) -------------------------------
    sequences = r"""
  { // two nibble commands in a row on byte 33: the second one keeps the first
    reset_act_data();
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x86);
    assert(run_command("SetEfficiencyStage1Time", "30") && sent[33] == 0x87);
    assert(actData[33] == 0x75); // actData itself is never changed
  }
  { // the memory expires after 12 s: back to the data block
    reset_act_data();
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    fake_millis += 11999;
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x86);
    reset_act_data();
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    fake_millis += 12000;
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
  }
  { // bivalent operating mode, then external pump (both byte 27)
    reset_act_data(); actData[26] = (char)0x56;
    assert(run_command("SetBivalentOperatingMode", "1") && sent[27] == 0x65);
    assert(run_command("SetBivalentExternalPump", "1") && sent[27] == 0xA5);
    assert(run_command("SetDemandControlEnable", "1") && sent[27] == 0xA6);
    assert(run_command("SetBivalentOperatingMode", "0") && sent[27] == 0x96);
  }
  { // both sensor offsets (byte 35)
    reset_act_data();
    assert(run_command("SetSensorOffsetRemoteTh1", "1") && sent[35] == 0xA6);
    assert(run_command("SetSensorOffsetOutdoor", "-1.5") && sent[35] == 0xA5);
    assert(actData[35] == (char)0x86);
  }
  { // byte 31: extra pump, static pressure and extra pump mode in a row, bits 7-6 always 0
    reset_act_data(); actData[24] = (char)0x56; actData[31] = (char)0x5A;
    assert(run_command("SetExtraPump", "2") && sent[31] == 0x1B);
    assert(run_command("SetStaticPressure", "1") && sent[31] == 0x2B);
    assert(run_command("SetExtraPumpMode", "0") && sent[31] == 0x27);
    assert(run_command("SetStaticPressure", "0") && sent[31] == 0x17);
  }
  { // the remembered byte is also used for the state checks of other commands
    reset_act_data(); actData[24] = (char)0x56; actData[31] = (char)0x5B;
    assert(run_command("SetExtraPump", "0") && sent[31] == 0x15);
    assert(!run_command("SetExtraPumpMode", "0")); // extra pump is not on DHW any more, although actData still says so
    reset_act_data(); actData[26] = (char)0x5E; actData[27] = (char)0x55;
    assert(run_command("SetBivalentAPHeat", "1") && sent[26] == 0x6E);
    assert(run_command("SetBivalentAPStartDelay", "35") && sent[67] == 36);
  }
  { // a rejected command, a setter call that only validates (Modbus) and a command for another byte leave no memory
    reset_act_data();
    assert(!run_command("SetEfficiencyStage1Time", "3"));
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
    reset_act_data();
    { byte command[256]; char log[256]; char payload[] = "25";
      assert(set_efficiency_stage1_time(payload, command, log) > 0 && command[33] == 0x76); }
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
    reset_act_data();
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    assert(run_command("SetSensorOffsetRemoteTh1", "1") && sent[35] == 0xA6);
    assert(run_command("SetHeatingOnDelay", "60") && sent[109] == 3);
    assert(run_command("SetEfficiencyStage3Time", "15") && sent[34] == 0x04);
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x86);
  }
  { // SetHeatingOnOutdoorTemp: below the heating OFF temperature while the data block reports it, otherwise only 5..127
    reset_act_data();
    assert(run_command("SetHeatingOnOutdoorTemp", "23") && sent[108] == 151);
    assert(!run_command("SetHeatingOnOutdoorTemp", "24"));
    actData[83] = 0; // not reported
    assert(run_command("SetHeatingOnOutdoorTemp", "30") && sent[108] == 158);
    assert(run_command("SetHeatingOnOutdoorTemp", "34") && sent[108] == 162); // highest value the menu can offer (OFF max 35 - 1)
    assert(!run_command("SetHeatingOnOutdoorTemp", "35"));
    reset_act_data(); actData[0] = 0; // no data block
    assert(run_command("SetHeatingOnOutdoorTemp", "30") && sent[108] == 158);
    assert(run_command("SetHeatingOnOutdoorTemp", "34") && sent[108] == 162);
    assert(!run_command("SetHeatingOnOutdoorTemp", "35") && !run_command("SetHeatingOnOutdoorTemp", "127"));
    assert(!run_command("SetHeatingOnOutdoorTemp", "4"));
  }
  { // zone type: only the two one-zone values, two commands in a row, a rejected or foreign state sends nothing
    reset_act_data();
    assert(run_command("SetZoneType", "1") && sent[21] == 0x19);
    assert(run_command("SetZoneType", "0") && sent[21] == 0x15);
    assert(actData[21] == 0x15);
    reset_act_data(); actData[21] = (char)0x16;
    assert(!run_command("SetZoneType", "0") && !run_command("SetZoneType", "1"));
    actData[21] = (char)0x26;
    assert(!run_command("SetZoneType", "0") && !run_command("SetZoneType", "1"));
    reset_act_data(); actData[0] = 0;
    assert(!run_command("SetZoneType", "1") && !run_command("SetZoneType", "0"));
    reset_act_data(); actData[21] = 0;
    assert(!run_command("SetZoneType", "1"));
    // a raw/cloud frame that sets two zones (0x16) is remembered: the command refuses even before the read-back
    reset_act_data();
    { byte frame[sizeof(panasonicSendQuery)]; memcpy(frame, panasonicSendQuery, sizeof(frame)); frame[21] = 0x16;
      send_command(frame, sizeof(frame)); }
    assert(!run_command("SetZoneType", "1"));
    reset_act_data();
  }
  { // byte 28: the water mode commands never change bits 7-6 (and 5-4, and the other pair), for every byte value they accept
    for (int value = 1; value < 256; ++value) {
      for (int cooling = 0; cooling < 2; ++cooling) {
        for (int payload = 0; payload < 2; ++payload) {
          reset_act_data(); actData[28] = (char)value;
          const int shift = cooling ? 2 : 0;
          const int pair = (value >> shift) & 3;
          char text[2] = { (char)('0' + payload), 0 };
          const bool ok = run_command(cooling ? "SetCoolingWaterMode" : "SetHeatingWaterMode", text);
          assert(ok == (pair == 1 || pair == 2));
          if (ok) {
            assert(sent[28] != 0);
            assert((sent[28] & ~(3 << shift)) == (value & ~(3 << shift)));
            assert(((sent[28] >> shift) & 3) == payload + 1);
            assert((sent[28] & 0xC0) == (value & 0xC0));
            for (unsigned int i = 0; i < sent.size(); ++i) if (i != 28) assert(sent[i] == panasonicSendQuery[i]);
          }
        }
      }
    }
    reset_act_data();
    assert(run_command("SetHeatingWaterMode", "1") && sent[28] == 0x96);
    assert(run_command("SetCoolingWaterMode", "1") && sent[28] == 0x9A);
    assert(run_command("SetHeatingWaterMode", "0") && sent[28] == 0x99);
    reset_act_data();
  }
  { // byte 28: SetDefrostSignal changes exactly bits 7-6, for every byte value it accepts (payload 0 -> b01, 1 -> b10)
    for (int value = 1; value < 256; ++value) {
      for (int payload = 0; payload < 2; ++payload) {
        reset_act_data(); actData[28] = (char)value;
        char text[2] = { (char)('0' + payload), 0 };
        assert(run_command("SetDefrostSignal", text));
        assert((sent[28] & 0x3F) == (value & 0x3F));
        assert(((sent[28] >> 6) & 3) == payload + 1);
        for (unsigned int i = 0; i < sent.size(); ++i) if (i != 28) assert(sent[i] == panasonicSendQuery[i]);
      }
    }
    reset_act_data(); actData[28] = 0;
    assert(!run_command("SetDefrostSignal", "1") && !run_command("SetDefrostSignal", "0"));
    reset_act_data(); actData[0] = 0;
    assert(!run_command("SetDefrostSignal", "1") && !run_command("SetDefrostSignal", "0"));
    // in a row with the water modes through the pending byte, in both orders; actData itself is never changed
    reset_act_data();
    assert(run_command("SetDefrostSignal", "0") && sent[28] == 0x55);
    assert(run_command("SetHeatingWaterMode", "1") && sent[28] == 0x56);
    assert(run_command("SetCoolingWaterMode", "1") && sent[28] == 0x5A);
    assert(run_command("SetDefrostSignal", "1") && sent[28] == 0x9A);
    assert(run_command("SetHeatingWaterMode", "0") && sent[28] == 0x99);
    assert(actData[28] == (char)0x95);
    // no check against bivalent control (byte 26 bits 1-0 = b10): the heat pump resolves the conflict itself
    reset_act_data(); actData[26] = (char)0x5E;
    assert(run_command("SetDefrostSignal", "1") && sent[28] == 0x95 && sent[26] == panasonicSendQuery[26]);
    assert(run_command("SetDefrostSignal", "0") && sent[28] == 0x55);
    reset_act_data(); actData[28] = (char)0x55;
    assert(run_command("SetDefrostSignal", "1") && sent[28] == 0x95);
    reset_act_data();
  }
  { // tank connection, then the tank heater type that needs it; a rejected tank command leaves the heater type blocked
    reset_act_data();
    assert(!run_command("SetDHWHeaterType", "1"));
    assert(run_command("SetDHWInstalled", "1") && sent[24] == 0x56);
    assert(run_command("SetDHWHeaterType", "1") && sent[25] == 0x26);
    assert(run_command("SetDHWInstalled", "0") && sent[24] == 0x55);
    assert(!run_command("SetDHWHeaterType", "0"));
    reset_act_data();
    assert(run_command("SetDHWInstalled", "1") && sent[24] == 0x56);
    deliver_block(2000, 24, 0x55); // the pump did not apply it: the data block is the truth again
    assert(!run_command("SetDHWHeaterType", "1"));
    // capacity bits 7-6 stay when the tank flag changes, also through a pending byte
    reset_act_data(); actData[24] = (char)0x96;
    assert(run_command("SetDHWInstalled", "0") && sent[24] == 0x95);
    assert(run_command("SetDHWInstalled", "1") && sent[24] == 0x96);
    assert(run_command("SetSolarMode", "2") && sent[24] == 0xB6);
    // the tank cannot be removed while the extra pump is on DHW, also when the extra pump command is still pending
    reset_act_data(); actData[24] = (char)0x56; actData[31] = (char)0x5A;
    assert(run_command("SetExtraPump", "2") && sent[31] == 0x1B);
    assert(!run_command("SetDHWInstalled", "0"));
    assert(run_command("SetExtraPump", "1") && sent[31] == 0x1A);
    assert(run_command("SetDHWInstalled", "0") && sent[24] == 0x55);
    reset_act_data();
  }
  { // SG ready: the limits need the mode; SG ready and the bivalent operating mode exclude each other
    reset_act_data();
    assert(!run_command("SetSGReadyHeatPower1", "4.2") && !run_command("SetSGReadyCoolCapacity1", "-3"));
    assert(run_command("SetSGReadyMode", "2") && sent[27] == 0x5D);
    assert(run_command("SetSGReadyHeatPower1", "4.2") && sent[52] == 43); // uses the pending byte 27, not the data block
    assert(run_command("SetSGReadyCoolCapacity1", "-3") && sent[102] == 125);
    assert(run_command("SetSGReadyMode", "1") && sent[27] == 0x59);
    assert(!run_command("SetSGReadyHeatPower1", "4.2")); // capacity mode: no power limits
    assert(run_command("SetSGReadyCoolCapacity2", "-6") && sent[103] == 122);
    assert(run_command("SetSGReadyMode", "0") && sent[27] == 0x55);
    assert(!run_command("SetSGReadyCoolCapacity2", "-6"));
    reset_act_data(); actData[26] = (char)0x5E;
    assert(run_command("SetBivalentOperatingMode", "1") && sent[27] == 0x65);
    assert(!run_command("SetSGReadyMode", "1") && !run_command("SetSGReadyMode", "2"));
    assert(run_command("SetBivalentOperatingMode", "0") && sent[27] == 0x55);
    assert(run_command("SetSGReadyMode", "2") && sent[27] == 0x5D);
    assert(!run_command("SetBivalentOperatingMode", "1"));
    assert(run_command("SetBivalentOperatingMode", "2") && sent[27] == 0x7D); // intelligent does not conflict
    assert(run_command("SetBivalentOperatingMode", "0") && sent[27] == 0x5D);
    reset_act_data();
  }
  { // state bivalent control off (26 = 0x5D) with a hidden bivalent SG ready pattern (27 = 0x65); every way out works
    reset_act_data(); actData[26] = (char)0x5D; actData[27] = (char)0x65;
    assert(run_command("SetSGReadyMode", "1") && sent[27] == 0x69);
    assert(run_command("SetSGReadyMode", "0") && sent[27] == 0x65);
    assert(run_command("SetBivalentOperatingMode", "0") && sent[27] == 0x55);
    assert(!run_command("SetBivalentOperatingMode", "1")); // needs bivalent control = yes
    reset_act_data(); actData[26] = (char)0x5D; actData[27] = (char)0x65;
    assert(run_command("SetBivalentOperatingMode", "0") && sent[27] == 0x55);
    assert(run_command("SetSGReadyMode", "2") && sent[27] == 0x5D);
    assert(run_command("SetSGReadyMode", "0") && sent[27] == 0x55);
    // bivalent control on, bivalent SG ready and SG ready both set (reached at the remote controller): both can be switched off
    reset_act_data(); actData[26] = (char)0x5E; actData[27] = (char)0x69;
    assert(!run_command("SetBivalentOperatingMode", "1"));
    assert(run_command("SetSGReadyMode", "0") && sent[27] == 0x65);
    reset_act_data(); actData[26] = (char)0x5E; actData[27] = (char)0x69;
    assert(run_command("SetBivalentOperatingMode", "0") && sent[27] == 0x59);
    reset_act_data(); actData[26] = (char)0x5E; actData[27] = (char)0x6D;
    assert(run_command("SetBivalentOperatingMode", "2") && sent[27] == 0x7D);
    assert(run_command("SetSGReadyMode", "0") && sent[27] == 0x75);
    // switching bivalent control on does not matter for "off"; SG ready on is refused as soon as it is on with b10
    reset_act_data(); actData[26] = (char)0x5D; actData[27] = (char)0x65;
    assert(run_command("SetSGReadyMode", "1") && sent[27] == 0x69);
    reset_act_data(); actData[26] = (char)0x5E; actData[27] = (char)0x65;
    assert(!run_command("SetSGReadyMode", "1") && !run_command("SetSGReadyMode", "2"));
    assert(run_command("SetSGReadyMode", "0") && sent[27] == 0x65);
    reset_act_data();
  }
  { // solar high limit and SG ready limits refuse without a data block and while their own byte reads 0
    reset_act_data(); actData[0] = 0;
    assert(!run_command("SetSolarHighLimit", "80"));
    reset_act_data(); actData[64] = 0;
    assert(!run_command("SetSolarHighLimit", "80"));
    reset_act_data(); actData[27] = (char)0x5D; actData[52] = 0; actData[102] = 0;
    assert(!run_command("SetSGReadyHeatPower1", "4.2") && !run_command("SetSGReadyCoolCapacity1", "-3"));
    assert(run_command("SetSGReadyDHWPower1", "4.2") && sent[51] == 43 && run_command("SetSGReadyCoolCapacity2", "-6") && sent[103] == 122);
    reset_act_data();
  }
  { // sensor offset of the return sensor and zone 1 sensor type: pending nibbles
    reset_act_data();
    assert(run_command("SetSensorOffsetReturn", "-2") && sent[36] == 0x48);
    assert(run_command("SetSensorOffsetReturn", "1") && sent[36] == 0xA8);
    assert(run_command("SetZ1SensorSettings", "2") && sent[22] == 0x13);
    assert(run_command("SetZ1SensorSettings", "0") && sent[22] == 0x11);
    assert(actData[36] == (char)0x88 && actData[22] == 0x11);
  }
  { // low nibbles of bytes 36 / 37 (DHW tank, flow): the other nibble stays, also across commands in a row
    reset_act_data(); actData[36] = (char)0x48; actData[37] = (char)0x68;
    assert(run_command("SetSensorOffsetDhwTank", "1") && sent[36] == 0x4A);
    assert(run_command("SetSensorOffsetReturn", "3") && sent[36] == 0xEA);   // low nibble from the pending byte
    assert(run_command("SetSensorOffsetDhwTank", "-3") && sent[36] == 0xE2); // high nibble from the pending byte
    assert(run_command("SetSensorOffsetFlow", "0.5") && sent[37] == 0x69);
    assert(run_command("SetSensorOffsetFlow", "-1") && sent[37] == 0x66);
    for (unsigned int i = 0; i < sent.size(); ++i) if (i != 37) assert(sent[i] == panasonicSendQuery[i]);
    assert(actData[36] == (char)0x48 && actData[37] == (char)0x68);
    reset_act_data(); actData[36] = 0; actData[37] = 0;
    assert(!run_command("SetSensorOffsetDhwTank", "1") && !run_command("SetSensorOffsetFlow", "1"));
    reset_act_data(); actData[0] = 0;
    assert(!run_command("SetSensorOffsetDhwTank", "1") && !run_command("SetSensorOffsetFlow", "1"));
    reset_act_data();
  }
  { // shared-byte commands never send a byte they could not complete: no data block, byte 0
    reset_act_data(); actData[0] = 0;
    assert(!run_command("SetEfficiencyStage3Time", "15") && !run_command("SetBivalentAPStartDelay", "35"));
    assert(!run_command("SetAntiFreezeMode", "0") && !run_command("SetDHWDefrost", "0"));
    reset_act_data(); actData[34] = 0; actData[35] = 0; actData[33] = 0;
    assert(!run_command("SetEfficiencyStage3Time", "15") && !run_command("SetSensorOffsetOutdoor", "1"));
    assert(!run_command("SetSensorOffsetRemoteTh1", "1") && !run_command("SetEfficiencyStage1Time", "25"));
    reset_act_data();
  }
"""
    checks.append(sequences)
    cases += 35

    shadow_tests = r"""
  const unsigned long SETTLE = 2000, LIFE = 12000;
  { // N1: a data block received after the settle time ends the memory, whether it confirms or not
    reset_act_data();
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    deliver_block(SETTLE, 33, 0x75); // rejected or not applied: actData is the truth
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
    reset_act_data();
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    deliver_block(SETTLE, 33, 0x76); // confirmed
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x86);
    deliver_block(SETTLE, 33, 0x86);
    assert(run_command("SetEfficiencyStage1Time", "30") && sent[33] == 0x87);
  }
  { // a block that is on its way when the command is handed over still carries the old value and does not end it
    reset_act_data();
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    deliver_block(SETTLE - 1, 33, 0x75);
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x86);
    deliver_block(1); // 1 ms after the second command: still pending
    assert(run_command("SetEfficiencyStage1Time", "30") && sent[33] == 0x87);
    deliver_block(SETTLE, 33, 0x75); // the later block ends it (age counted from the last command)
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
  }
  { // N1: the other documented failure: upstream SetHeatingControl after SetDHWDefrost, read-back received
    reset_act_data(); actData[30] = (char)0x5A;
    assert(run_command("SetDHWDefrost", "0") && sent[30] == 0x59);
    assert(run_command("SetHeatingControl", "0") && sent[30] == 0x04); // upstream frame: only its own pair
    assert(run_command("SetDHWDefrost", "1") && sent[30] == 0x56);     // pending 0x55 (defrost b01, control b01), not 0x59/0x5A
    deliver_block(SETTLE, 30, 0x55);                                  // read-back
    assert(run_command("SetDHWDefrost", "1") && sent[30] == 0x56);
  }
  { // N1: bivalent mode changed by the upstream setter: AP heat is refused afterwards (needs advanced parallel)
    reset_act_data(); actData[26] = (char)0x5E;
    assert(run_command("SetBivalentAPHeat", "1") && sent[26] == 0x6E);
    assert(run_command("SetBivalentMode", "0") && sent[26] == 0x04);
    assert(!run_command("SetBivalentAPHeat", "0"));
    deliver_block(SETTLE, 26, 0x66);
    assert(!run_command("SetBivalentAPHeat", "0"));
  }
  { // N1: the pad heater (upstream, byte 25) is kept by the heater capacity command
    reset_act_data(); actData[25] = (char)0x25;
    assert(run_command("SetInternalHeaterCapacity", "6") && sent[25] == 0x29);
    assert(run_command("SetExternalPadHeater", "2") && sent[25] == 0x30);
    assert(run_command("SetInternalHeaterCapacity", "3") && sent[25] == 0x35);
  }
  { // N1: alternative outside sensor (upstream, byte 20) is kept by the anti freeze command
    reset_act_data(); actData[20] = (char)0x6A;
    assert(run_command("SetAntiFreezeMode", "0") && sent[20] == 0x66);
    assert(run_command("SetAltExternalSensor", "0") && sent[20] == 0x10);
    assert(run_command("SetAntiFreezeMode", "1") && sent[20] == 0x5A);
  }
  { // N1: smart DHW (upstream, byte 24) changes the byte the extra pump gate reads
    reset_act_data(); actData[24] = (char)0x56; actData[31] = (char)0x5B;
    assert(run_command("SetSmartDHW", "1") && sent[24] == 0x80);
    assert(run_command("SetExtraPumpMode", "0") && sent[31] == 0x17); // tank bits unchanged
    actData[24] = (char)0x55; // user removed the tank in the meantime: read back, pending ends
    deliver_block(SETTLE);
    assert(!run_command("SetExtraPumpMode", "0"));
  }
  { // N2: web console and rules engine call send_command() directly, without send_heatpump_command()
    reset_act_data();
    assert(run_direct("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    assert(run_direct("SetEfficiencyStage2Time", "35") && sent[33] == 0x86);
    assert(run_command("SetEfficiencyStage1Time", "30") && sent[33] == 0x87); // mixed paths
    assert(actData[33] == 0x75);
    reset_act_data(); actData[26] = (char)0x56; // bivalent control yes, mode alternative
    assert(run_direct("SetBivalentOperatingMode", "1") && sent[27] == 0x65);
    assert(run_direct("SetBivalentExternalPump", "1") && sent[27] == 0xA5);
    assert(run_command("SetDemandControlEnable", "1") && sent[27] == 0xA6);
  }
  { // N2: a raw frame (SendRawValue / proxy) goes through send_command() too and is remembered
    reset_act_data();
    byte frame[sizeof(panasonicSendQuery)];
    memcpy(frame, panasonicSendQuery, sizeof(frame));
    frame[33] = 0x76;
    send_command(frame, sizeof(frame));
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x86);
    // frames that are no write for the main data block are ignored: optional PCB header, short frame, no data block
    reset_act_data();
    frame[0] = 0xF1; frame[1] = 0x11; frame[2] = 0x01; frame[3] = 0x50; frame[33] = 0x76;
    send_command(frame, sizeof(frame));
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
    reset_act_data();
    memcpy(frame, panasonicSendQuery, sizeof(frame)); frame[33] = 0x76;
    send_command(frame, sizeof(frame) - 1);
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
    reset_act_data(); actData[0] = 0;
    send_command(frame, sizeof(frame));
    actData[0] = 0x71;
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
  }
  { // N3: expiry is a difference of unsigned 32-bit times and an expired entry is invalidated
    reset_act_data();
    fake_millis = 1000;
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    { byte command[256]; char log[256]; char payload[] = "35";
      fake_millis = 1000 + LIFE + 5; // expired; a dry run (Modbus) looks at the byte and does not use the entry
      assert(set_efficiency_stage2_time(payload, command, log) > 0 && command[33] == 0x85);
      // the dry run is read-only (it runs in another task): the entry is cleared by the next data block
      deliver_block(0);
      fake_millis = 1000 + 5000; // the clock is back in the old window, as it is after 2^32 ms
      assert(set_efficiency_stage2_time(payload, command, log) > 0 && command[33] == 0x85); }
    // a command shortly before the wrap of millis(): pending across the wrap, expired after it
    reset_act_data();
    fake_millis = 0xFFFFFFFFUL - 2000;
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    fake_millis += 5000; // wrapped
    assert(fake_millis < 5000);
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x86);
    fake_millis += 13000;
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
    // the block hook invalidates as well, so a pending entry does not wait for a look
    reset_act_data();
    fake_millis = 1000;
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    deliver_block(LIFE + 1000);
    fake_millis = 1000 + 5000;
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
  }
  { // N4: nothing handed over -> nothing remembered
    reset_act_data();
    listen_only = true;
    assert(!run_command("SetEfficiencyStage1Time", "25"));
    listen_only = false;
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
    reset_act_data();
    queue_full = true;
    assert(!run_command("SetEfficiencyStage1Time", "25"));
    queue_full = false;
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
    // ESP8266: a buffered command (send_command returns false) is handed over and remembered
    reset_act_data();
    buffered = true;
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    buffered = false;
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x86);
  }
  { // dry run (Modbus validation) and rejected commands record nothing and never touch actData
    reset_act_data();
    { byte command[256]; char log[256]; char payload[] = "25";
      assert(set_efficiency_stage1_time(payload, command, log) > 0 && command[33] == 0x76);
      char bad[] = "3";
      assert(set_efficiency_stage1_time(bad, command, log) == 0); }
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x85);
    char before[sizeof(actData)]; memcpy(before, actData, sizeof(before));
    assert(run_command("SetEfficiencyStage1Time", "25"));
    deliver_block(SETTLE);
    assert(memcmp(before, actData, sizeof(before)) == 0);
  }
  { // a frame byte with value 0 is no information: byte 35 stays with actData when only byte 33 was sent
    reset_act_data();
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    actData[35] = (char)0x96; // changed at the remote controller; the block is not processed yet
    assert(run_command("SetSensorOffsetOutdoor", "0") && sent[35] == 0x98);
  }
  { // one data block ends every pending byte that is old enough, not only the byte of the last command
    reset_act_data();
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);
    assert(run_command("SetSensorOffsetRemoteTh1", "1") && sent[35] == 0xA6);
    deliver_block(SETTLE, 33, 0x76); // block ends all pending bytes that are old enough, not only the byte of the last command
    assert(run_command("SetEfficiencyStage2Time", "35") && sent[33] == 0x86);
    assert(run_command("SetSensorOffsetOutdoor", "-1.5") && sent[35] == 0x85); // byte 35 ended too: actData (0x86) is used
  }
  { // ESP32: a frame that still waits in cmdQueue is not ended by the blocks that arrive meanwhile
    reset_act_data(); actData[28] = (char)0x55;
    queued = true;
    assert(run_command("SetEfficiencyStage1Time", "25") && sent[33] == 0x76);  // frame 1, another byte
    assert(run_command("SetCoolingWaterMode", "1") && sent[28] == 0x59);        // frame 2
    deliver_block(SETTLE, 28, 0x55, -1, 0, 2);     // both still queued
    deliver_block(SETTLE, 28, 0x55, 33, 0x76, 1);  // frame 1 has left and is answered; frame 2 still waits
    assert(run_command("SetHeatingWaterMode", "1") && sent[28] == 0x5A);        // frame 3, built from the pending 0x59
    deliver_block(LIFE + 1000, 28, 0x55, -1, 0, 2); // waiting frames outlive the life time
    assert(run_command("SetDefrostSignal", "1") && sent[28] == 0x9A);           // frame 4
    deliver_block(SETTLE - 1, 28, 0x59, -1, 0, 0);  // all frames left just now: this block is too early to be trusted
    assert(run_command("SetDefrostSignal", "0") && sent[28] == 0x5A);           // frame 5
    deliver_block(1, 28, 0x9A, -1, 0, 0);           // frame 5 left; answer still shows the old value
    deliver_block(SETTLE, 28, 0x5A);                // read-back with an empty queue ends the pending state
    actData[28] = (char)0x55;
    assert(run_command("SetHeatingWaterMode", "1") && sent[28] == 0x56);
    queued = false;
  }
  { // other traffic in the queue does not keep an entry alive whose own frame has left
    reset_act_data(); actData[28] = (char)0x55;
    queued = true;
    assert(run_command("SetDefrostSignal", "1") && sent[28] == 0x95);           // the pump will not take it
    deliver_block(500, 28, 0x55, -1, 0, 0);          // the frame left, answer with the old value
    for (int i = 0; i < 20; i++) {                   // a rule sends one command on another byte per block
      assert(run_command("SetEfficiencyStage1Time", i % 2 ? "25" : "20"));
      deliver_block(5000, 28, 0x55, -1, 0, 1);
    }
    assert(run_command("SetHeatingWaterMode", "1") && sent[28] == 0x56);        // actData, not the stale 0x95
    deliver_block(0, -1, 0, -1, 0, 0);               // the queue drains
    // a frame that never leaves the queue ends after the safety time
    reset_act_data(); actData[28] = (char)0x55;
    assert(run_command("SetDefrostSignal", "1") && sent[28] == 0x95);
    deliver_block(59000, 28, 0x55, -1, 0, 1);
    { byte command[256]; char log[256]; char payload[] = "1";
      assert(set_heating_water_mode(payload, command, log) > 0 && command[28] == 0x96); }
    deliver_block(1000, 28, 0x55, -1, 0, 1);
    { byte command[256]; char log[256]; char payload[] = "1";
      assert(set_heating_water_mode(payload, command, log) > 0 && command[28] == 0x56); }
    deliver_block(0, -1, 0, -1, 0, 0);               // the queue drains: ring and entries are consistent again
    queued = false;
  }
  { // ESP8266: a buffered frame that is sent later does not put its older value back
    reset_act_data(); actData[28] = (char)0x55;
    assert(run_command("SetHeatingWaterMode", "1") && sent[28] == 0x56);        // written directly
    buffered = true;
    assert(!run_command("SetCoolingWaterMode", "1") || true); assert(sent[28] == 0x5A);
    std::vector<byte> second = sent;
    assert(!run_command("SetDefrostSignal", "1") || true); assert(sent[28] == 0x9A);
    std::vector<byte> third = sent;
    buffered = false;
    deliver_block(SETTLE, 28, 0x56);                 // answer to the first frame: the buffered ones are kept
    rmw_set_resending(true); send_command(second.data(), (int)second.size()); rmw_set_resending(false);
    assert(run_command("SetCoolingWaterMode", "0") && sent[28] == 0x96);        // newer value kept, not 0x5A
    deliver_block(SETTLE, 28, 0x5A);                 // the third frame still waits
    assert(run_command("SetCoolingWaterMode", "1") && sent[28] == 0x9A);
    rmw_set_resending(true); send_command(third.data(), (int)third.size()); rmw_set_resending(false);
    deliver_block(SETTLE, 28, 0x9A);                 // nothing waits any more: ends
    actData[28] = (char)0x55;
    assert(run_command("SetHeatingWaterMode", "1") && sent[28] == 0x56);
  }
"""
    checks.append(shadow_tests)
    cases += 17

    # structure of the production code that the host harness cannot compile (HeishaMon.ino, rules.cpp)
    ino = (ROOT / "HeishaMon/HeishaMon.ino").read_text()
    rules = (ROOT / "HeishaMon/rules.cpp").read_text()
    assert ino.count("rmw_note_command_sent(command, length)") == 1 and ino.count("rmw_note_command_queued(command, length, ") == 2, "ESP8266 send; ESP32 queue and ESP8266 buffer path"
    assert re.search(r"if \(xQueueSend\(cmdQueue, &cmd, 0\) != pdTRUE\) return false;[^\n]*\n\s*rmw_note_command_queued\(command, length, true\);", ino)
    assert re.search(r"if \(cmdnrel \+ 1 <= MAXCOMMANDSINBUFFER\) rmw_note_command_queued\(command, length, false\);[^\n]*\n\s*pushCommandBuffer", ino)
    assert re.search(r"sendCommandReadTime = millis\(\);[^\n]*\n\s*rmw_note_command_sent\(command, length\);", ino)
    assert re.search(r"decode_heatpump_data\(data, actData[^\n]*\n#ifdef ESP32\n\s*rmw_data_block_received\(cmdQueue != NULL \? \(unsigned int\)uxQueueMessagesWaiting\(cmdQueue\) : 0\);[^\n]*\n#else\n\s*rmw_data_block_received\(0\);", ino)
    assert ino.count("rmw_data_block_received(") == 2
    assert re.search(r"rmw_set_resending\(true\);[^\n]*\n\s*send_command\(cmdbuffer\[cmdstart\]\.data, cmdbuffer\[cmdstart\]\.length\);\n\s*rmw_set_resending\(false\);", ino)
    body = source.split("static byte effective_byte")[1].split("static byte merge_sent_byte")[0]
    assert " = " not in body.replace("i = 0", "").replace("!= address", ""), "effective_byte is read-only"

    settle = int(re.search(r"#define RMW_SETTLE_MS (\d+)UL", source)[1])
    assert settle == int(re.search(r"#define SERIALTIMEOUT (\d+)", ino)[1]), "settle time is tied to SERIALTIMEOUT"
    assert int(re.search(r"#define RMW_SHADOW_MS (\d+)UL", source)[1]) == 12000
    # every writer (MQTT/Modbus via send_heatpump_command, web console, rules) ends in send_command(cmd, len)
    assert re.search(r"if \(len > 0\) send_command\(cmd, len\);", source)
    assert re.search(r"len = tmp\.func\(cpy, cmd, log_msg\);.*?send_command\(cmd, len\);", ino, re.S)
    assert re.search(r"tmp\.func\(payload, cmd, log_msg\);.*?send_command\(cmd, len\);", rules, re.S)
    assert "send_heatpump_command(topicName, payload, send_command" in (ROOT / "HeishaMon/HeishaModbusServer.cpp").read_text()
    assert "remember_rmw_command" not in source and "rmwCommands" not in source
    cases += 13

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
