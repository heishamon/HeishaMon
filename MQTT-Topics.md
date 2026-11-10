# MQTT Topics for HeishaMon

`TOP...` IDs identify the normal decoded heat pump values; `XTOP...` and `OPT...` identify values from the extra data block and optional PCB. Not every MQTT topic needs such an ID: device status and event topics such as `LWT`, `log`, `stats`, `rules/state`, and `rules/executed` are documented separately without a TOP number, and so are the GPIO and relay states (`gpio/...`).

## Availability Topic:

ID | Topic | Response
--- | --- | ---
|| LWT | Online/Offline (automatically returns to Offline if connection with the HeishaMon lost)

## Rules Engine Status:

Topic | Response/Description
--- | ---
rules/state | Read-only, retained: `1` if at least one rule block is loaded, `0` if no rule blocks are loaded (both ESP8266 and ESP32).

The full topic is `<base>/rules/state`, for example `panasonic_heat_pump/rules/state`. It is published on MQTT connection/reconnection, when the state changes, and at the configured full-data resend interval (`updateAllTime`). Failed publications are retried. There is no command to set this topic; publishing to it does not enable or disable rules.

This reports loaded rules, not whether a rule is executing at that instant. `0` can mean that no ruleset exists, parsing failed, rules were deinitialized, or loading was skipped after a crash reboot. It does not distinguish these causes. The existing `<base>/stats` JSON field `"rules active"` provides the number of loaded rule blocks. Check `<base>/LWT` for device availability; a retained rules state can outlive a disconnected device.

## Rule Execution Events:

`<base>/rules/executed` (for example `panasonic_heat_pump/rules/executed`) publishes a read-only JSON event after a rule execution returns, when **Publish rule executions to MQTT** is enabled in the settings (off by default, because every rule run adds a publish). It is **not retained**.

```json
{"rule":"timer=1","rule_id":2,"result":0,"uptime_ms":123456,"nested":false,"duration_us":245}
```

- `rule`: block name, such as `System#Boot`, `timer=1`, `@Outside_Temp`, or a custom helper name.
- `rule_id`: the engine's numeric block ID; it may change when the ruleset is edited.
- `result`: interpreter return code (`0` = successful return, nonzero = error). Success does not imply that a command was emitted or acknowledged by the heat pump.
- `uptime_ms`: device uptime at publication, in milliseconds (wraps after approximately 49.7 days).
- `nested`: `true` for a successfully completed helper block invoked by another rule. Each completed helper produces its own event before the outer block's event.
- `duration_us`: elapsed microseconds for an outer Boot, timer, or topic/event invocation, including its helper calls. Omitted for nested helper events. Nested event publication time is included in the outer duration.

Ruleset validation does not publish execution events. A returned error is reported for the outer invocation; a helper that fails before returning successfully has no separate completion event. A crash cannot produce a completion event. Events use MQTT QoS 0, are sent only while connected, and are neither queued nor replayed after reconnecting. Boot events may therefore be absent if MQTT is not connected yet. Every execution can produce traffic, including frequent timer/topic handlers and their helper calls.

## Log Topic:

ID | Topic | Response
--- | --- | ---
LOG1 | log | response from headpump (level switchable)

## Sensor Topics:

