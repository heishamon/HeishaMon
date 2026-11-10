# Testing the Aquarea parity test firmware on your heat pump

This guide is for HeishaMon owners who want to help find out whether the new topics (TOP170 to TOP196) and commands (SET56 to SET104) work on their heat pump series. Background and evidence: [AquareaParity.md](AquareaParity.md). Topic and command reference: [MQTT-Topics.md](MQTT-Topics.md). German version: [AquareaParity-Testing_DE.md](AquareaParity-Testing_DE.md).

## What is known so far

- Everything was worked out on **one M-series unit** (WH-SDC0316M9E8 with WH-WXG09ME8). Nothing is known about H, J, K and L series.
- The test firmware runs on a HeishaMon large (ESP32-S3) on that unit: it boots, keeps all existing topics, and the 27 new topics show the values of the installer menu.
- On that unit the new **commands** were sent through this firmware and read back (heat pump off): the values arrived, neighbouring settings in the same byte stayed, commands that must be refused were refused, and everything was set back. Left out on purpose: `SetZoneType`, `SetAntiFreezeMode 0`, `SetDefrostSignal 1`, `SetZ1SensorSettings 3`.
- Whether a setting exists on another series is not known. It may well work there; it has simply not been tried.
- This is test firmware. You use it at your own risk. Do not test on a system you cannot reach physically.

## What is new in this firmware

**27 new read topics, 26 of them with a new command.** They show installer-menu settings that HeishaMon could not read before.

| Topic | Name | What it is | New command |
|---|---|---|---|
| TOP170 | Static_Pressure | Static pressure option | SetStaticPressure |
| TOP171 | Heating_On_Outdoor_Temp | Outdoor temperature below which heating switches on | SetHeatingOnOutdoorTemp |
| TOP172 | Heating_On_Delay | Delay before heating switches on | SetHeatingOnDelay |
| TOP173 | DHW_Booster_Delay | Switch-on delay of an external tank heater | SetDHWBoosterDelay |
| TOP174 to TOP176 | Efficiency_Stage1/2/3_Time | Stage times of heating control "efficiency" | SetEfficiencyStage1Time, …2Time, …3Time |
| TOP177, TOP178 | Sensor_Offset_Remote_Th1, Sensor_Offset_Outdoor | Sensor calibration offsets | SetSensorOffsetRemoteTh1, SetSensorOffsetOutdoor |
| TOP193, TOP195, TOP196 | Sensor_Offset_Return, Sensor_Offset_DHW_Tank, Sensor_Offset_Flow | Sensor calibration offsets | SetSensorOffsetReturn, SetSensorOffsetDhwTank, SetSensorOffsetFlow |
| TOP179 | Anti_Stick_Mode | Anti-stick (actuator exercise) | none: the heat pump ignores writes |
| TOP180, TOP181 | Bivalent_Operating_Mode, Bivalent_External_Pump | Bivalent sub-settings | SetBivalentOperatingMode, SetBivalentExternalPump |
| TOP182, TOP183 | Extra_Pump_Use, Extra_Pump_Mode | Extra pump: not used / heating / DHW, comfort / efficiency | SetExtraPump, SetExtraPumpMode |
| TOP184 to TOP190 | SG_Ready_1/2_DHW/Heat/Cool_Power, SG_Ready_Stop_Power | SG ready power consumption limits (kW) | SetSGReadyDHWPower1, …HeatPower1, …CoolPower1, …DHWPower2, …HeatPower2, …CoolPower2, SetSGReadyStopPower |
| TOP191, TOP192 | SG_Ready_1/2_Cool_Capacity | SG ready cooling offsets | SetSGReadyCoolCapacity1, …2 |
| TOP194 | Defrost_Signal | Defrost signal output (shares a terminal with bivalent control) | SetDefrostSignal |

**22 existing topics that were read-only and can now be written.**

