---
name: electron-test
description: Write and run automated regression tests for Acorn Electron programs as monitor scripts (*.mon) that the emulator executes headless and that fail with exit status 1 — memory compared with expected files, breakpoints that must be reached, typed input — plus running the whole suite on macOS, Linux, Windows and in CI. Use when adding tests for an Electron program, checking that a change did not break anything, or before committing/releasing.
---

# Tests as monitor scripts

Read `electron-emulator` first. `EMU` = the helper `emu.py` from `electron-emulator`
(`python3 <skills>/electron-emulator/scripts/emu.py`, `py -3 ...` on Windows; `<skills>` is
the folder that holds this skill).

A `.mon` script is a test: every failing check prints a line starting with `?`
and the emulator exits with 1. What fails:
- `c addr file` — memory differs from the file;
- `g` — no breakpoint reached within 3000 frames (≈ 1 min machine time);
- `t text` — the text could not be typed;
- any command that cannot be carried out (missing file, bad address).

## Layout

```
tests/
  hello.mon            # one behaviour per script
  hello.expect.bin     # expected bytes
  score_add.mon
  ...
```
Each script runs **in its own directory**, so refer to files relative to it
(`../build-electron/prog.bin`, `hello.expect.bin`). A line `#! <options>`
adds emulator options for that script, e.g. `#! --sideways-ram 16`.

Run all:
```
EMU test tests/                  # PASS/FAIL per script, exit 1 if any failed
EMU test --png-dir build-electron/test-pics tests/   # plus a screenshot of each end state
```
Rebuild (see `electron-asm`) before testing.

## Patterns

**Routine test — call a routine with known inputs, check outputs.**
Put the result in zero page or a buffer and stop on a breakpoint after it:
```
# add_bcd.mon
f 100
l 0E00 ../build-electron/prog.bin
w 0070 19 23          # inputs
pc 0E40               # address of add_bcd (from the .lbl file)
b 0E5A                # the RTS of add_bcd
g
c 0072 add_bcd.expect.bin   # expected: 42
```
Avoid letting `RTS` run after `pc` — there is no caller on the stack.

**Whole program via BASIC.**
```
f 100
l 0E00 ../build-electron/prog.bin
t CALL &E00\n
f 10
c 0070 hello.expect.bin
```

**BASIC program.** Have it write results somewhere checkable
(`?&70=S%`, `!&70=X%`). The `#!` line passes the `.bas` to the emulator, which
types it in at start-up:
```
#! ../src/sum.bas
f 100
t RUN\n
f 50
c 0070 sum.expect.bin
```

**Reaching a point at all** (e.g. "the game reaches its main loop"):
`b <mainloop>` + `g` — failure if never reached.

**Screen contents.** Save screen memory once a known-good picture is reached
(`sv 6000 7FFF screen.expect.bin` in mode 6, `3000 7FFF` in mode 0–2) and later
compare with `c`. Do this only after you have looked at the PNG and confirmed
it is right. Timing-dependent screens make brittle tests; stop at a breakpoint
rather than after `f n` where possible.

## Making expected files

Small ones by hand (Python, works everywhere):
```
python3 -c "open('tests/add_bcd.expect.bin','wb').write(bytes([0x42]))"
```
Larger ones: run once, inspect (PNG, `m`), and only if correct capture with
`sv`. Never generate expectations from a run you have not checked — that only
freezes the bug in.

## Rules

- A test must fail before the fix (or for the wrong answer) — check that once
  by breaking the expectation.
- Keep each script short and about one thing; the name says what.
- Run the whole suite before committing and before `electron-release`.

## CI

In CI the emulator has to be downloaded from its releases (Windows `.zip`,
macOS/Linux `.tar.gz`) and pointed to with `ELECTRON_EMU`, and it needs the
ROMs, which may not be distributed: they can only come from the user privately
(e.g. a secret). Without that, tests run locally only. Ask before setting up
anything like this.
