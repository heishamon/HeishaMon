# MQTT Topics for HeishaMon

## Availability Topic:

ID | Topic | Response
--- | --- | ---
|| LWT | Online/Offline (automatically returns to Offline if connection with the HeishaMon lost)

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
TOP15 | main/Heat_Power_Production | Thermal heat power production (Watt)
TOP16 | main/Heat_Power_Consumption | Elektrical heat power consumption at heat mode (Watt)
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
TOP38 | main/Cool_Power_Production | Thermal cooling power production (Watt)
TOP39 | main/Cool_Power_Consumption | Elektrical cooling power consumption (Watt)
TOP40 | main/DHW_Power_Production | Thermal DHW power production (Watt)
TOP41 | main/DHW_Power_Consumption | Elektrical DHW power consumption (Watt)
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
TOP66 | main/Low_Pressure | Low Pressure (Kgf/Cm2)
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
TOP111 | main/Z2_Sensor_Settings | Setting of the sensor for zone 2 (0=water, 1=ext thermostat, 2=int. thermostat or thermistor)
TOP112 | main/Z1_Sensor_Settings | Setting of the sensor for zone 1 (0=water, 1=ext thermostat, 2=int. thermostat or thermistor)
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
TOP123 | main/Z1_Pump_State | Zone 1 Pump State
TOP124 | main/Z2_Pump_State | Zone 2 Pump State
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



All Topics related with state can have also value -1 - unknown - but only in abnormal situations.

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
SET28 | SetBuffer | Set buffer installed | 0=not installed, 1=installed
SET29 | SetHeatingOffOutdoorTemp | Set Outdoor Temperature to stop heating | 5 to 35
SET30 | SetExternalControl | Set external control switch | 0=disabled, 1=enabled
SET31 | SetExternalError | Set external error signal| 0=disabled, 1=enabled
SET32 | SetExternalCompressorControl | Set external compressor control switch | 0=disabled, 1=enabled
SET33 | SetExternalHeatCoolControl | Set external heat/cool control switch | 0=disabled, 1=enabled
SET34 | SetBivalentControl | Set bivalent control switch | 0=disabled, 1=enabled
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