| Topic | Name | New command |
|---|---|---|
| TOP45 | Room_Holiday_Shift_Temp | SetRoomHolidayShiftTemp |
| TOP76, TOP81 | Heating_Mode, Cooling_Mode (compensation curve / direct) | SetHeatingWaterMode, SetCoolingWaterMode |
| TOP100 | DHW_Installed | SetDHWInstalled |
| TOP101 to TOP105 | Solar_Mode, Solar_On_Delta, Solar_Off_Delta, Solar_Frost_Protection, Solar_High_Limit | SetSolarMode, SetSolarOnDelta, SetSolarOffDelta, SetSolarFrostProtection, SetSolarHighLimit |
| TOP107 | Liquid_Type (water / glycol) | SetLiquidType |
| TOP109 | Anti_Freeze_Mode | SetAntiFreezeMode |
| TOP111 | Z1_Sensor_Settings | SetZ1SensorSettings |
| TOP132, TOP136, TOP137 | Bivalent_Advanced_Heat, …Start_Delay, …Stop_Delay | SetBivalentAPHeat, SetBivalentAPStartDelay, SetBivalentAPStopDelay |
| TOP146 | Zone_Configuration (zone 1 room / pool) | SetZoneType |
| TOP147 | Internal_Heater_Capacity (3 / 6 / 9 kW) | SetInternalHeaterCapacity |
| TOP148 | DHW_Heater_Type | SetDHWHeaterType |
| TOP149 | SG_Ready_Control | SetSGReadyMode |
| TOP150 | Demand_Control | SetDemandControlEnable |
| TOP151 | DHW_Defrost | SetDHWDefrost |
| TOP153 | Pool_Delta | SetPoolDelta |

**Web interface.** The new topics appear in the "Heatpump Values" table and in `/json`.

**Not changed.** All existing topics keep their numbers and names; existing commands send the same bytes as before. New commands on bytes that hold several settings send only their own field, like the existing ones.

## What you need

- A HeishaMon you can reach with a USB-C cable if something goes wrong (large board), or the serial adapter you used for the first flash (small board).
- The firmware file you are running now, to go back. Released files are in [binaries/](binaries/).
- 30 minutes for the read-only test. For the write test another 30 minutes with the heat pump switched off.
- Access to the installer menu of your remote controller (to compare values), or the Aquarea Service Cloud.

## Step 1: note your current state

1. Open the HeishaMon web page and write down the firmware version shown at the top.
2. Open `http://<heishamon>/json` and save the page. It is your record of all values before the test.
3. Take photos of the settings page. Settings and rules are kept by an update, but there is no export.

## Step 2: flash

1. Download a test build of this change. Where to find it is written in the pull request that adds these topics (look for "aquarea parity" in the pull requests of HeishaMon).
2. Pick the file for your board: `HeishaMon_ESP32-…bin` for the large board, `HeishaMon_ESP8266-…bin` for the small one.
3. On the HeishaMon web page open **Firmware**, choose the file, and check that the MD5 field is filled (it is taken from the file name; if it is empty, copy the character string before `.bin` from the file name).
4. Start the update and wait for the success message and the restart (about 20 seconds without values).

If the upload fails or the MD5 does not match, the old firmware stays in place. There is **no automatic fallback** if the new firmware starts and then misbehaves: flash your previous file from the same page, or by USB if the page is not reachable.

## Step 3: check that it runs like before (no commands)

Watch for 30 minutes:

| Check | Expected |
|---|---|
| Version at the top of the web page | the name of the test build |
| Free memory and "Correct data" on the web page | about the same as before the update |
| MQTT values | arrive as before |
| Uptime | keeps counting, no restarts |
| CZ-TAW1 on the proxy port (if you use it) | cloud keeps working |

If any of this is wrong, go back to your previous firmware and report it (step 6).

## Step 4: read-only test (safe on every series)

Open the web page (table "Heatpump Values") or `/json` and compare the new rows with the installer menu of your remote controller. Nothing is written in this step.

| Topic | Name | Installer menu item to compare |
|---|---|---|
| TOP170 | Static_Pressure | Static pressure |
| TOP171 | Heating_On_Outdoor_Temp | Outdoor temperature for heating ON |
| TOP172 | Heating_On_Delay | Heating ON delay time |
| TOP173 | DHW_Booster_Delay | Booster heater delay (external tank heater) |
| TOP174 to TOP176 | Efficiency_Stage1/2/3_Time | Heating control "efficiency", stage times |
| TOP177, TOP178 | Sensor_Offset_Remote_Th1, Sensor_Offset_Outdoor | Sensor calibration |
| TOP193, TOP195, TOP196 | Sensor_Offset_Return, Sensor_Offset_DHW_Tank, Sensor_Offset_Flow | Sensor calibration |
| TOP179 | Anti_Stick_Mode | Custom menu, anti-stick |
| TOP180, TOP181 | Bivalent_Operating_Mode, Bivalent_External_Pump | Bivalent connection |
| TOP182, TOP183 | Extra_Pump_Use, Extra_Pump_Mode | Extra pump |
| TOP184 to TOP192 | SG_Ready_… | SG ready power limits and cooling offsets |
| TOP194 | Defrost_Signal | Defrost signal |