ID | Topic | Response/Description
:--- | --- | ---
TOP0 | main/Heatpump_State | Heatpump state (0=off, 1=on)
TOP1 | main/Pump_Flow | Pump flow (l/min)
TOP2 | main/Force_DHW_State | DHW status (0=off, 1=on)
TOP3 | main/Quiet_Mode_Schedule | Quiet mode schedule (0=inactive, 1=active)
TOP4 | main/Operating_Mode_State | Operating mode (0=Heat only, 1=Cool only, 2=Auto(Heat), 3=DHW only, 4=Heat+DHW, 5=Cool+DHW, 6=Auto(Heat)+DHW, 7=Auto(Cool), 8=Auto(Cool)+DHW)
TOP5 | main/Main_Inlet_Temp | Main inlet water temperature (°C)
TOP6 | main/Main_Outlet_Temp | Main outlet water temperature (°C)
TOP7 | main/Main_Target_Temp | Main outlet water target temperature (°C)
TOP8 | main/Compressor_Freq | Compressor frequency (Hz)
TOP9 | main/DHW_Target_Temp | DHW target temperature (°C)
TOP10 | main/DHW_Temp | Actual DHW temperature (°C)
TOP11 | main/Operations_Hours | Heatpump operating time (Hours)
TOP12 | main/Operations_Counter | Heatpump starts (counter)
TOP13 | main/Main_Schedule_State | Main thermostat schedule state (0=inactive, 1=active)
TOP14 | main/Outside_Temp | Outside ambient temperature (°C)
TOP15 | main/Heat_Power_Production | Thermal heat power production (Watt) — invalid on heatpumps with extra data block support, see XTOP3
TOP16 | main/Heat_Power_Consumption | Elektrical heat power consumption at heat mode (Watt) — invalid on heatpumps with extra data block support, see XTOP0
TOP17 | main/Powerful_Mode_Time | Powerful state in minutes (0, 1, 2 or 3 x 30min)
TOP18 | main/Quiet_Mode_Level | Quiet mode level (0=off, 1=less power, 2=even less power, 3=least power)
TOP19 | main/Holiday_Mode_State | Holiday mode (0=off, 1=scheduled, 2=active)
TOP20 | main/ThreeWay_Valve_State | 3-way valve mode (0=Room, 1=DHW)
TOP21 | main/Outside_Pipe_Temp | Outside pipe temperature (°C)
TOP22 | main/DHW_Heat_Delta | DHW heating delta (-12 to -2) (K)
TOP23 | main/Heat_Delta | Heat delta (K)
TOP24 | main/Cool_Delta | Cool delta (K)
TOP25 | main/DHW_Holiday_Shift_Temp | DHW Holiday shift temperature  (-15 to +15)
TOP26 | main/Defrosting_State | Defrost state (0=off, 1=on)
TOP27 | main/Z1_Heat_Request_Temp | Zone 1 Heat Requested shift temp (-5 to 5) or direct heat temp (20 to max)
TOP28 | main/Z1_Cool_Request_Temp | Zone 1 Cool Requested shift temp (-5 to 5) or direct cool temp (5 to 20)
TOP29 | main/Z1_Heat_Curve_Target_High_Temp | Target temperature at lowest point on the heating curve (°C)
TOP30 | main/Z1_Heat_Curve_Target_Low_Temp | Target temperature at highest point on the heating curve (°C)
TOP31 | main/Z1_Heat_Curve_Outside_High_Temp | Lowest outside temperature on the heating curve (°C)
TOP32 | main/Z1_Heat_Curve_Outside_Low_Temp | Highest outside temperature on the heating curve (°C)
TOP33 | main/Room_Thermostat_Temp | Remote control thermostat temp (°C)
TOP34 | main/Z2_Heat_Request_Temp | Zone 2 Heat Requested shift temp (-5 to 5) or direct heat temp (20 to max)
TOP35 | main/Z2_Cool_Request_Temp | Zone 2 Cool Requested shift temp (-5 to 5) or direct cool temp (5 to 20)
TOP36 | main/Z1_Water_Temp | Zone 1 Water outlet temperature (°C)
TOP37 | main/Z2_Water_Temp | Zone 2 Water outlet temperature (°C)
TOP38 | main/Cool_Power_Production | Thermal cooling power production (Watt) — invalid on heatpumps with extra data block support, see XTOP4
TOP39 | main/Cool_Power_Consumption | Elektrical cooling power consumption (Watt) — invalid on heatpumps with extra data block support, see XTOP1
TOP40 | main/DHW_Power_Production | Thermal DHW power production (Watt) — invalid on heatpumps with extra data block support, see XTOP5
TOP41 | main/DHW_Power_Consumption | Elektrical DHW power consumption (Watt) — invalid on heatpumps with extra data block support, see XTOP2
TOP42 | main/Z1_Water_Target_Temp | Zone 1 water target temperature (°C)
TOP43 | main/Z2_Water_Target_Temp | Zone 2 water target temperature (°C)
TOP44 | main/Error | Last active Error from Heat Pump
TOP45 | main/Room_Holiday_Shift_Temp | Room heating Holiday shift temperature (-15 to 15)
TOP46 | main/Buffer_Temp | Actual Buffer temperature (°C)
TOP47 | main/Solar_Temp | Actual Solar temperature (°C)
TOP48 | main/Pool_Temp | Actual Pool temperature (°C)
TOP49 | main/Main_Hex_Outlet_Temp | Outlet 2, after heat exchanger water temperature (°C)
TOP50 | main/Discharge_Temp | Discharge Temperature (°C)
TOP51 | main/Inside_Pipe_Temp | Inside pipe temperature (°C)
TOP52 | main/Defrost_Temp | Defrost temperature (°C)
TOP53 | main/Eva_Outlet_Temp | Eva Outlet temperature (°C)
TOP54 | main/Bypass_Outlet_Temp | Bypass Outlet temperature (°C)
TOP55 | main/Ipm_Temp | Ipm temperature (°C)
TOP56 | main/Z1_Temp | Zone1: Actual Temperature (°C) 
TOP57 | main/Z2_Temp | Zone2: Actual Temperature (°C) 
TOP58 | main/DHW_Heater_State | When enabled, backup/booster heater can be used for DHW heating (0=disabled, 1=enabled)
TOP59 | main/Room_Heater_State | When enabled, backup heater can be used for room heating (0=disabled, 1=enabled)
TOP60 | main/Internal_Heater_State | Internal backup heater state (0=inactive, 1=active)
TOP61 | main/External_Heater_State | External backup/booster heater state (0=inactive, 1=active)
TOP62 | main/Fan1_Motor_Speed | Fan 1 Motor rotation speed (R/Min)
TOP63 | main/Fan2_Motor_Speed | Fan 2 Motor rotation speed (R/Min)
TOP64 | main/High_Pressure | High Pressure (Kgf/Cm2)
TOP65 | main/Pump_Speed | Pump Rotation Speed (R/Min)
TOP66 | main/Low_Pressure | Low Pressure (Kgf/Cm2). Always 0 on the tested M-series unit (byte 164 stays 0x01, also with the compressor running; the Aquarea Service Cloud shows no low pressure either)
TOP67 | main/Compressor_Current | Compressor/Outdoor unit Current (Ampere)
TOP68 | main/Force_Heater_State | Force heater status (0=inactive, 1=active)
TOP69 | main/Sterilization_State | Sterilisation State (0=inactive, 1=active)
TOP70 | main/Sterilization_Temp | Sterilisation Temperature (°C)
TOP71 | main/Sterilization_Max_Time | Sterilisation maximum time (minutes)
TOP72 | main/Z1_Cool_Curve_Target_High_Temp | Target temperature at highest point on the cooling curve (°C)
TOP73 | main/Z1_Cool_Curve_Target_Low_Temp | Target temperature at lowest point on the cooling curve (°C)
TOP74 | main/Z1_Cool_Curve_Outside_High_Temp | Highest outside temperature on the cooling curve (°C)
TOP75 | main/Z1_Cool_Curve_Outside_Low_Temp | Lowest outside temperature on the cooling curve (°C)
TOP76 | main/Heating_Mode | Compensation / Direct mode for heat (0 = compensation curve, 1 = direct)
TOP77 | main/Heating_Off_Outdoor_Temp | Above this outdoor temperature all heating is turned off(5 to 35 °C)
TOP78 | main/Heater_On_Outdoor_Temp | Below this temperature the backup heater is allowed to be used by heatpump heating logic(-15 to 20 °C)
TOP79 | main/Heat_To_Cool_Temp | Outdoor temperature to switch from heat to cool mode when in auto setting(°C)
TOP80 | main/Cool_To_Heat_Temp | Outdoor temperature to switch from cool to heat mode when in auto setting (°C)
TOP81 | main/Cooling_Mode | Compensation / Direct mode for cool (0 = compensation curve, 1 = direct)
TOP82 | main/Z2_Heat_Curve_Target_High_Temp | Target temperature at lowest point on the heating curve (°C)
TOP83 | main/Z2_Heat_Curve_Target_Low_Temp | Target temperature at highest point on the heating curve (°C)
TOP84 | main/Z2_Heat_Curve_Outside_High_Temp | Lowest outside temperature on the heating curve (°C)
TOP85 | main/Z2_Heat_Curve_Outside_Low_Temp | Highest outside temperature on the heating curve (°C)
TOP86 | main/Z2_Cool_Curve_Target_High_Temp | Target temperature at highest point on the cooling curve (°C)
TOP87 | main/Z2_Cool_Curve_Target_Low_Temp | Target temperature at lowest point on the cooling curve (°C)
TOP88 | main/Z2_Cool_Curve_Outside_High_Temp | Highest outside temperature on the cooling curve (°C)
TOP89 | main/Z2_Cool_Curve_Outside_Low_Temp | Lowest outside temperature on the cooling curve (°C)
TOP90 | main/Room_Heater_Operations_Hours | Electric heater operating time for Room (Hour)
TOP91 | main/DHW_Heater_Operations_Hours | Electric heater operating time for DHW (Hour)
TOP92 | main/Heat_Pump_Model | Heat pump model, all values in HeatPumpType.md
TOP93 | main/Pump_Duty | Pump duty
TOP94 | main/Zones_State | Zones state (0 = zone1 active, 1 = zone2 active, 2 = zone1 and zone2 active)
TOP95 | main/Max_Pump_Duty | Max pump duty configured
TOP96 | main/Heater_Delay_Time | Heater delay time (J-series only)
TOP97 | main/Heater_Start_Delta | Heater start delta (J-series only)
TOP98 | main/Heater_Stop_Delta | Heater stop delta (J-series only)
TOP99 | main/Buffer_Installed | Buffer tank installed
TOP100 | main/DHW_Installed | DHW tank installed
TOP101 | main/Solar_Mode | Solar mode (disabled, to buffer, to DHW)
TOP102 | main/Solar_On_Delta | Solar heating delta on
TOP103 | main/Solar_Off_Delta | solar heating delta off
TOP104 | main/Solar_Frost_Protection | Solar frost protection temp
TOP105 | main/Solar_High_Limit | Solar max temp limit
TOP106 | main/Pump_Flowrate_Mode | Settings for pump flow rate (0=DeltaT, 1=Maximum flow, J-series only)
TOP107 | main/Liquid_Type | Type of liquid in settings (Water / Glycol)
TOP108 | main/Alt_External_Sensor | If external outdoor sensor is selected
TOP109 | main/Anti_Freeze_Mode | Is anti freeze mode enabled or disabled
TOP110 | main/Optional_PCB | If the optional PCB is enabled (if installed)
TOP111 | main/Z1_Sensor_Settings | Setting of the sensor for zone 1 (0=water, 1=ext thermostat, 2=int. thermostat, 3=thermistor)
TOP112 | main/Z2_Sensor_Settings | Setting of the sensor for zone 2 (0=water, 1=ext thermostat, 2=int. thermostat, 3=thermistor)
TOP113 | main/Buffer_Tank_Delta | Delta of buffer tank setting in Kelvin
TOP114 | main/External_Pad_Heater | If the external pad heater is enabled (if installed)
TOP115 | main/Water_Pressure | Water Pressure in bar (K/L series)
TOP116 | main/Second_Inlet_Temp | Water Inlet 2 Temperature(K/L series)
TOP117 | main/Economizer_Outlet_Temp | Economizer Outlet Temperature (K/L series)
TOP118 | main/Second_Room_Thermostat_Temp | Remote control 2 thermostat temp (°C)
TOP119 | main/External_Control | Is the external control switch enabled
TOP120 | main/External_Heat_Cool_Control | Is the heat/cool control switch enabled (optional pcb setting)
TOP121 | main/External_Error_Signal | Is the external error signal enabled
TOP122 | main/External_Compressor_Control | Is the external compressor control enabled (optional pcb setting)
TOP123 | main/Z2_Pump_State | Zone 2 Pump State
TOP124 | main/Z1_Pump_State | Zone 1 Pump State
TOP125 | main/TwoWay_Valve_State | 2-Way Valve State
TOP126 | main/ThreeWay_Valve_State2 | 3-Way Valve State (2nd definition)
TOP127 | main/Z1_Valve_PID | PID Value for Zone 1 mixing valve
TOP128 | main/Z2_Valve_PID | PID Value for Zone 2 mixing valve
TOP129 | main/Bivalent_Control | Bivalent control (0=disable, 1=enable)
TOP130 | main/Bivalent_Mode	 | Bivalent mode (0=Alternative, 1=Parallel, 2=Advanced Parallel)
TOP131 | main/Bivalent_Start_Temp		 | Bivalent start temperature
TOP132 | main/Bivalent_Advanced_Heat	 | Bivalent adv. par. heat control (disable/enable)
TOP133 | main/Bivalent_Advanced_DHW	 	 | Bivalent adv. par. DHW control (disable/enable)
TOP134 | main/Bivalent_Advanced_Start_Temp	 | Bivalent adv. par. heat start temp
TOP135 | main/Bivalent_Advanced_Stop_Temp	 | Bivalent adv. par. heat stop temp
TOP136 | main/Bivalent_Advanced_Start_Delay	 | Bivalent adv. par. heat start delay
TOP137 | main/Bivalent_Advanced_Stop_Delay	 | Bivalent adv. par. heat stop delay
TOP138 | main/Bivalent_Advanced_DHW_Delay	 | Bivalent adv. par. DHW delay
TOP139 | main/Heating_Control | Heating Control
TOP140 | main/Smart_DHW | Smart DHW
TOP141 | main/Quiet_Mode_Priority | Quiet Mode Priority (0=sound, 1=capacity)
TOP142 | main/Expansion_Valve | Expansion Valve (Steps)
TOP143 | main/DHW_Sensor_Selection | DHW tank sensor selection (0=Top, 1=Center) (K/L series All-In-One only)
TOP144 | main/Dry_Concrete_State | Concrete drying (0=off, 1=on)
TOP145 | main/DHW_Capacity_Mode | DHW capacity (0=Standard, 1=Variable; J series)
TOP146 | main/Zone_Configuration | Zone configuration (decimal raw code: 21=one zone, Z1 room; 25=one zone, Z1 pool; 22=two zones, Z2 room; 38=two zones, Z2 pool)
TOP147 | main/Internal_Heater_Capacity | Configured internal heater capacity (3/6/9 kW; -1=unavailable)
TOP148 | main/DHW_Heater_Type | DHW heater type (0=internal, 1=external)
TOP149 | main/SG_Ready_Control | SG Ready control (0=disabled, 1=enabled, capacity, 2=enabled, power consumption)
TOP150 | main/Demand_Control | Demand control (0=disabled, 1=enabled)
TOP151 | main/DHW_Defrost | DHW defrost option (0=no, 1=yes; K/L series)
TOP152 | main/Dry_Concrete_Target_Temp | Current stage target temperature for concrete drying (°C)
TOP153 | main/Pool_Delta | Pool temperature delta (K)
TOP154 | main/SG_Ready_1_Heat_Capacity | SG Ready setting 1 heating capacity (%)
TOP155 | main/SG_Ready_1_DHW_Capacity | SG Ready setting 1 DHW capacity (%)
TOP156 | main/SG_Ready_2_Heat_Capacity | SG Ready setting 2 heating capacity (%)
TOP157 | main/SG_Ready_2_DHW_Capacity | SG Ready setting 2 DHW capacity (%)
TOP158 | main/Room_Operation_Max_Time | Maximum room operation time during alternating room/DHW operation (minutes)
TOP159 | main/DHW_Heatup_Max_Time | Maximum DHW heat-up time (minutes)
TOP160 | main/Quiet_Mode_Actual_State | Actual quiet mode state (0=off, 1=on)
TOP161 | main/Powerful_Mode_Actual_State | Actual powerful mode state (0=off, 1=on)
TOP162 | main/Heat_Cool_Switch_State | Actual heat/cool switch state (0=heat, 1=cool)
TOP163 | main/External_Switch_State | Actual external switch state (0=closed, 1=open)
TOP164 | main/Bivalent_Boiler_Contact_State | Actual bivalent boiler contact state (0=off, 1=on)
TOP165 | main/Z2_Active | Actual zone 2 activity (0=inactive, 1=active)
TOP166 | main/Z1_Active | Actual zone 1 activity (0=inactive, 1=active)
TOP167 | main/Backup_Heater_Enable | Backup heater custom-menu setting (0=inactive, 1=active)
TOP168 | main/Cooling_Enable | Cooling custom-menu setting (0=inactive, 1=active)
TOP169 | main/Heat_Request_Temp_With_Buffer | Heating water shift/direct target including buffer delta (°C)
TOP170 | main/Static_Pressure | Static pressure option (0=disabled, 1=enabled; installer menu item "Static pressure", byte 31 bits 5-4)
TOP171 | main/Heating_On_Outdoor_Temp | Outdoor temperature for heating ON (°C; installer menu, byte 108)
TOP172 | main/Heating_On_Delay | Heating ON delay time (minutes, multiples of 30; byte 109)
TOP173 | main/DHW_Booster_Delay | Switch-on delay of the external DHW booster heater (minutes; byte 60, only meaningful with DHW tank and external heater)
TOP174 | main/Efficiency_Stage1_Time | Heating control "efficiency" stage 1 duration (minutes, multiples of 5; byte 33 low nibble)
TOP175 | main/Efficiency_Stage2_Time | Heating control "efficiency" stage 2 duration (minutes, multiples of 5; byte 33 high nibble)
TOP176 | main/Efficiency_Stage3_Time | Heating control "efficiency" stage 3 duration (minutes, multiples of 5; byte 34 low nibble)
TOP177 | main/Sensor_Offset_Remote_Th1 | Sensor calibration offset Th1 (remote), K in steps of 0.5 (byte 35 high nibble)
TOP178 | main/Sensor_Offset_Outdoor | Sensor calibration offset outdoor sensor, K in steps of 0.5 (byte 35 low nibble)
TOP179 | main/Anti_Stick_Mode | Anti-stick mode, actuator exercise (0=disabled, 1=enabled; byte 34 bits 7-6). Read-only: the heat pump ignores writes to it, it can only be changed at the remote controller custom menu
TOP180 | main/Bivalent_Operating_Mode | Bivalent operating mode, cloud field "operating mode" (0=auto, 1=SG ready, 2=intelligent; byte 27 bits 5-4). Not the switching behaviour of TOP130
TOP181 | main/Bivalent_External_Pump | Bivalent external pump, cloud field "external pump" (0=off, 1=on; byte 27 bits 7-6)
TOP182 | main/Extra_Pump_Use | Extra pump use (0=not used, 1=heating, 2=DHW; byte 31 bits 1-0, derived from the values 0x55, 0x5A, 0x5B, 0x57, see below)
TOP183 | main/Extra_Pump_Mode | Extra pump mode (0=comfort, 1=efficiency; byte 31 bits 3-2, derived like TOP182). Only meaningful when TOP182 is 2 (DHW); 0 is also reported while the pump is not used
TOP184 | main/SG_Ready_1_DHW_Power | SG ready power consumption setting 1, DHW (kW, 0.1 steps; byte 51). Label from the cloud, see below
TOP185 | main/SG_Ready_1_Heat_Power | SG ready power consumption setting 1, heating (kW; byte 52)
TOP186 | main/SG_Ready_1_Cool_Power | SG ready power consumption setting 1, cooling (kW; byte 53)
TOP187 | main/SG_Ready_2_DHW_Power | SG ready power consumption setting 2, DHW (kW; byte 54)
TOP188 | main/SG_Ready_2_Heat_Power | SG ready power consumption setting 2, heating (kW; byte 55)
TOP189 | main/SG_Ready_2_Cool_Power | SG ready power consumption setting 2, cooling (kW; byte 56, label by elimination)
TOP190 | main/SG_Ready_Stop_Power | SG ready consumption limit while the heat pump is stopped (kW; byte 57)
TOP191 | main/SG_Ready_1_Cool_Capacity | SG ready capacity setting 1, cooling offset (°C; byte 102)
TOP192 | main/SG_Ready_2_Cool_Capacity | SG ready capacity setting 2, cooling offset (°C; byte 103)
TOP193 | main/Sensor_Offset_Return | Sensor calibration offset return sensor, K in steps of 0.5 (byte 36 high nibble)
TOP194 | main/Defrost_Signal | Defrost signal, installer menu System setup (0=no, 1=yes; byte 28 bits 7-6). Shares one terminal with bivalent control (TOP129), see SET102
TOP195 | main/Sensor_Offset_DHW_Tank | Sensor calibration offset DHW tank sensor, K in steps of 0.5 (byte 36 low nibble)
TOP196 | main/Sensor_Offset_Flow | Sensor calibration offset flow sensor, K in steps of 0.5 (byte 37 low nibble)

