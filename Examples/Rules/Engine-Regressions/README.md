# Rules engine regressions

Not a ruleset to upload to a heat pump. `rules.txt` here is a minimal script that reproduces past bugs in the rules engine itself (`HeishaMon/src/rules/`), and `tests/` holds one scenario per bug, so `../run_tests.sh` catches a regression of the engine and not only of an example ruleset.

Run them with `../harness/harness rules.txt tests/<scenario>.txt`, or all tests with `../run_tests.sh`.

## Covered bugs

- `tests/01-constants-not-overwritten.txt` — heishamon/HeishaMon#1006: with three or more independent statements in one block, `bc_assign_slots()` counted its temporary slot numbers down into the negative range, where they collide with heap indices of constants. `$c = 1 + 1` then wrote its result over the shared literal `1`, so a following `setTimer(1, 5)` armed the wrong timer with the wrong delay, and every firing computed different values.
- `tests/02-null-literal-temp-slots.txt` — heishamon/HeishaMon#1009: a literal `NULL` in a block (`$x = NULL;`, `if #g == NULL then`) is stored as a `VNULL` heap slot in group 1, but `vm_heap_next()` counted it while it looked for the n-th free temporary slot (group 0). The allocator then gave the same temporary slot to two values, so `$c = $a / $b` loaded both operands into one slot and computed `$b / $b` = 1 instead of 0.5. The result depended on the heap layout, so it changed with the number of `#` globals.