For each row note one of: **matches**, **differs** (write both values), **menu item does not exist on my unit**. A value that never changes and makes no sense (for example 0 minutes, -128 °C) usually means the byte is not used on your series. That is a useful result too.

If you can, change one of these settings **on the remote controller** and check that the topic follows. That proves the byte position on your series without sending anything from HeishaMon.

## Step 5: write test (optional, heat pump switched off)

Only do this if step 4 matched for the setting you want to test. Switch the heat pump off first (wait until the compressor has stopped by itself or has run for at least 20 minutes).

How to send a command: MQTT topic `<base>/commands/<Name>` with the value as payload, or `http://<heishamon>/command?<Name>=<value>` in a browser. The log on the **Console** page shows whether the command was sent or refused.

For each test: send the test value, wait 40 seconds, check the topic and the remote controller, send the old value again, wait 40 seconds, check again.

Suggested first tests (each changes one harmless setting; use your own current value to go back):

| Command | Test value | Topic to watch |
|---|---|---|
| `SetHeatingOnDelay` | your value + 30 (multiples of 30 minutes) | Heating_On_Delay |
| `SetEfficiencyStage1Time` | your value + 5 (multiples of 5 minutes) | Efficiency_Stage1_Time, and Efficiency_Stage2_Time must stay the same |
| `SetSensorOffsetOutdoor` | your value + 0.5 | Sensor_Offset_Outdoor, and Sensor_Offset_Remote_Th1 must stay the same |
| `SetStaticPressure` | the other value (0 / 1) | Static_Pressure, and Extra_Pump_Use must stay the same |

Rules for the write test:

- One command at a time. Wait for the next data block before sending another command.
- After each test compare `/json` with the copy from step 1. Only the tested value may differ.
- Stop at the first unexpected change or error code, restore the old value on the remote controller, and report.

Do **not** send these unless you know what they do on your installation:

- `SetBuffer 1` without a buffer sensor (error H43 on the M series, stays until reset).
- `SetSolarMode 1` or `2` without a solar sensor (error H28 on the M series while the heat pump runs; it switches off and stays off until reset).
- `SetAntiFreezeMode 0`.
- `SetInternalHeaterCapacity` with a value above the back-up heater that is installed (not tested, result unknown).
- Switches for hardware you do not have: `SetDHWInstalled`, `SetSolarMode`, `SetSGReadyMode`, `SetBivalentControl`, `SetZoneType`, `SetZ1SensorSettings`.
- `SetHeatingWaterMode` / `SetCoolingWaterMode` (they move the heating and cooling curves).
- `SetDefrostSignal` together with bivalent control (the heat pump switches one of them off).

## Step 6: report

Please report in the pull request, or where you got the test build, also when everything works:

1. Heat pump model (indoor and outdoor unit) and series, HeishaMon board (small / large), firmware version before and the version tested.
2. Result of step 3 (runs like before: yes / no, what differed).
3. The table of step 4 with matches / differs / not present.
4. For step 5: command, value sent, what the topic and the remote controller showed, and the Console log lines.
5. Which back-up heater capacities the installer menu of your remote controller offers (for example only 3 kW, or 3 / 6 / 9 kW), and what Internal_Heater_Capacity shows. Reading is enough; do not write a higher value.
6. If your installation has a buffer tank, solar, pool or DHW tank sensor: the values of Buffer_Temp, Solar_Temp, Pool_Temp and DHW_Temp in `/json`, and whether the matching function (Buffer_Installed, Solar_Mode, DHW_Installed) is on or off. Reading is enough. On the M-series test unit without these sensors they read -78 °C (DHW: -128 °C); we want to know what a connected sensor shows, also while the function is switched off.
7. Your `/json` output before and after, if you are willing to share it.

## Going back

Flash your previous firmware file from the **Firmware** page. Settings stay. If the web page cannot be reached, flash by USB; a factory reset is possible by holding the BOOT button for more than 10 seconds (large board), after which the hotspot `HeishaMon-Setup` appears.
