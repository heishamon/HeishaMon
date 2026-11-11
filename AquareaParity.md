# Aquarea parity: findings, evidence and open tasks

This file documents how installer-menu settings of the Panasonic Aquarea were located in the datagrams, and what was observed when they were changed. The aim is that HeishaMon can read and write what the wired remote controller and the Aquarea Service Cloud can. Topic and command reference: [MQTT-Topics.md](MQTT-Topics.md); byte positions: [ProtocolByteDecrypt.md](ProtocolByteDecrypt.md).

## 1. Scope and limits of the evidence

- One unit only: M series, WH-SDC0316M9E8 with WH-WXG09ME8, one heating circuit, no DHW tank, no buffer, no solar, optional PCB setting "yes". HeishaMon large (ESP32), firmware 4.3.1, CZ-TAW1 on the proxy port.
- Most write tests were made with the heat pump switched off. Exceptions are marked (3.4 and 4).
- Other series (H, J, K, L) were not tested. Byte positions that come only from the existing decode tables were not re-checked.

## 2. How findings were made

1. Read side: change one setting on the remote controller or in the Service Cloud, compare the 203-byte data block (`71 C8 01 10`) before and after.
2. Write side, cloud: capture the 111-byte command (`F1 6C 01 10`) the CZ-TAW1 sends through the HeishaMon proxy port.
3. Write side, raw: send a command with `SendRawValue` (`rawcmd` below builds it) that carries only the bytes under test, read the data block back after 35 to 45 s, restore the old value.
4. Where possible, confirm the meaning on the remote controller or after a reload of the Service Cloud.

## 3. Protocol findings

### 3.1 Command frame

- A data byte of 0x00 means "no change". The same holds for a zero bit pair inside a shared byte (shown for bytes 20, 24, 25, 26, 27, 28, 30, 31) and for a zero nibble (shown for bytes 22, 33, 35, 36, 37). For bytes 20, 22, 25, 28, 30 and 33 this was checked with raw writes that set one field to its current value and all other fields of the byte to 0, together with a change of byte 60 that showed the frame was taken over: the other fields stayed unchanged in every case (heat pump off).
- A command with a wrong length is ignored without an error.
- A write is visible in the data block 0.4 to 5 s after the log line `Command datagram sent bytes: 111`.
- Values outside the step size of the menu are shown for one read cycle and then reverted by the heat pump. Seen with +1 minute on bytes 67, 69, 98 and 101 (menu steps of 5 minutes) and +1 on byte 100. Valid steps are kept.
- Some writes are ignored or discarded: byte 34 bits 7-6 (anti-stick, ignored), byte 64 (solar high limit, reverted after about 5 s on a unit without solar), byte 46 (dry concrete stage temperature, not taken over), byte 100 = 0xC6.
- Bytes 120 and 122 of the data block (cooling function enabled, among others) have no position in the 110-byte command and cannot be written.

### 3.2 Encodings

| Kind | Encoding |
|---|---|
| Temperatures | °C + 128 |
| Durations in minutes | minutes + 1 |
| Sensor offsets (bytes 35 to 37) | offset / 0.5 + 8 per nibble (8 = 0 K) |
| Heating ON delay (byte 109) | minutes / 30 + 1 |
| Efficiency stage times (bytes 33, 34) | minutes / 5 + 1 per nibble |
| Two-state options | bit pair b01 = off / no, b10 = on / yes |

### 3.3 Byte assignments found or confirmed on the M series

| Byte | Bits | Setting | Evidence |
|---|---|---|---|
| 20 | 7-6 | Circulation liquid (b01 water, b10 glycol) | raw write |
| 20 | 3-2 | Anti-freeze | cloud frame, raw write; the Service Cloud shows "anti-freeze = no" for 0x66 |
| 20 | 1-0 | Optional PCB | raw write 0x69, the Service Cloud then shows "no" and greys the dependent fields |
| 24 | 7-6 / 5-4 / 3-2 / 1-0 | DHW capacity / solar mode / buffer / DHW tank | cloud frames, raw write |
| 25 | 3-2 | Back-up heater capacity (b01 3 kW, b10 6 kW, b11 9 kW) | raw write; the Service Cloud shows 6 kW for 0x29 and 9 kW for 0x2D on a unit with a 9 kW heater. Units with a smaller heater not tested |
| 26 | 1-0 / 3-2 / 5-4 | Bivalent control / switching behaviour / heating | cloud frames, raw write |
| 27 | 3-2 / 5-4 / 7-6 | SG ready / bivalent operating mode / bivalent external pump | cloud frames, raw write |
| 28 | 7-6 | Defrost signal (remote controller item 22) | raw write, read on the remote controller for both values |
| 31 | 3-0 / 5-4 | Extra pump use and mode / static pressure | cloud frames, raw write |
| 33, 34 | nibbles | Efficiency stage times | cloud frame, raw write |
| 35 | low / high | Sensor offset outdoor / remote Th1 | cloud frame, raw write |
| 36 | low / high | Sensor offset DHW tank / return | cloud frame, raw write (low nibble) |
| 37 | low | Sensor offset flow | cloud field change, raw write |
| 51 to 57, 102, 103 | | SG ready power limits and cooling offsets | raw write, labels from the cloud |
| 60 | | DHW booster delay | raw write |
| 65 to 69 | | Bivalent temperatures and delays | cloud frames; delays by raw write with a 5-minute step |
| 108, 109 | | Heating ON outdoor temperature / delay | raw write |

