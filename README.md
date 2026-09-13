[![Join us on Slack chat room](https://img.shields.io/badge/Slack-Join%20the%20chat%20room-orange)](https://join.slack.com/t/panasonic-wemos/shared_invite/enQtODg2MDY0NjE1OTI3LTgzYjkwMzIwNTAwZTMyYzgwNDQ1Y2QxYjkwODg3NjMyN2MyM2ViMDM3Yjc3OGE3MGRiY2FkYzI4MzZiZDVkNGE)
[![Build binary](https://github.com/the78mole/HeishaMon/actions/workflows/main.yml/badge.svg)](https://github.com/the78mole/HeishaMon/actions/workflows/main.yml)


# Panasonic H, J, K & L Series Aquarea air-water heat pump protocol

This project makes it possible to read information from Panasonic Aquarea heat pump and report the data either to an MQTT server or as JSON format over HTTP.

Eine deutschsprachige [README_DE.md](README_DE.md) findest du hier. \
Een nederlandse vertaling [README_NL.md](README_NL.md) vind je hier. \
Suomen kielellä [README_FI.md](README_FI.md) luettavissa täällä.

*Help on translation to other languages is welcome.*

# Current releases
Latest release is available [here](https://github.com/Egyras/HeishaMon/releases). The ESP8266 compiled binary can be installed on a Wemos D1 mini, on the HeishaMon PCB and generally on any ESP8266 based board compatible with Wemos build settings (at least 4MB flash). You can also download the code and compile it yourself (see required libraries below). The ESP32-S3 binary is for the newer, large, version of heishamon.


# Using the software
HeishaMon is able to communicate with the Panasonic Aquarea H, J, K and L&series. [Confirmed by users types of HP you can find here](HeatPumpType.md) \
If you want to compile this image yourself be sure to use the mentioned libraries and support for a filesystem on the esp8266 so select the correct flash option in arduino ide for that.

When starting, without a configured wifi, an open-wifi-hotspot will be visible allowing you to configure your wifi network and your MQTT server. Configuration page will be located at http://192.168.4.1 . \

After configuring and booting the image will be able to read and talk to your heatpump. The GPIO13/GPIO15 connection will be used for communications so you can keep your computer/uploader connected to the board if you want. \
Serial 1 (GPIO2) can be used to connect another serial line (GND and TX from the board only) to read some debugging data.

All received data will be sent to different MQTT topics (see below for topic descriptions). There is also a 'panasonic_heat_pump/log' MQTT topic which provides debug logging and a hexdump of the received packets (if enabled in the web portal).

Some heatpumps (K/L series and newer) also send an additional "extra data block" with separate, more precise power consumption/production values. HeishaMon detects this automatically at boot and, if supported, publishes these as XTOP topics under 'panasonic_heat_pump/extra/...' (see [MQTT-Topics.md](MQTT-Topics.md) for the full list). If your heatpump doesn't support this extra data block, these topics simply won't appear. Note that on heatpumps that do support it, the "old" power topics (TOP15/16, TOP38-41, see MQTT-Topics.md) are no longer populated by the heatpump and will report bogus/invalid values (e.g. -200) — use the XTOP power values instead in that case.

You can connect a 1wire network on GPIO4 which will report in seperate MQTT topics (panasonic_heat_pump/1wire/sensorid).

The software is also able to measure Watt on a S0 port of two kWh meters. You only need to connect GPIO12 and GND to the S0 of one kWh meter and if you need a second kWh meter use GPIO14 and GND. It will report on MQTT topic panasonic_heat_pump/s0/Watt/1 and panasonic_heat_pump/s0/Watt/2 and also in the JSON output. You can replace 'Watt' in the previous topic with 'Watthour' to get consumption counter in WattHour (per mqtt message) or to 'WatthourTotal' to get the total consumption measured in WattHour. To sync the WatthourTotal with your kWh-meter, publish the correct value to MQTT to the panasonic_heat_pump/s0/WatthourTotal/1 or panasonic_heat_pump/s0/WatthourTotal/2 topic with the 'retain' option while heishamon is rebooting. Upon reboot, heishamon reads this value as the last known value to you can sync using this method.

Updating the firmware is as easy as going to the firmware menu and, after authentication with username 'admin' and password 'heisha' (or other provided during setup), uploading the binary there.

A json output of all received data (heatpump and 1wire) is available at the url http://heishamon.local/json (replace heishamon.local with the ip address of your heishamon device if MDNS is not working for you).

Within the 'integrations' folder you can find examples how to connect your automation platform to the HeishaMon.

# Rules functionality
The rules functionality allows you to control the heatpump from within the HeishaMon itself. Which makes it much more reliable then having to deal with external domotica over WiFi. When posting a new ruleset, it is immidiatly validated and when valid used. When a new ruleset is invalid it will be ignored and the old ruleset will be loaded again. You can check the console for feedback on this. If somehow a new valid ruleset crashes the HeishaMon, it will be automatically disabled the next reboot, allowing you to make changes. This prevents the HeishaMon getting into a boot loop.

The techniques used in the rule library allows you to work with very large rulesets, but best practice is to keep it below 10.000 bytes.

Notice that sending commands to the heatpump is done asynced. So, commands sent to the heatpump at the beginning of your syntax will not immediatly be reflected in the values from the heatpump later on. Therefor, heatpump values should be read from the heatpump itself instead of those based on the values you keep yourself.

## Syntax
Two general rules are that spaces are mandatory and semicolons are used as end-of-line character.

### Variables
The ruleset uses the following variable structure:

- `#`: Globals
These variables can be accessed throughout the ruleset but have to defined inside a rule block. Don't use globals for all your variables, because it will persistently use memory.

- `$`: Locals
These variables live inside a rule block. When a rule block finishes, these variables will be cleaned up, freeing any memory used.

- `@`: Heatpump parameters
These are the same as listed in the Manage Topics documentation page and as found on the HeishaMon homepage. The ruleset also follows the R/W logic as used through the MQTT and REST API. That means that the read topics differ from the write topics. So reading the heatpump state is done through `@Heatpump_State`, changing the heatpump state through `@SetHeatpump`.

- `%`: Datetime variables
These can be used for date and time based rules. Currently `%hour` (0 - 23), `%minute` (0 - 59), `%month` (1 - 12), and `day` (1 - 7)  are supported. All are plain integers. A proper NTP configuration is needed to set the correct system date and time on the HeishaMon.

- `?`: Thermostat parameters
These variables reflect parameters read from the connected thermostat when using the OpenTherm functionality. When OpenTherm is supported this documentation will be extended with more precise information. The can check the opentherm tab for the variables that can be used. The names are the same for reading and writing, but not all values support reading and/or writing. The opentherm tab also lists this.

- `ds18b20#2800000000000000`: Dallas 1-wire temperature values
Use these variables to read the temperature of the connected sensors. These values are of course readonly. The id of the sensor should be placed after the hashtag.

- `s0#watt[hour[total]]_?`: The S0 values for port 1 and 2. Replace the question mark with the port number.
For example s0#watt_1 to get watt from port 1 and s0#watthourtotal_2 to get total watthours from port 2


When a variable is called but not yet set to a value, the value will be `NULL`.

Variables can be of boolean (`1` or `0`), float (`3.14`), integer (`10`), and string type. Defining strings is done with single or double quotes.

### Events or functions
Rules are written in `event` or `function` blocks. These are blocks that are triggered when something happened; either a new heatpump or thermostat value has been received or a timer fired. Or can be used as plain functions

```
on [event] then
  [...]
end

on [name] then
  [...]
end
```

Events can be Heatpump or thermostat parameters or timers:
```
on @Heatpump_State then
  [...]
end

on ?setpoint then
  [...]
end

on timer=1 then
  [...]
end
```

When defining functions, you just name your block and then you can call it from anywhere else:
```
on foobar then
  [...]
end

on @Heatpump_State then
  foobar();
end
```

Functions can have parameters which you can call:
```
on foobar($a, $b, $c) then
  [...]

on @Heatpump_State then
  foobar(1, 2, 3);
end
```

If you call a function less values then the function takes, all other parameters will have a NULL value.

There is currently one special function that calls when the system is booted on when a new ruleset is saved:
```
on System#Boot then
  [...]
end
```

This special function can be used to initially set your globals or certain timers.

### Operators
Regular operators are supported with their standard associativity and precedence. This allows you to also use regular math.
- `&&`: And
- `||`: Or
- `==`: Equals`
- `>=`: Greater or equal then
- `>`: Greater then
- `<`: Lesser then
- `<=`: Lesser or equal then
- `-`: Minus
- `%`: Modulus
- `*`: Multiply
- `/`: Divide
- `+`: Plus
- `^`: Power

Parenthesis can be used to prioritize operators as it would work in regular math.

### Functions
- `coalesce`
Returns the first value not `NULL`. E.g., `$b = NULL; $a = coalesce($b, 1);` will return 1. This function accepts an unlimited number of arguments.

- `max`
Returns the maximum value of the input parameters.

- `min`
Returns the minimum value of the input parameters.

- `isset`
Return boolean true when the input variable is still `NULL` in any other cases it will return false.

- `round`
Rounds the input float to the nearest integer.

- `floor`
The largest integer value less than or equal to the input float.

- `ceil`
The smallest integer value greater than or equal to the input float.

- `setTimer`
Sets a timer to trigger in X seconds. The first parameter is the timer number and the second parameters the number of seconds before it fires. A timer only fires once so it has to be re-set for recurring events. When a timer triggers it will can the timer event as described above. E.g.

- `print`
Prints a value to the console.

- `concat`
Concatenates various values into a combined string. E.g.: `@SetCurves = concat('{zone1:{heat:{target:{high:', @Z1_Heat_Curve_Target_High_Temp, ',low:32}}}}');`

- `gpio`
Allows setting or getting a GPIO state. When called with a single argument, a GPIO state is returned. When called with two arguments the state of a GPIO is set. This function only sets digital pins so the state can only be 0 or 1. The two relays on the large heishamon are gpio21 and gpio47. See the example to switch them each two seconds.

```
on System#Boot then
   setTimer(10, 2);
end

on timer=10 then
   setTimer(20, 2);
   gpio(21,0);
   gpio(47,1);
end

on timer=20 then
   setTimer(10, 2);
   gpio(21,1);
   gpio(47,0);
end
```

### Conditions
The only supported conditions are `if`, `else`, and `elseif`:

```
if [condition] then
  [...]
else
  if [condition] then
    [...]
  end
end
```

```
if [condition] then
  [...]
elseif [condition] then
  if [condition] then
    [...]
  else
    [...]
  end
elseif [condition] then
  [...]
else
  [...]
end
```

### Examples
Complete, documented example rulesets live in [`Examples/Rules/`](Examples/Rules/) — each comes with a README explaining its design and a `tests/` directory of regression scenarios. The scenarios run on your PC (no device needed) through the host-side harness in [`Examples/Rules/harness/`](Examples/Rules/harness/), which compiles the real HeishaMon rules engine with plain `g++` and performs the same validation as an on-device upload, then simulates topic changes and timers while checking the commands your ruleset sends. Run all tests with `Examples/Rules/run_tests.sh`, or validate your own ruleset before uploading with `harness/harness <rules.txt> parse` — see the harness README for the scenario language.

Once the rules system is in used by more and more users, additional examples will be added to the documentation.

*Calculating WAR*
```
on calcWar($Ta1, $Tb1, $Ta2, $Tb2) then
	#maxTa = $Ta1;

	if @Outside_Temp >= $Tb1 then
		#maxTa = $Ta1;
	elseif @Outside_Temp <= $Tb2 then
		#maxTa = $Ta2;
	else
		#maxTa = $Ta1 + (($Tb1 - @Outside_Temp) * ($Ta2 - $Ta1) / ($Tb1 - $Tb2));
	end
end
```

*Thermostat setpoint*
```
on ?roomTemp then
	calcWar(32, 14, 41, -4);

	$margin = 0.25;

	if ?roomTemp > (?roomTempSet + $margin) then
		if @Heatpump_State == 1 then
			@SetHeatpump = 0;
		end
	elseif ?roomTemp < (?roomTempSet - $margin) then
		if @Heatpump_State == 0 then
			@SetHeatpump = 1;
		end
	else
		@SetZ1HeatRequestTemperature = round(#maxTa);
	end
end
```

# Factory reset
A factory reset can be performed on the web interface. If the web interface is unavailable (not wifi or other reason) you can perform a hard factory reset. With recent firwmare you just hold the boot/flash button for more than 10 seconds. With older firmware you most do a double reset. This double reset should be performed not too fast but also not too slow. Usually halve a second between both resets should do the trick. To indicate that the double reset performed a factory reset, the blue led will flash rapidly (You need to press reset again now to restart HeishaMon back to normal where a WiFi hotspot should be visible again).

# Further information
Below you can find some technical details about the project. How to build your own cables. How to build your own PCB etc.

## Connection details:
Communication can be established thru one of the two sockets: CN-CNT or CN-NMODE. If you have an existing Panasonic CZ-TAW1 WiFi interface that you want to replace with HeishaMon, it is only a matter of plugging the cable out from CZ-TAW1 and reconnecting to your HeishaMon device. However it is not possible to use HeishaMon and the original CZ-TAW1 module together as an active device. It is however possible to put HeishaMon on "Listen Only" mode which will allow HeishaMon and the original CZ-TAW1 module to co-exist. The only downside to this is that HeishaMon is unable to send commands and use the optional PCB option.

Communication parameters: TTL 5V UART 9600,8,E,1  \
 \
CN-CNT Pin-out (from top to bottom) \
1 - +5V (250mA)  \
2 - 0-5V TX (from heatpump) \
3 - 0-5V RX (to heatpump)\
4 - +12V (250mA) \
5 - GND \
 \
CN-NMODE Pin-out (from left to right) \
"Warning! As printed on the PCB, the left pin is pin 4 and right pin is pin 1. Do not count 1 to 4 from left!  \
4 - +5V (250mA)  \
3 - 0-5V TX (from heatpump) \
2 - 0-5V RX (to heatpump) \
1 - GND

HeishaMon will receive power from the Panasonic over the cable (5v power).

## Long distance connection
It it possible to connect the HeishaMon over a long distance. Up to 5 meter is working with normal cabling. For longer distances a TTL-to-RS485 configuration as show in the picture below is possible. The however requires HeishaMon to be powered externally using 5v power (for example from an USB cable).

![TTL-over-RS485 HeishaMon long distance](optional-long-distance-heishamon.png)


## Where to get connectors
[RS-Online orders](Connectors_RSO.md)

[Conrad orders](Connectors_Conrad.md)

Use some 24 AWG shielded 4-conductors cable.


## The HeishaMon hardware itself
The PCB needed to connect to the heatpump is just a ESP8266 or a ESP32 (for the Heishamon Large it uses the ESP32-S3-MINI-1-N4R2) with some level shifting to get the 5v tx/rx down down 3.3v. That is the only thing to make it work.
For the ESP8266 the Heishamon software uses the schematic as described in the [ESP8266 hardware design guide](https://documentation.espressif.com/esp8266_hardware_design_guidelines_en.pdf) figure 1-10c on page 12. The GPIO5 shown there is the enable on boot (or disabled when listen only is selected). But it is also possible to user a proper schmitt buffer instead of basic level shifting.
The rest of Heishamon software is just adding more stuff, like Dallas 1wire, Opentherm etc.

To make things easy you can order a completed PCB from some project members: \
[Lectronz shop](https://lectronz.com/products/heishamon-communication-pcb) from Igor Ybema (aka TheHogNL) based in the Netherlands

## Building the arduino image yourself
boards: \
esp8266 by esp8266 community version 3.0.2 [Arduino](https://github.com/esp8266/Arduino/releases/tag/3.0.2)

All the [libs we use](LIBSUSED.md) necessary for compiling.


## MQTT topics
[Current list of documented MQTT topics can be found here](MQTT-Topics.md)

## Communication reliability
Messages from HeishaMon to the heatpump will occasionally be dropped, especially if multiple settings are changed within a short time. It's not feasible for HeishaMon to retry all dropped messages, so users should implement their own retry logic.

The suggested retry logic is as follows:
1. After changing a heatpump setting via HeishaMon, you should wait for HeishaMon to report that the setting has been changed.
2. If the setting isn't updated after 10 seconds, set it again.

This can be implemented on the HeishaMon itself using Rules if you wish, as in this example:
```
on StopExternalControl then
  if @External_Control != 0 then
    @SetExternalControl = 0;
    settimer(9,10);
  end
end

on timer=9 then
  if @External_Control != 0 then
    @SetExternalControl = 0;
  end
end
```

## EEPROM warning
As until today we don't know how the commands sent to the heatpump are processed in the heatpump itself. Most probably a lot of commands are written to EEPROM to be stored and available after a power failure, like setting the DHW temp. An EEPROM can facility a lot of writes but there is a limit. And we don't know the limit either. So make sure you don't overload the heatpump with too many commands. Every second is way too much. Just a few per hour, per settings, should probably be fine. Anyway, an heatpump is a slow heating(cooling) device so making changes that often is probably not even going to make any sense either.

## DS18b20 1-wire support
The software also supports ds18b20 1-wire temperature sensors reading. A proper 1-wire configuration (with 4.7kohm pull-up resistor) connected to GPIO4 will be read each configured secs (minimal 5) and send at the panasonic_heat_pump/1wire/"sensor-hex-address" topic. On the pre-made boards this 4.7kohm resistor is already installed.

## Large board relay control
The newer, large, heishamon contains two onboard relays which can be switched on and off using MQTT commands. The relays can be used for any contact switching, even 230V mains (max 5A). For example to switch the 230V contacts in the heatpump for controlling the 'external thermostat', switching a pump on or off or other lower power devices. I do not recommend to use the relay as a switch for a electric heater as they use too much power. To control the relay just send a value of 1 or 0 to the MQTT topic "panasonic_heat_pump/gpio/relay/one" for relay one or "panasonic_heat_pump/gpio/relay/two" for relay two.

## Opentherm support
If your heishamon board supports opentherm the software can also be used to bridge opentherm information from a compatible thermostat to your home automation over MQTT or JSON and as mentioned above it can also be connected directly in the rules to connect opentherm information to the heatpump and back, for example to display the outside temperature from the heatpump on your opentherm thermostat. If you enable opentherm support in settings there will be a new tab visible in the web page. On that tab you will see opentherm values. Some are of type R(ead) and some are W(rite), and some are both. Read means that the thermostat can read that information from the heishamon. You provide that information over MQTT (or using the rules) by updating this value on the mqtt 'opentherm/read' topic, for example 'panasonic_heat_pump/opentherm/read/outsideTemp'. The write values are information from the thermostat, like 'roomTemp'. These are available on mqtt topic 'opentherm/write'. You can use these values to change the heatpump behaviour in anyway you want using your home automation and mqtt set-commands to heishamon on using the internal rules.

The available opentherm variables are:
### WRITE values
- chEnable which is a boolean showing if central heating shoud be enabled. This is often used when the thermostat wants to heat up your house.
- dhwEnable which is a boolean showing if the dhw heating should be enabled. Often used as a user option on the thermostat to disable DHW heating during vacation
- coolingEnable which is a boolean showing if  cooling should be enabled. Amount of cooling is request in 'coolingControl', see below.
- roomTemp is the floating point value of the measured room temp by thermostat
- roomTempSet is the floating point value of the requested room temp setpoint on the thermostat
- chSetpoint is the floating point value of the calculated water setpoint by thermostat. Opentherm thermostats try to set this chSetpoint to not overshoot the room setpoint. Could be used to set the water setpoint on the heatpump but most thermostats are too fast responding compared to how heatpumps work
- maxRelativeModulation is the amount of modulation (0-100%) the heatpump (opentherm slave) is allowed to use (see relativeModulation in READ values, which should always be equal or lower than this max)
- coolingControl is the amount of cooling (0-100%) the thermostat requests from the heatpump. Requires an opentherm thermostat with cooling support.
### READ AND WRITE values
- dhwSetpoint is the floating point value which is the current DHW setpoint by thermostat, but can also be set by heishamon to override it. Not all thermostat support this though. It should not be set higher than dhwSetUppBound, see below.
- maxTSet is the floating point value which defines the maximum water setpoint. The user can set this on the thermostat or can also set from heishamon. It should not be set higher than chSetUppBound, see below.
### READ values
- chPressure is the floating point value which defines measured water pressure of central heating provided by heishamon
- outsideTemp is the floating point value which defines measured outside temperature of central heating provided by heishamon
- inletTemp is the floating point value which defines measured water inlet temperature of central heating provided by heishamon
- outletTemp is the floating point value which defines measured water outlet temperature of central heating provided by heishamon
- dhwTemp is the floating point value which defines measured dhw temperature of central heating provided by heishamon
- relativeModulation is the amount (0-100%) of modulation the heatpump (opentherm slave) is currently running on, should always be lower or equal than the maxRelativeModulation set by the thermostat
- flameState is a boolean value (send 'true', 'on' or '1' to enable) which defines if the central heating is providing heat central
- chState is a boolean value (send 'true', 'on' or '1' to enable) which defines if the heatpump is on room/central heating mode (for example 3-way valve on room, in heating mode)
- dhwState is a boolean value (send 'true', 'on' or '1' to enable) which defines if the heatpump is on DHW mode (for example 3-way valve on dhw)
- coolingState is a boolean value (send 'true', 'on' or '1' to enable) which defines if the heatpump is on room/central cooling mode (for example 3-way valve on room, in cooling mode)
- dhwSetUppBound is a integer value from 0 to 127 which sets the max DHW temperature supported so the thermostat can not request a dhwSetpoint higher than this. Default is set to 75. To override, send a MQTT message to this topic and make it retained so heishamon receives it again after reboot.
- dhwSetLowBound is a integer value from 0 to 127 which sets the min DHW temperature supported so the thermostat can not request a dhwSetpoint lower than this. Default is set to 40. To override, send a MQTT message to this topic and make it retained so heishamon receives it again after reboot.
- chSetUppBound is a integer value from 0 to 127 which sets the max CH (heating water) temperature supported so the thermostat can not request a chSetpoint higher than this. Default is set to 65. To override, send a MQTT message to this topic and make it retained so heishamon receives it again after reboot.
- chSetLowBound is a integer value from 0 to 127 which sets the min CH (heating water) temperature supported so the thermostat can not request a chSetpoint lower than this. Default is set to 20. To override, send a MQTT message to this topic and make it retained so heishamon receives it again after reboot.

## Protocol byte decrypt info:
[Current list of documented bytes decrypted can be found here](ProtocolByteDecrypt.md)

## MQTT TLS support:
TLS support or MQTT is currently implemented only for the ESP32 version. TLS is disabled by default and can be enabled in the settings. When enabling, make sure that your MQTT broker accepts secure connection from heishamon, including setting up a PEM CA certificate (e.g. self-signed). Such a certificate will roughly look as follows (the x's being seemingly random characters... the BEGIN and END lines belong to the certificate):
-----BEGIN CERTIFICATE-----
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
===========THIS=====IS====JUST===A====PLACEHOLDER===============================
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
-----END CERTIFICATE-----
Upolad a text file (plain text with suffix pem, e.g. "CA.pem") of the certificate to heishamon in the setting screen. If the syntaxt is correct it will be stored, otherwise discarded. After having uploaded the certificate, activate TLS support and (in usual setups) switch the MQTT port to 8883. !!There will be no support for TLS problems!! Google can help you with problems. In case of problems, first try without TLS support and only after that works try to switch TLS support on.
Currently, there is no possibility to delete a once-uploaded CA certificate. So if you want to remove your certificate, simply upload the dummy/sample above.

## Integration Examples for Opensource automation systems
[Openhab2](Integrations/Openhab2)

[Home Assistant](Integrations/Home%20Assistant)

[IOBroker Manual](Integrations/ioBroker_manual)

[Domoticz](Integrations/Domoticz)
