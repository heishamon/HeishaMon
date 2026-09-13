# DHW force-heater boost

A small, self-contained HeishaMon ruleset (`rules.txt`) that boosts the DHW tank with the pump's force heater / emergency heating mode (`SetForceHeater`, SET47 — the same function as the heater button on the remote controller) when the tank drops below a low threshold, using a manually-verified sequence: engage the heater in heat-only mode, wait, switch to DHW-only for a fixed run, then switch back and release the heater.

Regression scenarios for this ruleset live in `tests/` and run off-device through the host-side rules harness — run them with `../harness/harness rules.txt tests/<scenario>.txt`.

## Emergency-mode preconditions (verified on a J-series)

- **The DHW backup/booster heater must be configured for "internal" in the heat pump's own service/system-config menu.** This is a one-time, physical/service-menu setting on the pump's own controller — it is *not* exposed as a HeishaMon topic or command, so it cannot be set from `rules.txt` and must be configured by hand before relying on this ruleset. It is a different setting from `DHW_Heater_State` (TOP58)/`SetDHWHeaterState`, which is left alone by this ruleset.
- **The operating mode must be heat-only (0) before the force heater is engaged.** In any other mode the pump refuses or drops the force-heater request.
- **The heatpump must be switched off.** Emergency heating replaces normal operation; the #918 dumps show `Heatpump_State` reading `0` throughout heater operation.
- **`SetForceDHW` must not be combined with emergency mode.** The two are mutually exclusive: in a live test the pump accepted the force-heater request (TOP68 → 1) and then cleared it itself ~5 s later when a `SetForceDHW` request was sent alongside it. (See also heishamon/HeishaMon#918 for the remote-controller side of this exclusivity.)

## The verified boost sequence

Confirmed working by hand on real hardware, and what the state machine below reproduces:

1. With the heater's system config already set to "internal" (see above), engage the force heater (`@SetForceHeater = 1`) while heat-only mode is active and the heatpump is off.
2. Wait a fixed **3 minutes** — this gap is required, not incidental.
3. Switch the operating mode from heat-only (0) to **DHW-only (3)**.
4. Let it run for a fixed **10 minutes**, regardless of tank temperature.
5. Switch the operating mode back to heat-only (0) and release the force heater (`@SetForceHeater = 0`).

## What the rules do

1. **Boot (`System#Boot`)** — seeds the globals (`#fhDhwLow`, the start threshold in °C; `#fhDhwActive`, the state machine: 0 = idle/fixing preconditions, 1 = heater engaged and waiting out the fixed 3-minute pre-switch delay, 2 = DHW-only mode running the fixed 10-minute boost, 3 = winding down/restoring; `#fhModeBefore` / `#fhStateBefore`, what to restore afterwards) and arms `timer=30` with a 60 s delay, inside which all topic values are populated after a cold boot (~40 s worst case).

2. **`on timer=30`** — the sole driver of every state transition; just calls `fhdhw_step()`. Re-armed by every precondition-fixing step and by both fixed-duration waits (180 s, then 600 s) and the final cleanup step.

3. **`on @DHW_Temp`** — only calls `fhdhw_step()` while `#fhDhwActive == 0` (idle or still fixing preconditions). This is deliberate: the two fixed-duration phases (the 3-minute pre-switch wait and the 10-minute DHW-only run) must elapse in full regardless of how often the tank temperature ticks in between, so those states are only ever advanced by their own `timer=30` firing, never by an incidental `@DHW_Temp` change.

4. **`fhdhw_step()`** — the whole state machine. **Start** (idle, `@DHW_Temp` below `#fhDhwLow`) walks the preconditions one command per pass: operating mode not heat-only → remember it in `#fhModeBefore`, set mode 0, retry in 60 s; heatpump on → remember that in `#fhStateBefore`, switch it off, retry in 60 s; preconditions met → send `@SetForceHeater = 1` and arm a 180 s timer. **After 180 s**: switch to DHW-only (`@SetOperationMode = 3`) and arm a 600 s timer. **After 600 s**: switch back to heat-only (`@SetOperationMode = 0`), release the heater — but only if the pump still reports it active (`@Force_Heater_State == 1`), so no redundant command is sent when the pump or the user already cancelled it — and arm the timer once more. **Final cleanup** (one more pass): restore the remembered operating mode and heatpump state, reset to idle.

## Design notes

- **Timer ID `30`** avoids the IDs used by the other examples (`1` in Compressor-Short-Cycle-Guard, `10`/`20`–`24` in Jeisha-DHW-Radiators-Rowbuffer), so this ruleset can be merged alongside them unchanged.
- **One command per precondition pass, cleanup staged the same way.** Fixing operating mode and switching the pump off each happen in separate timer-spaced firings, and no firing ever emits more than two commands (the mode-switch/heater-release pair, and the mode/heatpump restore pair) — the heat pump's slow serial link punishes command bursts, and the pump needs to acknowledge each state change before the next one is meaningful.
- **The 3-minute and 10-minute phases are fixed durations, not conditions.** Unlike the precondition-fixing steps (which retry until the read-back confirms), these two waits always run to completion — the DHW-only phase in particular ignores tank temperature entirely, matching the manually-verified procedure.
- **`#fhModeBefore`/`#fhStateBefore` double as the "restore needed" flags.** They are only non-zero while a boost is in flight; cleanup restores them and resets them to `0`. Don't reset them in the start branch — that would clobber the values saved by a precondition pass one minute earlier.
- **Async write caveat.** `@SetForceHeater = 1` / `@SetOperationMode = ...` do not make their read-back topics reflect the new value within the same firing; every check reads values acknowledged in earlier datagrams.
- **Watch TOP68 vs TOP60 when validating.** `Force_Heater_State` (TOP68) is the accepted request; `Internal_Heater_State` (TOP60) is the actual heater relay. Whether emergency heating serves the DHW tank at all depends on the installation — on a unit without a DHW tank booster heater the pump may only heat the room circuit (see heishamon/HeishaMon#918).
