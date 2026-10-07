# Modbus register map v3

TCP port **502**, unit ID **1**. All addresses below are zero-based protocol offsets,
without a 40001 prefix. Add 1 only if your client expects one-based addressing.
ESP32 supports Modbus TCP; ESP8266 does not.

## Enabling Modbus TCP

Modbus TCP has no authentication, so it is **off by default**. In **Settings**:

- **Enable Modbus TCP server (port 502)**: starts the server after a reboot. Without
  further options the server is read-only.
- **Allow Modbus writes**: additionally accepts FC05 (relays) and FC06 (heat pump
  commands, including `SetReset`). While this is off, write requests are answered
  with `ILLEGAL_FUNCTION`.

Only enable Modbus on a network you trust.

Modbus requests are handled in a different task than the rest of the firmware.
Register reads are served from a snapshot of the heat pump data that the main loop
refreshes, and accepted writes are queued and executed by the main loop like any
MQTT command. A write is therefore acknowledged when it is queued, not when the
heat pump has processed it. If the queue is full the request is answered with
`SERVER_DEVICE_BUSY`; retry it later.

## Address map at a glance

From low to high; every block has reserved room to grow.

| Range | Function codes | Contents |
| --- | --- | --- |
| 0-4999 | FC03 | int16 measurements: main 0, extra 1000, optional PCB 2000, S0 3000 |
| 5000-9999 | FC06, FC16 | int16 commands: heat pump 5000, optional PCB 6000, system 7000 |
| 10000-19999 | FC03 | float32 measurements (2 registers each): main 10000, extra 12000, optional PCB 14000, S0 16000 |
| 20000-29999 | FC16 | float32 commands (2 registers each): heat pump 20000, optional PCB 22000, system 24000 |
| Coils 30000-31999 | FC01, FC05 | Relays (coil 30000 = relay 1, coil 30001 = relay 2). Coils are a separate address space in Modbus, so this does not overlap the registers above. |
| 32000-32999 | FC03 | Device information: register map version, relay state |

## Fixed blocks

Each measurement block reserves room for **1,000 topics**. Counts can grow without
moving any other block. Only implemented topics/commands are accessible; reserved
addresses return `ILLEGAL_DATA_ADDRESS`.

Registers whose data is not available also return `ILLEGAL_DATA_ADDRESS` instead of a
value decoded from an empty buffer: extra topics (XTOP) on heat pumps without the
extra data block, and optional PCB topics (OPT) while optional PCB emulation is disabled.

| Group | Reserved integer block (FC03, int16) | Reserved float block (FC03, float32) | Currently implemented |
| --- | --- | --- | --- |
| Main TOPn | 0-999 | 10000-11999 | TOP0-TOP143: integer 0-143, float 10000-10287 |
| Extra XTOPn | 1000-1999 | 12000-13999 | XTOP0-XTOP5: integer 1000-1005, float 12000-12011 |
| Optional PCB OPTn | 2000-2999 | 14000-15999 | OPT0-OPT6: integer 2000-2006, float 14000-14013 |
| S0 inputs | 3000-3999 | 16000-17999 | Input 1: integer 3000-3005, float 16000-16011; input 2: integer 3100-3105, float 16200-16211 |

**Integer address = group base + topic number.**
**Float start = 10000 + 2 * integer address.**
Read two registers for a float: MSW at the start address, LSW at start + 1.
All floats are IEEE 754 float32, unscaled.

| Other group | Reserved block | Access | Implemented |
| --- | --- | --- | --- |
| Heat-pump commands | 5000-5999 | FC06 / FC16, int16 | 5000-5048; 5015 is reserved for JSON-only SetCurves and rejects writes |
| Optional PCB commands | 6000-6999 | FC06 / FC16, int16 | 6000-6013 |
| System commands | 7000-7999 | FC06 / FC16, int16 | 7000 = SetReset |
| Float32 commands | 20000-29999 | FC16, float32 (2 registers) | 20000 + 2 * (command address - 5000): heat pump 20000-21999, optional PCB 22000-23999, system 24000-25999 |
| Relay coils (separate coil address space) | 30000-31999 | FC01 (read), FC05 (write) | 30000 = relay 1, 30001 = relay 2 |
| Device information | 32000-32999 | FC03, uint16 | 32000 = register map version, currently 3; 32010 / 32011 = relay 1 / relay 2 state (0 = off, 1 = on) |

