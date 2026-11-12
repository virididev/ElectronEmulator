---
name: electron-run
description: Run a program in the Acorn Electron emulator headless and see the result — a BBC BASIC listing (.bas/.txt), a machine code binary (.bin), a tape (.uef/.csw) or a snapshot (.state) — by typing, loading, pressing keys and taking a PNG screenshot, on macOS, Linux or Windows. Use whenever an Electron program has to be tried out, its screen output checked, or a game driven with key presses.
---

# Running a program and looking at it

Read `electron-emulator` first. `EMU` = the helper `emu.py` from `electron-emulator`
(`python3 <skills>/electron-emulator/scripts/emu.py`, `py -3 ...` on Windows; `<skills>` is
the folder that holds this skill). Put scripts and outputs in a scratch or `build-electron/`
directory, not in the sources.

After each run **look at the PNG with the Read tool** — that is how you see
what the program printed or drew. Also check the exit status and any `?` lines.

## BASIC program

```
EMU run --png out.png --monitor run.mon prog.bas
```
```
# run.mon — prog.bas is typed in at start-up
f 100
t RUN\n
f 100
```

The listing is typed in at full speed after boot (the `t RUN\n` waits until
that typing is done). Lines must have line numbers; lower case becomes capitals.
A quick one-liner without a program: put `t PRINT 6*7\n` in the script.

If the program waits for input, type it: `t 42\n`. If it runs forever, `f n`
just runs n frames and the screenshot shows the moment it stopped.

## Machine code

```
# run.mon
f 100
l 0E00 prog.bin
t CALL &E00\n
f 10
```
`CALL` lets the final `RTS` return to BASIC. For code that never returns
(games), use `t CALL &E00\n` then `f <n>` for as long as you want to watch.

## Tape or snapshot

`EMU run --png out.png --monitor wait.mon game.uef` — the tape is inserted and
loaded with `CHAIN""` or `*RUN` automatically. Waiting:
- know the entry point → `b 0E00` then `g` (stops when the program starts);
- otherwise `f 1000`, screenshot, and more frames if it is still loading.

Without monitor: `EMU run --png out.png --frames 1500 game.uef`.

A snapshot (`.state`) puts the machine straight back into a saved state.

## Playing / pressing keys

```
f 100
t CALL &E00\n
kd Space       # hold space
f 10
ku
kd Left
f 50           # walk left for a second
ku
m 0070 0071    # where did the player end up?
```
Key names are the host keyboard's as SDL names them (`A`, `1`, `Space`,
`Return`, `Left`, `Up`, `Left_Shift`, `Left_Ctrl`, `Escape`); the keyboard is
positional. Take several screenshots by running several short scripts, each
stopping at a later frame, when an animation or sequence matters.

## Reading results without a picture

`m addr end` prints memory; let the program leave results in zero page
(`&70–&8F`) or a known buffer, so a script can check them with `c` (see
`electron-test`). `sv addr end file` saves memory for a binary comparison.

## Window (only for the user)

Started without `--headless` the emulator opens its normal window, for the
user to try the program: the program from `EMU path` with the file as argument
(macOS also: `open -a "Acorn Electron" prog.uef`). An agent stays headless.
