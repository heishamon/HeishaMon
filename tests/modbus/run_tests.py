"""Compile the actual Modbus server with host shims; no device/network is used."""

from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = HERE / "build"
OUT.mkdir(parents=True, exist_ok=True)

# Only heat-pump decoding/command execution is stubbed. The register map, unit
# descriptions, real command tables, request handlers and HTML rows are compiled.
decode = (ROOT / "HeishaMon/decode.h").read_text(encoding="utf-8")
commands = (ROOT / "HeishaMon/commands.h").read_text(encoding="utf-8")
definitions = []
for result, signature in re.findall(r"^(String|unsigned int) ([^;\n]+);", decode + commands, re.M):
    if signature.startswith(("getDataValue(", "getDataValueExtra(", "getOptDataValue(")):
        continue
    definitions.append(f"{result} {signature} {{ return " + ('"0"' if result == "String" else "0") + "; }")
(OUT / "generated_stubs.h").write_text("\n".join(definitions) + "\n", encoding="utf-8")

source = HERE / "test_modbus.cpp"
executable = OUT / "test_modbus"
compiler = shutil.which("g++")
if not compiler:
    raise SystemExit("g++ is required")
subprocess.run([compiler, "-std=c++17", "-DESP32", "-I" + str(HERE / "stubs"),
                "-I" + str(OUT), str(source), "-o", str(executable)], check=True)
subprocess.run([str(executable)], check=True)

# Check the shipped integration against the new map.
loxone = ET.parse(ROOT / "Integrations/Loxone/MB_HeishaMon.xml").getroot()
# Title -> (address, Loxone function code). Measurements are float32 (FC03)
# except the error code, which only the int16 register carries (H74 = 8074);
# int16 commands use FC06 at 5000+, temperature setpoints float32 FC16 at 20000+.
expected = {"Heat_Power_Consumption": (12000, 3), "Heat_Power_Production": (12006, 3),
            "ErrorInformation": (44, 3), "SetHeatpump": (5000, 6),
            "SetQuietMode": (5002, 6), "SetOperationMode": (5008, 6),
            "SetMaxPumpDuty": (5014, 6), "SetZ1HeatRequestTemperature": (20008, 16),
            "SetZ1CoolRequestTemperature": (20010, 16)}
actual = {entry.attrib["Title"]: (int(entry.attrib["ModbusAddress"]), int(entry.attrib["ModbusCmd"]))
          for entry in loxone.findall("ModbusCmd")}
wrong = {name: actual.get(name) for name, value in expected.items() if actual.get(name) != value}
assert not wrong, f"Loxone template does not match register map: {wrong}"
print("PASS: Loxone map v3 template")