FC05 accepts 0x0000 for off and 0xFF00 for on. Coil 30002 is not a relay.

The relay state can be read back with **FC01 (Read Coils)** on coils 30000 and 30001 (1 = on), or with
FC03 on registers 32010 (relay 1) and 32011 (relay 2) for clients without coil support. Reading
also works while *Allow Modbus writes* is off. The state follows the actual relay output, so
switching over MQTT or the web UI is reflected as well (refreshed in the main loop).
A relay written just before can still read its old state until the main loop has executed the write.

### Writing float32 values

Every command also has a float32 address, laid out like the int16 command block:
**float address = 20000 + 2 * (command address - 5000)**. Example: SetDHWTemp is int16 5010
and float32 20020 / 20021; SetPoolTemp is int16 6006 and float32 22012 / 22013.

Write a float with **FC16 (Write Multiple Registers)**: start at the high word (MSW first, like
reading), quantity 2, one command per request. The value is the **real, unscaled value**
(21.5 is 21.5, not 2150). Both words arrive in one request, so a half-written value is never
executed. FC06 cannot write floats. FC16 with quantity 1 at an int16 command address behaves
like FC06.

- Heat pump commands only take whole numbers, so 21.0 is accepted and 21.5 is rejected with
  `ILLEGAL_DATA_VALUE`. Optional PCB temperatures accept decimals (21.5).
- NaN, infinity and values beyond +-32767 are rejected with `ILLEGAL_DATA_VALUE`.
- A start address on the low word, or a quantity other than 2, is rejected
  (`ILLEGAL_DATA_ADDRESS` / `ILLEGAL_DATA_VALUE`).
FC03 supports 1-125 registers per request; reads across a reserved gap are rejected.
Only FC01, FC03, FC05, FC06 and FC16 are supported.

## Register page

Open **Modbus** in the device menu or `http://<heishamon-ip>/modbus`.
The searchable page is generated from the actual firmware ranges and command IDs.
It lists both addresses for each measurement, scaling, function codes and every
command, sorted in ascending order of the 16-bit / coil address. It only displays the map; it does not send commands or show live values.

## Scaling

The multiplier of the 16-bit integer registers is fixed by the topic's unit, not the current value text.
**The float32 registers are never scaled**: they hold the real value (20.5 = 20.5), so no
multiplier is needed there.


- Temperature (Celsius/Kelvin), flow, pressure and current (Ampere): **x100** in the int16
  registers. Divide by 100 in your client; 2050 means 20.50, -525 means -5.25.
- States, counters, power (W), rotational speed and other units: **x1**.
- Integer values are saturated to -32768 through 32767. Use floats for large counters/power values.
- Extra/optional topics use their own unit definitions, independent of the main topic at the
  same index.
- Non-numeric readings return 0. Error codes in the integer
  Error register 44 use A=1000, B=2000, ..., H=8000 plus the number: H74=8074.
  Its float counterpart returns 0 for text; use register 44 for error information.