The relay states are published on change and after each MQTT (re)connect. Commands go to `gpio/relay/one` and `gpio/relay/two` (without `/state`). The extra GPIOs use the same layout: commands to `gpio/extra/N`, states (retained) on `gpio/extra/N/state`. After a reboot the outputs are restored once from their retained `/state` topics; see the README section *GPIO states and restore at boot*.

The topics above are relative to the configured MQTT base topic. New topics TOP144–TOP196 use the documented normal data block (0x10); availability depends on the heat pump model and configuration. Two-bit status fields use -1 for an unset encoding (00). Z1_Active and Z2_Active are individual activity bits, distinct from Zones_State. Quiet/Powerful actual states are distinct from the configured level/time topics.




All Topics related with state can have also value -1 - unknown - but only in abnormal situations.

## Extra Sensor Topics:
Some heatpumps (K/L series and newer) send an additional "extra data block" on top of the normal data block. HeishaMon detects this automatically at boot and, if available, starts requesting and decoding it as well. These extra topics provide separate, more precise power consumption/production values (Watt) per mode, published under the `extra/` topic prefix. If your heatpump does not support the extra data block, these topics will not be published.

**Warning:** on heatpumps that do support the extra data block, the heatpump stops populating the "old" power topics listed above (TOP15 Heat_Power_Production, TOP16 Heat_Power_Consumption, TOP38 Cool_Power_Production, TOP39 Cool_Power_Consumption, TOP40 DHW_Power_Production, TOP41 DHW_Power_Consumption). Those will then report bogus/invalid values (e.g. -200). In that case, ignore those topics and use the XTOP equivalents below instead.