### 3.4 Couplings inside the heat pump

- **Defrost signal and bivalent control share one terminal.** Setting bivalent control to yes sets byte 28 bits 7-6 to "no"; setting the defrost signal to yes sets bivalent control to "no". Switching bivalent off does not restore the defrost signal.
- **Buffer tank = yes without a buffer sensor raises H43** (zone 1 sensor fault) within 5 s. The zone 1 water temperature then reads -78 °C. The fault stays latched after the buffer is set back to "no" and has to be reset. Shown twice: with all parent switches set, and with only byte 24 bits 3-2 set.
- **Solar mode on without a solar sensor raises H28** (solar sensor fault) within 5 s **while the heat pump is running**, and the heat pump switches off. The fault stays after the solar mode is set back and has to be reset (`SetReset`); the first `SetHeatpump 1` after the reset was not kept, a second one 90 s later was. With the heat pump switched off the same setting (buffer and DHW, about 30 s and 60 s) raised no fault.
- **Faults of this kind can depend on the operating state.** The tests of DHW tank, SG ready and bivalent control were made with the heat pump off and showed no fault; that says nothing about the running heat pump.
- Sensor bytes without a connected sensor on this unit: buffer, solar, pool and both zone water temperatures (bytes 149, 150, 151, 145, 146) read 0x32 (-78 °C), with the solar mode off and on. The DHW temperature (byte 141) reads 0x00 (-128 °C) with DHW tank = no and 0x6C (-20 °C) with DHW tank = yes. What these bytes show on a unit that has the sensor while the function is switched off is not known.
- Byte 22 = 0x14 and byte 28 = 0x96 / 0x99 (water temperature mode) move the heat and cool curve bytes 75 to 79 and 86 to 90; some values return by themselves, others have to be written back.

### 3.5 Parent and child settings

- The heat pump does **not** gate child values on their parent switch. With DHW tank = no it accepted bytes 42, 98, 99, 101; with bivalent = no bytes 67, 69, 70; with solar, buffer and cooling off bytes 59, 61 to 63, 94.
- Not tested: child values with optional PCB = no.
- The Service Cloud does gate. Fields greyed while the parent is off:

| Parent | Fields it unlocks |
|---|---|
| DHW tank | Capacity, tank heater, DHW defrost, extra pump; on other tabs: the DHW block of the operation settings (heating interval, charge time, re-heat delta, sterilisation), a DHW row in the weekly timer, DHW target temperature, DHW emergency mode and tank heater on the user tab |
| Bivalent | Operating mode, bivalent temperature, switching behaviour, heating yes/no. Target temperatures and delays need "heating = yes"; the external pump needs switching behaviour "alternative"; the DHW part also needs a tank |
| Solar | Delta T on / off, frost protection, high limit |
| SG ready | Both stages for DHW and heating, cooling offsets, power limits |
| Optional PCB | Zones and sensors, solar, error output, demand control, SG ready, external compressor switch, heat/cool switch, extra pump |
| Water temperature mode "compensation curve" | Greys the fixed value (heating and cooling) |

- Cloud pitfall: setting the DHW tank back to "no" does not write the capacity bits; a hidden "standard" stays in the unit (byte 24 = 0x95 instead of 0x55).

## 4. Observations on the running heat pump

- Writing a sensor offset (flow, return, DHW tank, outdoor, remote Th1) stopped the compressor about 18 s later for about 4 minutes, also when the value was written unchanged. No error code was shown. Seen with all five offsets, 7 of 7 times. Not seen with the 26 other settings that were written back unchanged or with four changed settings (heating ON delay, efficiency stage time, DHW booster delay, SG ready power limit). Whether the remote controller or the Service Cloud trigger the same, and whether other series behave the same, is not known.
- The faults in 3.4 (H43, H28) are not prevented by HeishaMon; the heat pump accepts the values (3.5). Whether to guard against them is left to the integrations.

## 5. Tools

`Tools/capture_tools.py` (Python 3, no dependencies) works on the HeishaMon log with "Hexdump" enabled:

```bash
# main data block of several captures, e.g. before / setting changed / changed back
python3 Tools/capture_tools.py diff before.log changed.log back.log
# commands the Service Cloud sent through the proxy port
python3 Tools/capture_tools.py proxy cloud.log
# build a raw command for the SendRawValue topic: byte 22 = 0x01, byte 60 = 0x56
python3 Tools/capture_tools.py rawcmd 22=01 60=56
```

A capture should cover at least a minute (a data block arrives every 5 s); `diff` ignores bytes that are not stable inside a capture (measurements). Do not use the optional PCB emulation while a physical optional PCB is installed. Note that `ProtocolByteDecrypt.md` counts bit pairs from the MSB; `diff` prints bit numbers with bit 7 as MSB.