**Temperature commands are x100 as well**, like the int16 readings. The heat pump only takes
whole degrees, so the value must be a multiple of 100: write 4500 to set SetDHWTemp to 45,
-500 for -5. Other values (for example 4550) are rejected with `ILLEGAL_DATA_VALUE`. Affected
commands: SetZ1HeatRequestTemperature, SetZ1CoolRequestTemperature,
SetZ2HeatRequestTemperature, SetZ2CoolRequestTemperature, SetDHWTemp, SetFloorHeatDelta,
SetFloorCoolDelta, SetDHWHeatDelta, SetHeaterStartDelta, SetHeaterStopDelta, SetBufferDelta,
SetHeatingOffOutdoorTemp, SetBivalentStartTemp, SetBivalentAPStartTemp,
SetBivalentAPStopTemp, SetHeaterOnOutdoorTemp and SetSterilizationTemp. All other commands (modes, states, times,
duty) are unscaled signed int16. Allowed values are those of the regular HeishaMon command
handlers. SetCurves needs JSON and must use MQTT/HTTP.

**Optional PCB temperature commands are x100, like the temperature readings**:
SetPoolTemp, SetBufferTemp, SetZ1RoomTemp, SetZ1WaterTemp, SetZ2RoomTemp, SetZ2WaterTemp
and SetSolarTemp. Write 2150 to set 21.50. The other optional PCB commands are x1.

Writing an optional PCB command while optional PCB emulation is disabled returns
`ILLEGAL_DATA_ADDRESS`.

## S0 inputs (large ESP32 board)

Both S0 inputs are readable with **FC03**. Enable S0 in Settings and configure the
correct **pulses per kWh** for each meter. The S0 group reserves 3000-3999;
each input has a permanent 100-field block. Unused fields remain invalid.
Adding S0 does not change existing addresses or the register map version (3).

| Value | Unit | S0 1 integer | S0 1 float MSW / LSW | S0 2 integer | S0 2 float MSW / LSW |
| --- | --- | --- | --- | --- | --- |
| Watt | W | 3000 | 16000 / 16001 | 3100 | 16200 / 16201 |
| WatthourTotal | Wh | 3001 | 16002 / 16003 | 3101 | 16202 / 16203 |
| Watthour_Last_Report | Wh | 3002 | 16004 / 16005 | 3102 | 16204 / 16205 |
| PulseQuality | % | 3003 | 16006 / 16007 | 3103 | 16206 / 16207 |
| AvgPulseWidth | ms | 3004 | 16008 / 16009 | 3104 | 16208 / 16209 |
| Enabled | 0/1 | 3005 | 16010 / 16011 | 3105 | 16210 / 16211 |

All S0 values use **x1**. Integer readings truncate fractions and saturate at 32767.
**Use floats for energy totals**, high power and fractional readings. Floats retain
about seven significant decimal digits; this is not an exact 64-bit energy counter.
To display kWh, divide the Wh total by 1000 in the client.

- `Watt` uses the S0 subsystem's calculated power, including its low-power decay.
- `WatthourTotal` uses accumulated pulses and the configured pulses/kWh. It includes
  totals restored by the existing MQTT mechanism. This feature adds no flash
  persistence; without a restore the counter starts again after reboot.
- `Watthour_Last_Report` holds the energy from the last completed S0 reporting
  interval (the same interval reported over MQTT/WebSocket). Before the first
  report it is 0. It is **not energy since the last Modbus read**: polling never
  resets or consumes pulses, energy counters or the reporting interval.
- `PulseQuality` follows the S0 UI convention `100 * (good + 1) / (good + bad + 1)`;
  an enabled input with no pulses yet reports 100%.
- `Enabled` is 1 when S0 is enabled, initialized and pulses/kWh is greater than 0.
  Otherwise all six values of that input are 0, including the status.
- All fields are read-only. Both words of each float are generated from one
  captured reading within an FC03 request. Read both words together, or all six
  floats in one request of 12 registers.

## Main commands

Addresses are based on permanent command IDs, not array order. The name to ID table
is `MAIN_COMMANDS` in `HeishaMon/ModbusRegisterMap.h`. IDs 1-1000 map to
5000 + ID - 1; ID 100 is reserved permanently and maps to SetReset at 7000.