ID | Topic | Response/Description
:--- | --- | ---
XTOP0 | extra/Heat_Power_Consumption_Extra | Electrical heat power consumption (Watt)
XTOP1 | extra/Cool_Power_Consumption_Extra | Electrical cooling power consumption (Watt)
XTOP2 | extra/DHW_Power_Consumption_Extra | Electrical DHW power consumption (Watt)
XTOP3 | extra/Heat_Power_Production_Extra | Thermal heat power production (Watt)
XTOP4 | extra/Cool_Power_Production_Extra | Thermal cooling power production (Watt)
XTOP5 | extra/DHW_Power_Production_Extra | Thermal DHW power production (Watt)

## Option PCB Topics:
The following topics are actions from the heatpump to the optional pcb (for example, start pump on zone 2). This is only available if you have enable optional pcb emulation.
These values are not visible if you have the real optional pcb installed.

ID | Topic | Response/Description
:--- | --- | ---
OPT0 | optional/Z1_Water_Pump | Z1 water pump action request (0=off, 1=on)
OPT1 | optional/Z1_Mixing_Valve | Z1 mixing valve action request (0=off, 1=decrease, 2=increase)
OPT2 | optional/Z2_Water_Pump | Z2 water pump action request (0=off, 1=on)
OPT3 | optional/Z2_Mixing_Valve | Z2 mixing valve action request (0=off, 1=decrease, 2=increase)
OPT4 | optional/Pool_Water_Pump | Pool water pump action request (0=off, 1=on)
OPT5 | optional/Solar_Water_Pump | Solar water pump action request (0=off, 1=on)
OPT6 | optional/Alarm_State | Alarm state (0=off, 1=on)

## Command Topics:

These topics are commands through heishamon to set modes and values on the heatpump and they can be set by either using:

MQTT: send mqtt message to base_topic/commands/SetTopic (e.g.: panasonic_heat_pump/commands/SetHeatpump)

HTTP REST API: http://x.x.x.x/command?[topic]=[value]&[topic]=[value] (e.g.: http://x.x.x.x/command?SetQuietMode=3&SetZ1HeatRequestTemperature=21_

 ID |Topic | Description | Value/Range
