---
name: heishamon-rules
description: How to read and write HeishaMon rules-engine scripts (rules.txt) — the sigils (@/#/$/%), event/function blocks, built-in functions, and the device-side validation/safety behavior. Use before writing or editing any HeishaMon ruleset, including the worked examples under Examples/Rules.
---

A HeishaMon ruleset is a small event-driven DSL, parsed and run by the firmware's own interpreter (`HeishaMon/src/rules/rules.cpp` in this repo). There is no on-device build/lint step for a ruleset itself — it's plain text uploaded through the device's web UI (`/rules`) or REST API. Off-device, `Examples/Rules/harness/` compiles the real engine for the host PC and runs scripted regression scenarios (each example keeps its own under `tests/`; run all with `Examples/Rules/run_tests.sh`) — validate rule changes there before uploading. The canonical, most up-to-date grammar reference lives at https://github.com/CurlyMoo/rules — check there if something below seems out of date.

## Coding Standards

### Code Style for HeishaMon Rules
- **3-space indentation** (not tabs)
- **Single quotes** for strings
- **LF line endings**
- **UTF-8 charset**

## Sigils (variable lifetime and source)

- `@Name` — an **MQTT topic** HeishaMon exposes. Read-only sensor topics (e.g. `@Outside_Temp`, `@Heatpump_State`, `@Operating_Mode_State`) reflect live heat pump state. Write-capable `@Set…` topics (e.g. `@SetHeatpump`, `@SetOperationMode`, `@SetDHWTemp`, `@SetCurves`) publish a command to the heat pump when assigned to. See `MQTT-Topics.md` / `Manage-Topics.md` in the repo root for the full topic list, and note the R/W split: reading a value and setting it go through different topic names.
- `#name` — a **persistent global**. Survives across rule firings but *not* across a reboot — re-seed these in `on System#Boot`. Names are case-sensitive (`#opMode` and `#OpMode` are different variables); pick a casing convention and stay consistent within a script.
- `$name` — a **local**, scoped to a single `on … end` block.
- `%name` — an **engine-supplied datetime variable**: `%hour` (0–23), `%minute` (0–59), `%month` (1–12), `%day` (1–7). Requires NTP to be configured correctly.
- `?name` — a **thermostat parameter** (OpenTherm), when OpenTherm support is enabled. Same names for read and write; not all are readable/writable — check the device's OpenTherm tab.
- `ds18b20#<sensor-id>` — a Dallas 1-wire temperature reading, read-only.
- `s0#watt[hour[total]]_<port>` — S0 kWh-meter values for port 1 or 2, e.g. `s0#watt_1`, `s0#watthourtotal_2`.

A variable read before it's ever set evaluates to `NULL`. Types are boolean (`1`/`0`), integer, float, or string (single- or double-quoted).

## Blocks

```
on [event] then
  ...
end
```

- `on System#Boot then … end` — runs once at firmware start and again after a new ruleset is saved. The natural place to seed `#…` defaults and arm initial timers.
- `on @<Topic> then … end` — fires whenever that MQTT topic's value changes.
- `on timer=<id> then … end` — fires when a timer armed by `setTimer(<id>, <seconds>)` elapses. Timers are one-shot; re-arm inside the handler for a recurring tick. Re-arming a pending timer replaces its remaining delay, and **`setTimer(<id>, 0)` cancels a pending timer — it never fires** (arming a non-pending timer with `0` is a no-op); use `setTimer(<id>, 1)` to make a handler run (almost) immediately. **Timer IDs are just integers you pick — don't reuse one for two unrelated purposes within a script**, or the handlers will collide.
- `on ?<param> then … end` — fires when a thermostat (OpenTherm) parameter changes.
- `on <name> then … end` plus `<name>(args…)` — a user-defined function/event, callable from anywhere else in the script. Missing call arguments become `NULL` inside the block. **Caveat (verified against the engine):** in a function block that declares `$` parameters, a top-level `@Set…` write placed directly before a built-in call (e.g. `setTimer(…)`) is executed out of order and silently dropped; the same statements inside an `if … then … end` work correctly. Prefer parameterless helper blocks that communicate through `#` globals (see the desired-state reconcile pattern under "Device-side safety behavior"), or wrap such statements in a condition.

## Operators

Standard precedence/associativity, parenthesization works as in normal math: `&& || == >= > < <= - % * / + ^`. `%` is integer modulo (don't confuse with the `%hour`-style sigil above — context disambiguates).

**`NULL` in conditions:** a bare comparison against a `NULL` variable is false (`if @X == 0 then` does not fire while `@X` is unset), but the same comparison *inside an `&&` chain* evaluates truthy (`if @X == 0 && 1 == 1 then` fires!). When combining conditions with `&&`/`||` over variables that might still be `NULL`, guard them with `isset()` first or use nested `if`s.

## Built-in functions

`coalesce(…)` (first non-`NULL` argument, unlimited args), `max(…)`, `min(…)`, `isset(x)` (`1` once `x` has a value, `0` while it is still `NULL`), `round(x)`, `floor(x)`, `ceil(x)`, `setTimer(id, seconds)` (`0` seconds cancels — see Blocks above), `print(x)`, `concat(…)` (string concatenation — handy for building a JSON-ish payload for a topic like `@SetCurves`), `gpio(pin)` / `gpio(pin, state)` (read or set a digital GPIO; on the ESP32-S3 board the two relays are `gpio(21, …)` and `gpio(47, …)`).

## Device-side safety behavior (don't route around these)

- A newly-uploaded ruleset is **validated immediately**. If invalid, it's rejected and the previous ruleset keeps running — check the console/log for why.
- If a valid-looking ruleset **crashes the device**, HeishaMon auto-disables rules on the next boot so you can recover without a boot loop. Don't try to defeat this from within a script.
- Keep rulesets under roughly 10 KB (best practice from the firmware author, not a hard limit).
- Sending a command to the heat pump is **asynchronous** — a `@Set…` write earlier in a block is not reflected in the corresponding read-topic later in the *same* firing. Always branch on the heat pump's own reported state, not on a value you just wrote.
- Commands to the heat pump can be **silently dropped**, especially several in quick succession (`README.md` → "Communication reliability"). Budget: **no more than two `@Set…` commands within 30 seconds**, total across the whole ruleset — structure state machines so a single firing emits at most two commands and later phases run on one-shot timers staggered ≥ 30 s apart (the Jeisha worked example is built around exactly this). Separately, settings are most likely persisted to the heat pump's EEPROM, which has a finite write life — a few writes per hour per setting is fine, anything approaching once per second is far too much (`README.md` → "EEPROM warning").
- For a command that must not stay dropped, use the **desired-state reconcile** pattern: keep the wanted value in a `#` global, write it from one parameterless helper block, and verify against the read-back topic on a timer, re-sending until it sticks:

  ```
  on applyHeatpump then
     @SetHeatpump = #wantHeatpump;
     setTimer(31, 30);
  end

  on timer=31 then
     if @Heatpump_State != #wantHeatpump then
        applyHeatpump();
     end
  end
  ```

  Change the setting from anywhere with `#wantHeatpump = 1; applyHeatpump();`. The retry stops by itself once the read-back matches, and the 30 s verify interval keeps even a permanently-refused command inside the two-commands-per-30-seconds budget (and safely beyond the read-back reporting latency, so it never re-sends a command that actually arrived). This is the generalized form of the retry example in `README.md` → "Communication reliability".

## Worked examples

- `Examples/Rules/Jeisha-DHW-Radiators-Rowbuffer/` contains a real multi-year-tuned ruleset (`rules.txt`) plus `README.md`, a block-by-block walkthrough of *why* it's written the way it is — DHW/defrost state machine, timer-budget discipline, pre-DHW state snapshotting, etc. Read `README.md` alongside `rules.txt` when you need to see these primitives combined into a non-trivial controller, or when extending that specific ruleset.
- `Examples/Rules/Compressor-Short-Cycle-Guard/` is a small, focused ruleset that suppresses the zone-1 heat request for a fixed window after the compressor stops (mild weather only) to reduce short-cycling, with a cold-weather override. Good minimal example of the timer-armed-suppression pattern and of using a sentinel value on an existing topic instead of a `#` global to track state.
- `Examples/Rules/Compressor-Kickstart-Shift-Bonus/` is a single 15-second polling loop that nudges the zone-1 curve shift: a step-wise bonus against overshoot stops at low load, a short +1 K kick to restart a stopped compressor early, and a return to heat-only after an externally started DHW-only run. Its README lists known issues (relative undo of a nudge, command budget) pinned by tests — a useful counter-example for the async-write and command-budget rules above.