| Address | Command |
| --- | --- |
| 5000 | `SetHeatpump` |
| 5001 | `SetHolidayMode` |
| 5002 | `SetQuietMode` |
| 5003 | `SetPowerfulMode` |
| 5004 | `SetZ1HeatRequestTemperature` |
| 5005 | `SetZ1CoolRequestTemperature` |
| 5006 | `SetZ2HeatRequestTemperature` |
| 5007 | `SetZ2CoolRequestTemperature` |
| 5008 | `SetOperationMode` |
| 5009 | `SetForceDHW` |
| 5010 | `SetDHWTemp` |
| 5011 | `SetForceDefrost` |
| 5012 | `SetForceSterilization` |
| 5013 | `SetPump` |
| 5014 | `SetMaxPumpDuty` |
| 5015 | `SetCurves` |
| 5016 | `SetZones` |
| 5017 | `SetFloorHeatDelta` |
| 5018 | `SetFloorCoolDelta` |
| 5019 | `SetDHWHeatDelta` |
| 5020 | `SetHeaterDelayTime` |
| 5021 | `SetHeaterStartDelta` |
| 5022 | `SetHeaterStopDelta` |
| 5023 | `SetMainSchedule` |
| 5024 | `SetAltExternalSensor` |
| 5025 | `SetExternalPadHeater` |
| 5026 | `SetBufferDelta` |
| 5027 | `SetBuffer` |
| 5028 | `SetHeatingOffOutdoorTemp` |
| 5029 | `SetExternalControl` |
| 5030 | `SetExternalError` |
| 5031 | `SetExternalCompressorControl` |
| 5032 | `SetExternalHeatCoolControl` |
| 5033 | `SetBivalentControl` |
| 5034 | `SetBivalentMode` |
| 5035 | `SetBivalentStartTemp` |
| 5036 | `SetBivalentAPStartTemp` |
| 5037 | `SetBivalentAPStopTemp` |
| 5038 | `SetForceHeater` |
| 5039 | `SetHeatingControl` |
| 5040 | `SetSmartDHW` |
| 5041 | `SetQuietModePriority` |
| 5042 | `SetPumpFlowrateMode` |
| 5043 | `SetDHWSensorSelection` |
| 5044 | `SetDHWHeaterState` |
| 5045 | `SetRoomHeaterState` |
| 5046 | `SetHeaterOnOutdoorTemp` |
| 5047 | `SetSterilizationTemp` |
| 5048 | `SetSterilizationMaxTime` |

## Optional PCB commands

Optional command IDs are explicitly assigned in `HeishaMon/ModbusRegisterMap.h`.
Their address is 6000 + ID. Enabling optional PCB emulation is still required
for the corresponding heat-pump command handlers.

| Address | Command |
| --- | --- |
| 6000 | `SetHeatCoolMode` |
| 6001 | `SetCompressorState` |
| 6002 | `SetSmartGridMode` |
| 6003 | `SetExternalThermostat1State` |
| 6004 | `SetExternalThermostat2State` |
| 6005 | `SetDemandControl` |
| 6006 | `SetPoolTemp` |
| 6007 | `SetBufferTemp` |
| 6008 | `SetZ1RoomTemp` |
| 6009 | `SetZ1WaterTemp` |
| 6010 | `SetZ2RoomTemp` |
| 6011 | `SetZ2WaterTemp` |
| 6012 | `SetSolarTemp` |
| 6013 | `SetOptPCBByte9` |

## System commands

| Address | Command |
| --- | --- |
| 7000 | `SetReset` |

## Extension rules

1. Append topic numbers within their existing group; never renumber or reuse a topic.
2. Keep block bases fixed. Compile-time checks reject more than 1,000 topics per group.
3. Give each new command a permanent, unused ID in `MAIN_COMMANDS`. Keep ID 100 reserved for SetReset.
4. Add optional command IDs to `OPTIONAL_COMMANDS` explicitly, regardless of upstream table order.
   The firmware does not compile while a command has no ID.
5. Update the documentation and run `python tests/modbus/run_tests.py` and the firmware build.
6. Changing an assigned address, its meaning or scaling requires a new map version.