:--- | :--- | --- | ---
SET1  | SetHeatpump | Set heatpump on or off | 0=off, 1=on
SET2  | SetHolidayMode | Set holiday mode on or off | 0=off, 1=on
SET3  | SetQuietMode | Set quiet mode level | 0=off, 1=less power, 2=even less power, 3=least power
SET4  | SetPowerfulMode | Set powerful mode run time in minutes | 0=off, 1=30, 2=60 or 3=90
SET5  | SetZ1HeatRequestTemperature | Set Z1 heat shift or direct heat temperature | -5 to 5 or 20 to max
SET6  | SetZ1CoolRequestTemperature | Set Z1 cool shift or direct cool temperature | -5 to 5 or 20 to max
SET7  | SetZ2HeatRequestTemperature | Set Z2 heat shift or direct heat temperature | -5 to 5 or 20 to max
SET8  | SetZ2CoolRequestTemperature | Set Z2 cool shift or direct cool temperature | -5 to 5 or 20 to max
SET9  | SetOperationMode | Sets operating mode | 0=Heat only, 1=Cool only, 2=Auto, 3=DHW only, 4=Heat+DHW, 5=Cool+DHW, 6=Auto+DHW
SET10 | SetForceDHW | Forces DHW (Operating mode should be set first to inlcude DWH mode (operation mode 3,4,5 or 6) to be effective. Please look at SET9 )  | 0, 1
SET11 | SetDHWTemp | Set DHW target temperature | 40 - 75
SET12 | SetForceDefrost | Forces defrost routine | 0, 1
SET13 | SetForceSterilization | Forces DHW sterilization routine | 0, 1
SET14 | SetPump | Set Water Pump to service mode, max speed | 0, 1
SET15 | SetMaxPumpDuty | Set max Water Pump duty in service menu | decimal value translate to hexadecimal in service menu <br/>64 to 254
SET16 | SetCurves | Set zones heat/cool curves | JSON document (see below)
SET17 | SetZones | Set zones to active | 0 = zone 1 active, 1 = zone2 active, 2 = zone1 and zone2 active
SET18 | SetFloorHeatDelta | Set floor heating delta in Kelvin | 1-15
SET19 | SetFloorCoolDelta | Set floor cooling delta in Kelvin | 1-15
SET20 | SetDHWHeatDelta | Set DHW heating delta in Kelvin | -12 to -2 (negative value)
SET21 | SetHeaterDelayTime | Set heater start delay time (only J-series) | in minutes
SET22 | SetHeaterStartDelta | Set heater start delta T (only J-series) | in kelvin
SET23 | SetHeaterStopDelta | Set heater stop delta T (only J-series) | in kelvin
SET24 | SetMainSchedule | Set weekly schedule | 0=off, 1=on
SET25 | SetAltExternalSensor | Set the alternative external outdoor sensor | 0=disabled, 1=enabled
SET26 | SetExternalPadHeater | Set the external pad heater | 0=disabled, 1=type-A, 2=type-B
SET27 | SetBufferDelta | Set buffer tank delta | 0 - 10
SET28 | SetBuffer | Set buffer installed. On an M-series test unit, setting 1 without a connected buffer sensor raised error H43 within 5 s; the error stayed until it was reset, also after setting 0 again (see AquareaParity.md) | 0=not installed, 1=installed
SET29 | SetHeatingOffOutdoorTemp | Set Outdoor Temperature to stop heating | 5 to 35
SET30 | SetExternalControl | Set external control switch | 0=disabled, 1=enabled
SET31 | SetExternalError | Set external error signal| 0=disabled, 1=enabled
SET32 | SetExternalCompressorControl | Set external compressor control switch | 0=disabled, 1=enabled
SET33 | SetExternalHeatCoolControl | Set external heat/cool control switch | 0=disabled, 1=enabled
SET34 | SetBivalentControl | Set bivalent control switch. Shares one terminal with the defrost signal (SetDefrostSignal): on the test unit, switching bivalent control on set the defrost signal to no, and it did not come back when bivalent control was switched off again | 0=disabled, 1=enabled
SET35 | SetBivalentMode | Set bivalent mode | 0=alternative, 1=parallel, 2=advanced parallel
SET36 | SetBivalentStartTemp | Set bivalent start temp | -15 to 35
SET37 | SetBivalentAPStartTemp | Set bivalent adv. par. start temp | -15 to 35
SET38 | SetBivalentAPStopTemp | Set bivalent adv. par. stop temp | -15 to 35
SET39 | SetHeatingControl | Set heating control | 0=comfort, 1=efficiency
SET40 | SetSmartDHW | Set SmartDHW | 0=variable, 1=standard
SET41 | SetQuietModePriority | Set Quiet Mode Priority | 0=sound, 1=capacity
SET42 | SetPumpFlowrateMode | Set Pump Flowrate Mode | 0=deltaT, 1=max. duty
SET43 | SetDHWSensorSelection | Set DHW tank sensor selection (K/L series All-In-One only) | 0=Top, 1=Center
SET44 | SetDHWHeaterState | Allow DHW backup/booster heater | 0=blocked, 1=free
SET45 | SetRoomHeaterState | Allow Room backup/booster heater | 0=blocked, 1=free
SET46 | SetHeaterOnOutdoorTemp | Outdoor temperature for heater ON | -15 to 20
SET47 | SetForceHeater | Force heater mode (emergency heating), same as the heater button on the remote. State is reported in TOP68 | 0=off, 1=on
SET48 | SetReset | Reset/confirm active heatpump fault code (e.g. H72). Equivalent to pressing "Reset" on the CZ-TAW1 remote / indoor unit panel. Writes byte 8 of the outgoing query. Clears latched errors that soft power-cycle (`SetHeatpump` 0→1) cannot clear. | 0=no action, 1=reset
SET49 | SetSterilizationTemp | Set DHW sterilization target temperature; experimental, L-series write acceptance is hardware-verified | integer 55-75 (degrees C)
SET50 | SetSterilizationMaxTime | Set DHW sterilization holding time after reaching target temperature; experimental, L-series write acceptance is hardware-verified | 5, 10, 15, ..., 60 minutes; other values are rejected without sending a command
SET51 | SetHeatToCoolTemp | Outdoor temperature for automatic heat-to-cool switching; readback TOP79 | integer -127..127 °C (wire range)
SET52 | SetCoolToHeatTemp | Outdoor temperature for automatic cool-to-heat switching; readback TOP80 | integer -127..127 °C (wire range)
SET53 | SetRoomOperationMaxTime | Maximum room heating operation time when alternating with DHW; readback TOP158 | 30..7620 minutes, multiples of 30 (wire range)
SET54 | SetDHWHeatupMaxTime | Maximum DHW heat-up time; readback TOP159 | integer 1..254 minutes (wire range)
SET55 | SetQuietModeSchedule | Enable/disable the existing quiet-mode schedule; readback TOP3 | 0=off, 1=on
SET56 | SetAntiFreezeMode | Anti freezing option; readback TOP109. Shared byte, see below | 0=off, 1=on
SET57 | SetPoolDelta | Pool delta T; readback TOP153 | integer 0..10 K (installer menu range)
SET58 | SetDemandControlEnable | Demand control option in the installer menu (not the optional PCB value of SetDemandControl); readback TOP150. Shared byte, see below | 0=disabled, 1=enabled
SET59 | SetDHWDefrost | Use tank water for defrost; readback TOP151. Shared byte, see below | 0=no, 1=yes
SET60 | SetStaticPressure | Static pressure option; readback TOP170. Shared byte, see below | 0=disabled, 1=enabled
SET61 | SetHeatingOnOutdoorTemp | Outdoor temperature for heating ON; readback TOP171. Must be below the heating OFF temperature (SetHeatingOffOutdoorTemp) as soon as the heat pump has reported it | integer 5..34 °C (menu minimum 5; the heating OFF temperature goes up to 35)
SET62 | SetHeatingOnDelay | Heating ON delay time; readback TOP172 | 30..1440 minutes (0:30 to 24:00 in the installer menu), multiples of 30
SET63 | SetDHWBoosterDelay | Switch-on delay of the external DHW booster heater; readback TOP173 | 20..180 minutes (0:20 to 3:00 in the installer menu), multiples of 5
SET64 | SetEfficiencyStage1Time | Efficiency stage 1 duration; readback TOP174. Nibble field, see below | 0..60 minutes, multiples of 5
SET65 | SetEfficiencyStage2Time | Efficiency stage 2 duration; readback TOP175. Nibble field, see below | 0..60 minutes, multiples of 5
SET66 | SetEfficiencyStage3Time | Efficiency stage 3 duration; readback TOP176. Nibble field, see below | 0..30 minutes, multiples of 5
SET67 | SetSensorOffsetRemoteTh1 | Sensor offset Th1 (remote); readback TOP177. Nibble field, see below | -3 to 3 K in steps of 0.5, e.g. `-1.5`
SET68 | SetSensorOffsetOutdoor | Sensor offset outdoor sensor; readback TOP178. Nibble field, see below | -3 to 3 K in steps of 0.5, e.g. `-1.5`
SET69 | SetBivalentAPHeat | Bivalent "heating" in advanced parallel mode; readback TOP132. Shared byte, needs SetBivalentControl = 1 and SetBivalentMode = 2 (advanced parallel) | 0=no, 1=yes
SET70 | SetBivalentOperatingMode | Bivalent operating mode; readback TOP180. Shared byte. 1 and 2 need SetBivalentControl = 1; 1 (SG ready) is refused while SG ready (SetSGReadyMode) is on; 0 (auto) is always possible | 0=auto, 1=SG ready, 2=intelligent
SET71 | SetBivalentExternalPump | Bivalent external pump; readback TOP181. Shared byte, needs SetBivalentControl = 1 and SetBivalentMode alternative (TOP130 = 0) | 0=off, 1=on
SET72 | SetBivalentAPStartDelay | Delay for the bivalent heat source start in advanced parallel mode; readback TOP136. Needs SetBivalentControl = 1. A raw write with a valid 5-minute step was kept (tested with bivalent control off); a raw write with an invalid step was reset by the pump | 5..250 minutes, multiples of 5
SET73 | SetBivalentAPStopDelay | Delay for the bivalent heat source stop in advanced parallel mode; readback TOP137. Needs SetBivalentControl = 1. A raw write with a valid 5-minute step was kept (tested with bivalent control off); a raw write with an invalid step was reset by the pump | 5..250 minutes, multiples of 5
SET74 | SetExtraPump | Extra pump use; readback TOP182. `2` (DHW) needs a connected DHW tank (TOP100 = 1), `0` and `1` do not; the guard mirrors the installer UI, the unit itself accepted the value without a tank. Evidence: cloud frames and raw write. Shared byte, see below | 0=not used, 1=heating, 2=DHW
SET75 | SetExtraPumpMode | Extra pump mode; readback TOP183. Only with the extra pump on DHW (TOP182 = 2) and a connected DHW tank. Shared byte, see below | 0=comfort, 1=efficiency
SET76 | SetInternalHeaterCapacity | Back-up heater capacity; readback TOP147. Evidence: wire-tested by raw write for 3, 6 and 9 kW. Shared byte, see below | 3, 6 or 9 (kW)
SET77 | SetSolarOnDelta | Solar connection: delta T for tank ON; readback TOP102 | integer 5..15 K (installer menu range)
SET78 | SetSolarOffDelta | Solar connection: delta T for tank OFF; readback TOP103 | integer 2..10 K (installer menu range)
SET79 | SetSolarFrostProtection | Solar connection: anti-freeze temperature; readback TOP104 (for the solar high limit, TOP105, see SetSolarHighLimit) | integer -20..10 °C (installer menu range)
SET80 | SetRoomHolidayShiftTemp | Room heating shift in holiday mode; readback TOP45 | integer -127..127 K (wire range)
SET81 | SetZoneType | Zone 1 type room / pool; readback TOP146. Only changes byte 21 between the one-zone values 0x15 (TOP146 = 21) and 0x19 (25); refused with any other value, in particular with two zones (22, 38). Evidence: wire-tested by raw write and cloud frame on a one-zone unit. The pump moves other bytes, see below | 0=room, 1=pool
SET82 | SetHeatingWaterMode | Water temperature mode for heating; readback TOP76. Shared byte 28; only bits 1-0 are sent. Evidence: wire-tested by raw write (whole byte). The pump moves curve bytes and some do not come back by themselves, see below | 0=compensation curve, 1=direct
SET83 | SetCoolingWaterMode | Water temperature mode for cooling; readback TOP81. Shared byte 28; only bits 3-2 are sent. Evidence: wire-tested by raw write (whole byte). The pump moves curve bytes and some do not come back by themselves, see below | 0=compensation curve, 1=direct
SET84 | SetLiquidType | Circulation liquid; readback TOP107. Shared byte 20. Evidence: water (b01) and glycol (b10) wire-tested by raw write | 0=water, 1=glycol
SET85 | SetDHWHeaterType | Tank heater type; readback TOP148. Needs a connected DHW tank (TOP100 = 1). Shared byte 25. Evidence: raw write and cloud frame; pump may discard the value without a tank | 0=internal, 1=external
SET86 | SetDHWInstalled | DHW tank connection; readback TOP100. Shared byte 24, the capacity bits are kept. Refused with 0 while the extra pump is set to DHW. Evidence: wire-tested by raw write and cloud frame, the cloud also writes byte 30 | 0=no, 1=yes
SET87 | SetSolarMode | Solar connection mode; readback TOP101. On an M-series test unit, setting 1 or 2 without a connected solar sensor while the heat pump was running raised error H28 within 5 s and switched the heat pump off; the error stayed until it was reset (see AquareaParity.md). Shared byte 24. Evidence: DHW (b11) cloud frame and raw write; buffer (b10) raw write only, label from the decode table | 0=disabled, 1=buffer, 2=DHW
SET88 | SetSolarHighLimit | Solar connection: tank high limit; readback TOP105. Evidence: raw write accepted, but the pump reverted it after about 5 s (pump may discard); on the test unit, which has no solar installation, the value was never taken over. Refused without a data block or while byte 64 reads 0 | integer 70..90 °C (installer menu range)
SET89 | SetSGReadyMode | SG ready; readback TOP149 (shows 2 for power consumption). Shared byte 27. Switching on is refused only while bivalent control is on (SetBivalentControl = 1) and the bivalent operating mode is SG ready (TOP180 = 1); switching off is always possible. Evidence: wire-tested by raw write, mode names from the cloud labels | 0=off, 1=on, capacity, 2=on, power consumption
SET90 | SetSGReadyDHWPower1 | SG ready power consumption setting 1, DHW; readback TOP184. Needs SetSGReadyMode = 2. Evidence: raw write accepted and cloud frame, label from the cloud | 0.5..10.0 kW in steps of 0.1
SET91 | SetSGReadyHeatPower1 | SG ready power consumption setting 1, heating; readback TOP185. Needs SetSGReadyMode = 2. Evidence: wire-tested by raw write (bytes 52-55 in one frame), label from the cloud | 0.5..10.0 kW in steps of 0.1
SET92 | SetSGReadyCoolPower1 | SG ready power consumption setting 1, cooling; readback TOP186. Needs SetSGReadyMode = 2. Evidence: wire-tested by raw write (bytes 52-55 in one frame), label from the cloud | 0.5..10.0 kW in steps of 0.1
SET93 | SetSGReadyDHWPower2 | SG ready power consumption setting 2, DHW; readback TOP187. Needs SetSGReadyMode = 2. Evidence: wire-tested by raw write (bytes 52-55 in one frame), label from the cloud | 0.5..10.0 kW in steps of 0.1
SET94 | SetSGReadyHeatPower2 | SG ready power consumption setting 2, heating; readback TOP188. Needs SetSGReadyMode = 2. Evidence: wire-tested by raw write (bytes 52-55 in one frame), label from the cloud | 0.5..10.0 kW in steps of 0.1
SET95 | SetSGReadyCoolPower2 | SG ready power consumption setting 2, cooling; readback TOP189. Needs SetSGReadyMode = 2. Evidence: raw write accepted, label by elimination | 0.5..10.0 kW in steps of 0.1
SET96 | SetSGReadyStopPower | SG ready consumption limit while the heat pump is stopped; readback TOP190. Needs SetSGReadyMode = 2. Evidence: raw write accepted, label from the cloud | 0.5..10.0 kW in steps of 0.1
SET97 | SetSGReadyCoolCapacity1 | SG ready capacity setting 1, cooling offset; readback TOP191. Needs SG ready on (SetSGReadyMode 1 or 2). Evidence: raw write accepted, label from the cloud | integer -15..0 °C
SET98 | SetSGReadyCoolCapacity2 | SG ready capacity setting 2, cooling offset; readback TOP192. Needs SG ready on (SetSGReadyMode 1 or 2). Evidence: raw write accepted, label from the cloud | integer -15..0 °C
SET99 | SetSensorOffsetReturn | Sensor offset return sensor; readback TOP193. Nibble field, see below. Evidence: cloud frame only, no raw write of this nibble | -3 to 3 K in steps of 0.5, e.g. `-1.5`
SET100 | SetZ1SensorSettings | Zone 1 sensor type; readback TOP111. Only the zone 1 nibble of byte 22 is sent. Evidence: internal thermostat (2) cloud frame; 1 and 3 raw write only (3 moves other bytes); labels of 1 and 3 from the decode table | 0=water temperature, 1=external thermostat, 2=internal thermostat, 3=thermistor
SET102 | SetDefrostSignal | Defrost signal, installer menu System setup; readback TOP194. Shared byte 28; only bits 7-6 are sent. Shares one terminal with bivalent control, see below. Evidence: wire-tested by raw write; coupling with bivalent observed | 0=no, 1=yes
SET103 | SetSensorOffsetDhwTank | Sensor offset DHW tank sensor; readback TOP195. Nibble field, see below. Evidence: byte 36 low nibble confirmed as the DHW tank sensor offset by a test | -3 to 3 K in steps of 0.5, e.g. `-1.5`
SET104 | SetSensorOffsetFlow | Sensor offset flow sensor; readback TOP196. Nibble field, see below. Evidence: byte 37 low nibble confirmed as the flow sensor offset by a test | -3 to 3 K in steps of 0.5, e.g. `-1.5`

