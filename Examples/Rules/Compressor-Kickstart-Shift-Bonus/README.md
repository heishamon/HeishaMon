# Compressor kickstart and shift bonus

A compact HeishaMon ruleset (`rules.txt`) that nudges the zone-1 heat-curve shift to change when the compressor stops and starts. It does three things on one 15-second polling tick:

1. **Shift bonus** — while the compressor runs at low load and the outlet overshoots its target, raise the shift step by step, so the pump does not switch the compressor off on overshoot. Remove the whole bonus again when the low-load condition ends.
2. **Kickstart** — while the compressor is off and the outlet has fallen well below target, raise the shift by 1 K for 20 s, so the compressor restarts earlier than the pump's own restart hysteresis would allow.
3. **Return after DHW-only** — when a DHW-only run (started by something outside this ruleset) reaches its target, restore the heat pump on/off state from before the run and switch back to heat-only.

The ruleset assumes zone 1 runs on a **heat curve**, so `@Z1_Heat_Request_Temp` is the curve shift (−5 … +5, see `MQTT-Topics.md`, TOP27). With a direct water target the `< 5` cap and the ±1 nudges make no sense.

For the general rules-engine language (sigils, event blocks, built-in functions, device-side validation/safety behavior) see the `heishamon-rules` skill (`Examples/Rules/SKILL.md`). Regression scenarios for this ruleset live in `tests/` and run off-device through the host-side harness (`../harness/`, see its README) — run them all with `../run_tests.sh`, and add a scenario when you change behavior.

## What the rules do

### Boot (`System#Boot`)

Seeds the globals and arms `timer=10` with a 40 s delay, so the first tick runs after all topic values are populated:

| Global | Meaning |
|---|---|
| `#KickStartOn` | `1` = a kickstart is allowed, `0` = locked for 310 s after a kick |
| `#StateBeforeDHW` | `@Heatpump_State` seen on the last tick outside DHW-only mode |
| `#SwitchOffTemp` | counter of consecutive overshoot ticks (0 … 11) |
| `#Bonus` | how many kelvin of shift the bonus currently adds |

### The tick (`on timer=10`, every 15 s)

`timer=10` re-arms itself at the end of every firing, so the whole controller is one polling loop.

**Shift bonus.** The tick counts as *low load* when all of these hold: no defrost, `@Heat_Power_Consumption != 0`, `@Pump_Duty < 95` and `@Discharge_Temp < 35`.

- In low load, every tick with `@Main_Outlet_Temp >= @Z1_Water_Target_Temp + 2` increments `#SwitchOffTemp`; a tick without that overshoot resets it to 0.
- When the counter reaches 11 (≈ 2¾ min of uninterrupted overshoot) and the shift is below +5, the rule sends shift + 1, increments `#Bonus` and resets the counter. Since the target follows the shift, a persisting overshoot earns another +1 after another 11 ticks.
- Outside low load (defrost, compressor stopped, higher pump duty or discharge temperature), the rule sends shift − `#Bonus` once and sets `#Bonus` to 0. With no bonus it stays silent.

**Return after DHW-only.** On every tick outside DHW-only (`@Operating_Mode_State != 3`), the rule remembers `@Heatpump_State`. In DHW-only mode with `@DHW_Temp >= @DHW_Target_Temp` and `@DHW_Power_Consumption == 0`, it sends `@SetHeatpump` with the remembered state (only if it differs) and `@SetOperationMode = 0`.

**Kickstart.** If `#KickStartOn == 1`, the primary pump runs (`@Pump_Duty != 0`), the mode is heat-only (`0`), no defrost, the compressor is off and `@Main_Outlet_Temp < @Z1_Water_Target_Temp - 3`, the rule sends shift + 1, clears `#KickStartOn` and arms:

- `timer=11` (20 s) — sends the then-current shift − 1, taking the nudge back.
- `timer=12` (310 s) — sets `#KickStartOn` back to 1.

## Known issues

These are documented, not fixed; the `tests/` scenarios `09`, `11` and `12` pin the current behavior so a fix shows up as a deliberate test change.

- **Relative undo after a kick (`11-known-issue-dropped-kick`).** `timer=11` computes "current shift − 1" instead of restoring the value from before the kick. If the pump drops the +1 command (see the skill's communication-reliability note), the shift ends up 1 K *below* where it was, and nothing corrects it.
- **Bonus can get stuck (`12-known-issue-stuck-bonus`).** If, in the same tick, the compressor stops with a bonus active *and* the outlet is already below target − 3 K, the restore (shift − bonus) and the kick (stale shift + 1) go out together, because `@Set…` writes are asynchronous. `timer=11` then leaves the old bonus in place while `#Bonus` is already 0. This needs the outlet to drop ≥ 5 K within one tick, so it is unlikely, but it is permanent until the shift is changed by hand.
- **Command budget (`09-dhw-only-return-off`).** One tick can emit up to three `@Set…` commands, and the DHW-only return re-sends `@SetHeatpump`/`@SetOperationMode` on every tick until the read-back changes. The skill recommends no more than two commands per 30 s; the desired-state reconcile pattern with a ≥ 30 s verify timer would fit better.
- **`@Discharge_Temp < 35` is a low limit.** A running compressor usually has a much higher discharge temperature, so on many units the shift bonus may rarely or never engage. Tune it to your unit before relying on the bonus.

## Design notes for extending this ruleset

- **Timer IDs `10`, `11`, `12`** are used here. `Jeisha-DHW-Radiators-Rowbuffer` also uses `timer=10`, so renumber before merging. Both `Jeisha-DHW-Radiators-Rowbuffer` and `Compressor-Short-Cycle-Guard` also write `@SetZ1HeatRequestTemperature`; combining either with this ruleset as-is makes the shift writes fight each other.
- **Read-back before relative writes.** Both nudges compute the new shift from `@Z1_Heat_Request_Temp`. Storing the pre-nudge value in a `#` global and writing absolute values would remove both shift-related known issues above.