SET51–SET55 require complete integer payloads; empty, malformed, out-of-range or invalid-step values are rejected without sending a telegram. The ranges above describe the nonzero protocol encoding, not verified installer-menu limits. Use settings supported by your model; the heat pump may reject values outside its own menu range. These writes follow the documented telegram examples but have not been hardware-tested here. SetQuietModeSchedule toggles the existing schedule and does not configure its times or change the quiet level.

Example: publish `21` to `panasonic_heat_pump/commands/SetHeatToCoolTemp`, `90` to `panasonic_heat_pump/commands/SetRoomOperationMaxTime`, or `1` to `panasonic_heat_pump/commands/SetQuietModeSchedule` (replace the base topic as configured).

After SetBivalentControl or SetDefrostSignal wait for the next data block (one poll interval) before sending another command on byte 26 or byte 28 (SetBivalentMode, SetBivalentAPHeat, SetHeatingWaterMode, SetCoolingWaterMode, SetDefrostSignal): the heat pump changes the other byte by itself and a command built before the read-back would write the old value back.

TOP170–TOP196 and SET56–SET104 were worked out and tested on an M-series unit only. They may work on other series as well; this has not been tested (see AquareaParity-Testing.md).

SET56–SET104 (SET101 is not used: Modbus ID 100 is reserved for SetReset and SET numbers follow the Modbus ID + 1) are based on observations on one M-series unit (heat pump off) and on raw `SendRawValue` writes; they have not been tested on other models, where they may or may not work (see AquareaParity-Testing.md for how to check). On that M-series unit they were sent through this firmware on hardware (ESP32 board, heat pump off) and read back, except SetZoneType, SetAntiFreezeMode 0, SetDefrostSignal 1 and SetZ1SensorSettings 3, which were left out on purpose. They require complete payloads (`SetSensorOffset*` also accept one decimal `.0` or `.5`, `SetSGReady*Power` and `SetSGReadyStopPower` one decimal); empty, malformed, out-of-range or invalid-step values are rejected without sending a telegram. Where the Panasonic manuals give an installer-menu limit (SetPoolDelta, SetHeatingOnOutdoorTemp, SetHeatingOnDelay, SetDHWBoosterDelay, SetEfficiencyStage1Time, the three SetSolar* commands, SetSolarHighLimit, the SG ready limits, the sensor offsets), the range above is that menu range. Elsewhere (SetEfficiencyStage2Time, SetEfficiencyStage3Time, the bivalent delays, SetRoomHolidayShiftTemp) the manuals give no limit and the range is the one seen on the unit or the nonzero wire encoding; the heat pump may reject values outside its own menu range. The sensor offset step of 0.5 K is not stated in the manuals (it comes from the observed encoding). The back-up heater capacity accepts 3, 6 and 9 kW; the manuals list 3 / 6 / 9 kW depending on the model, a unit may offer fewer.

**Shared-byte commands (SET56, SET58–SET60, SET64–SET71, SET74–SET76, SET81–SET87, SET89, SET99, SET100, SET102–SET104):** these settings live in bytes 20, 21, 22, 24, 25, 26, 27, 28, 30, 31, 33, 34, 35, 36 and 37, which hold several fields. A zero bit pair or nibble in a command means "no change" for that field (checked on the M-series test unit for all of these bytes, see AquareaParity.md), so each command sends only its own field and leaves the rest of the byte 0, like the existing SetBivalentControl. Two commands in a row on the same byte therefore do not undo each other. A bit pair is sent as b01/b10 (b11 for three-way fields). The commands are rejected until a data block has arrived and while their own byte reads 0 (a setting the unit does not have). Checks against other settings (for example "needs a DHW tank") use the last data block: right after switching a parent setting on, a dependent command is refused until the next data block reports it.

**Stage 3 time (SET66):** the high nibble of byte 34 (anti-stick bits, read-only for the heat pump) is sent as 0, as the cloud does; a data block is still required.

**Sensor offsets and stage times (SET64–SET68, SET99, SET103, SET104):** the topics TOP174–TOP178, TOP193, TOP195 and TOP196 show the decoded nibble; on models that do not use bytes 33–37 the bytes are 0 and the topics show meaningless values (for example -5 minutes, -4.0 K), and the commands are rejected. The same holds for the other topics of this block that decode a byte which may read 0 on other models (TOP171, TOP184–TOP192: for example -128 °C, -0.1 kW); this follows the existing decode functions (for example TOP169, -128 °C), a value outside the range of the setting means that the byte is not provided.

Extra conditions (the Panasonic cloud offered the fields only in these states; the heat pump itself was not tested in the other states): SetBivalentAPHeat needs SetBivalentMode = 2 (advanced parallel), SetBivalentExternalPump needs SetBivalentMode = 0 (alternative), all bivalent sub-settings need SetBivalentControl = 1, SetExtraPump with `2` (DHW) and SetExtraPumpMode need a connected DHW tank (TOP100 = 1; this guard mirrors the installer UI, the unit itself accepted the DHW value by raw write without a tank; `0` and `1` of SetExtraPump are accepted without a tank, the cloud saved them that way), SetExtraPumpMode additionally needs TOP182 = 2. A command that does not meet its condition is rejected without sending a telegram. SetBivalentControl, SetBivalentMode and the bivalent temperatures (SET34–SET38) were not touched. On the test unit, raw-writing the bivalent switch (byte 26 = 0x5E) also changed bits 7-6 of byte 28 (defrost signal, SET102) from b10 to b01 a few seconds later, and switching bivalent off again (0x5D) did not restore them. The bivalent delays (SET72, SET73) were raw-written with an invalid step only, which the pump reset; a test with a valid 5-minute step is still to be done. Byte 70 (DHW delay, TOP138) has no command because it was not tested.

**Extra pump (byte 31):** only four complete values were observed: read 0x5A = heating/efficiency, 0x5B = DHW/efficiency, 0x57 = DHW/comfort, 0x55 = not used. The cloud wrote 0x1A, 0x1B, 0x17 and 0x15 for them (bits 7-6 sent as 0, bits 5-4 = static pressure unchanged); the use field was also raw-written (low nibble 5, A and B were accepted and read back, B without a connected tank), but not every combination was tested; the transitions not-used to DHW and comfort to efficiency were never written by the cloud. The split into "use" (bits 1-0: b01 not used, b10 heating, b11 DHW) and "mode" (bits 3-2: b01 comfort, b10 efficiency) is derived from these four values and not tested for other combinations (for example heating with comfort). The commands therefore only work while the low nibble of byte 31 reads one of these four values. SetExtraPump 0 and 1 send use and mode as the cloud did (0x5, 0xA); SetExtraPump 2 sends only the use pair (b11) and keeps the stored mode (from heating: efficiency, 0xB; from not used: comfort, 0x7); SetExtraPumpMode sends only the mode pair.

**Back-up heater capacity (SET76):** payload 3 / 6 / 9 sets bits 3-2 of byte 25 to b01 / b10 / b11. All three values were written and kept on a unit with a 9 kW heater, and the Service Cloud showed 6 kW and 9 kW for them. **Do not send a value above the heater that is installed:** what a unit with a 3 kW or 6 kW heater does with a higher value has not been tested. Only bits 3-2 of byte 25 are sent.


**SET81–SET104 (added with thinner evidence than the commands above):** each of these commands refuses without a data block, while its own byte reads 0 (the SG ready limits and cooling offsets also need byte 27 in the right state), and for values outside the ranges above. The evidence class is given per command in the table (wire-tested by raw write, cloud frame only, encoding inferred, pump may discard). All of it comes from one M-series unit with the compressor off. Settings behind the optional PCB (zone type pool, solar, SG ready) are only offered by the manuals with the optional PCB; the commands do not check for it and the heat pump may discard the value.

**Zone type (SET81):** only 0x15 <-> 0x19 of byte 21 is written, and only while the data block reports one of these two values; with any other value (two zones 0x16 / 0x26, unknown values) nothing is sent. Observed when the type was changed on a one-zone unit: the zone 1 request temperatures (bytes 38 / 39) changed from 0x80 to 0x99 and returned to 0x80 after switching back to room; the heating curve byte 75 (0xA0 -> 0xA3) and the cooling curve byte 86 (0x90 -> 0x8A) also changed. For 75 and 86 it is not known whether they return by themselves: they were restored by rewriting them from a snapshot together with byte 21, so SET81 can overwrite these two curve points; read them first. No further guard exists (owner request). SET57 (pool delta T) is the matching value for pool operation.

**Water temperature mode (SET82, SET83) - changes other settings, read this first:** payload 0 / 1 sets b01 / b10 of bits 1-0 (heating, TOP76) or bits 3-2 (cooling, TOP81) of byte 28. The two pairs were wire-tested with raw writes of the whole byte (0x95 -> 0x96 and 0x95 -> 0x99); the assignment of the pairs follows the topics. Only the pair that is set is sent; it must read b01 or b10. On the test unit the heat pump itself moved curve and request temperature bytes when the pair was changed, and some of them did not come back when the pair was written back. Heating pair (0x95 -> 0x96): bytes 38, 40 and 75-79 changed (38 = A3, 40 = A3, 75 = A3, 76 = A3, 77 = 7B, 78 = 8F, 79 = A3). After writing the pair back, 38, 40 and 79 returned by themselves within about 5 s, but 75-78 did not return to their earlier values in the roughly 3 minutes until they were rewritten by hand (75 even went to B7, the value of byte 79, instead of A0). Cooling pair (0x95 -> 0x99): bytes 39, 41, 86-88 and 90 changed (39 = 8A, 41 = 8A, 86 = 8A, 87 = 8A, 88 = 94, 90 = 8A). After writing the pair back, 39, 41 and 90 returned by themselves, but 86-88 did not return to their earlier values in the roughly 80 seconds until they were rewritten by hand (86 went to 8F first, 87 and 88 stayed at 8A and 94). It was not observed what would have happened after a longer wait, and the values are not shown to be factory values. A write of these commands can therefore overwrite the heating or cooling curve points; read the curve values (and the request temperature topics) before switching and write them back with SetCurves if they do not return. Both commands exist because the owner asked for them; they have no further guard.

**Defrost signal (SET102):** payload 0 / 1 sets bits 7-6 of byte 28 to b01 (no) / b10 (yes), only this pair is sent; it is rejected without a data block or while byte 28 reads 0. b10 is the installer menu value "Yes" (read 0x95 shown as "Yes" on the remote controller); b01 is "No" (0x55, shown as "No" on the remote controller). Raw writes of 0x95 and 0x55 were accepted. According to the service manual the defrost signal and bivalent control use the same terminal, so only one function can be active, and the heat pump resolves the conflict itself, which was seen in both directions in the data block: switching bivalent control on (byte 26 bits 1-0 = b10) set the defrost signal to no, and it did not return when bivalent control was switched off; setting the defrost signal to yes while bivalent control was on set bivalent control to no (byte 26 0x5E -> 0x5D). The command does not check bivalent control, so a write of yes switches bivalent control off, and SetBivalentControl 1 sets the defrost signal to no; read TOP129 and TOP194 afterwards.

**Circulation liquid (SET84):** water (b01) was written and read back. For glycol, b10 was raw-written and held (b11 had earlier been stored as b10); no cloud field for this setting was found. Both values were written by raw write and read back (b10 held for 45 s). The command sends b10.

**Tank heater type (SET85) and tank connection (SET86):** the heat pump discarded a written tank heater type while the tank connection was no and kept it with the tank connection set to yes (also after the tank connection was set back to no), so SET85 needs TOP100 = 1. SET86 changes bits 1-0 of byte 24 only (b01 no, b10 yes); a raw write of the tank flag alone was accepted. The cloud writes byte 30 as well when it changes the tank connection; this command does not. Byte 141, whose meaning is unknown, follows the tank flag (0x00 / 0x6C). The capacity bits 7-6 of byte 24 (SetSmartDHW) are not touched: the unit keeps a capacity that was set while a tank was connected also when the tank connection is set to no (it was seen at "standard" after saving "no" in the cloud), so the capacity does not return to "variable" by itself and TOP140 may keep showing it. Setting the tank connection to a state that does not match the installation is the installer's decision; the command only refuses 0 while the extra pump is set to DHW.

**Solar (SET87, SET88):** payload 0 / 1 / 2 sets bits 5-4 of byte 24 to b01 / b10 / b11; b11 was seen in the cloud and raw-written, b10 only raw-written. The solar high limit (byte 64, degC + 128, 70..90 per the service manual) was raw-written to 0xCF; the heat pump showed it for about 5 seconds and then reported the earlier value 0xD0 again (at that time the unit had no solar connection and byte 24 was raw-set to solar DHW, b11). The heat pump may therefore discard SET88; check TOP105 afterwards. A later raw write of byte 24 bits 5-4 = b10 (buffer) was accepted, but the value of byte 64 was again never taken over. Behaviour with a connected solar installation was not tested.

**SG ready (SET89–SET98):** byte 27 bits 3-2 (b01 off, b10 capacity, b11 power consumption) was written by the cloud and raw-written (0x55 -> 0x59 / 0x5D). The power consumption limits are (kW * 10 + 1) in bytes 51-57 and the cooling offsets degC + 128 in bytes 102 / 103. Raw writes were accepted for bytes 51, 56, 57, 102 and 103 while byte 27 was 0x5D, and bytes 52-55 were accepted in one raw frame and held (about 55 s) while byte 27 was 0x55 (SG ready off). Which byte belongs to which cloud field (DHW / heating / cooling, setting 1 / 2, stop consumption) comes from the cloud labels and from comparing cloud writes with the data block; byte 56 is the cooling field of setting 2 by elimination. The limit commands need SET89 = 2 (cooling offsets: 1 or 2), because the cloud only offered the fields in these modes; the wire evidence does not require this (bytes 52-55 were written with SG ready off), so the gate follows the UI state as a precaution. They also refuse without a data block and while their own byte reads 0. Per the manuals SG ready and the bivalent operating mode exclude each other and the unit resets the other one: the manuals only say that the bivalent "SG ready" input shares the terminal with menu 16 and that the unit resets the other setting itself (SM p.103, p.105), so the draft refuses only the combination that the manuals describe: SET89 refuses to switch on while bivalent control is on and TOP180 is 1 (SG ready), and SET70 refuses 1 (SG ready) while SG ready is on (TOP149 is 1 or 2); the bivalent operating mode "intelligent" (2) is not affected. Switching off is never refused: SET89 = 0 and SET70 = 0 work in every state in which byte 27 was read, SET70 = 0 also with bivalent control off, where a bivalent pattern left in byte 27 stays hidden (for example byte 26 = 0x5D with byte 27 = 0x65, which is what remains when bivalent control is switched off while the bivalent operating mode is SG ready; raw test C7b wrote byte 27 = 0x65 while bivalent control was on, and byte 27 was restored before bivalent control was switched off, so this combination itself was not observed). Both refusals are precautions of the draft, not requirements observed on the unit. The percentage fields of the capacity mode (TOP154-TOP157) have no command. Modbus: the kW commands take whole kW only (use MQTT for tenths), the kW topics are unscaled int16 (whole kW; the float32 registers carry the real value).

**Sensor offsets of byte 36 and byte 37 (SET99, SET103, SET104):** the nibble assignment of the return sensor (high nibble of byte 36) comes from a cloud write that changed the Th1 and the return offset in one frame and from the resulting data block; the cloud showed a system error after that write although it had reached the unit. The low nibbles of bytes 36 and 37 were raw-written (accepted); a later test confirmed that the low nibble of byte 36 is the offset of the DHW tank sensor (SET103, TOP195) and the low nibble of byte 37 the offset of the flow sensor (SET104, TOP196). Each command sends only its own nibble; the other nibble of the same byte (return sensor offset for byte 36, the nibble above the flow offset for byte 37, meaning unknown, 8 seen) is sent as 0 (no change).

**Zone 1 sensor type (SET100):** only the low nibble of byte 22 (zone 1) is written, payload 0..3 as nibble 1..4. Nibble 3 (internal thermostat) was written by the cloud; nibbles 2 and 4 were only raw-written (the name of 2 and 4 comes from the decode table of TOP111). Raw-writing nibble 4 changed the zone 1 request temperatures (bytes 38 / 39: 0x80 -> 0x94 / 0x9C), which returned by themselves after writing 1 back. Zone 2 (high nibble) has no command.


*If you operate your heatpump in water mode with direct temperature setup: topics ending xxxRequestTemperature will set the absolute target temperature.*

*But if you operature your heatpump in internal/external thermostat or thermistor mode with direct temperature, you can not use these SET15 and other topics to set the direct temperature. The direct temperature in that modes is stored in the high target compenstation curve value. So to change the requested direct temperature in those modes, use the SET16 to set this value. Maybe this weird behaviour is different for newer heatpump types. So if your heatpump works different, please feel free to updates this note in github.*

*To send Heating/Cooling Curves on topic SET16 you need to send a flattened JSON document. For example:*

```{"zone1":{"heat":{"target":{"high":35,"low":25},"outside":{"high":15,"low":-15}},"cool":{"target":{"high":35,"low":25},"outside":{"high":15,"low":-15}}},"zone2":{"heat":{"target":{"high":35,"low":25},"outside":{"high":15,"low":-15}},"cool":{"target":{"high":35,"low":25},"outside":{"high":15,"low":-15}}}}```

*The structure of the JSON document:*

```json
{
	"zone1": {
		"heat": {
			"target": {
				"high": 35,
				"low": 25
			},
			"outside": {
				"high": 15,
				"low": -15
			}
		},
		"cool": {
			"target": {
				"high": 35,
				"low": 25
			},
			"outside": {
				"high": 15,
				"low": -15
			}
		}
	},
	"zone2": {
		"heat": {
			"target": {
				"high": 35,
				"low": 25
			},
			"outside": {
				"high": 15,
				"low": -15
			}
		},
		"cool": {
			"target": {
				"high": 35,
				"low": 25
			},
			"outside": {
				"high": 15,
				"low": -15
			}
		}
	}
}
```
*But you are free to set only the value you need, for example:*

```json
{
  "zone1": {
    "heat": {
      "outside": {
        "low": -15
      }
    }
  }
}
```
