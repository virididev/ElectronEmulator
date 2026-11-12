---
name: electron-release
description: Package an Acorn Electron program as a cassette tape image (.uef) that loads and runs by itself, by letting the emulated Electron SAVE or *SAVE it, and verify it loads from a cold start in the emulator. Use when a BASIC or machine code program is finished and must be shipped, shared or put on a "tape", or when a loader plus code tape is needed.
---

# Releasing as a tape

Read `electron-emulator` first. `EMU` = the helper `emu.py` from `electron-emulator`
(`python3 <skills>/electron-emulator/scripts/emu.py`, `py -3 ...` on Windows; `<skills>` is
the folder that holds this skill).
Before this: the whole test suite passes (`electron-test`).

The Electron itself writes the tape: what `SAVE`/`*SAVE` sends to the cassette
becomes `NAME.uef` in `--saves <dir>` (default: the current directory). An
existing file is never overwritten — a new one gets `NAME 2.uef` — so save into
an empty output directory.

## Machine code

```
# make-tape.mon   (run with: EMU run --saves dist --monitor make-tape.mon)
f 100
l 0E00 ../build-electron/prog.bin
t *SAVE GAME E00 F80 E00\n
f 20
t \n            # answer "RECORD then RETURN"
f 1500          # let the save finish (longer for bigger programs)
```
`*SAVE name start end+1 exec`: end is exclusive (start + length), the last value
is the execution address. Check `dist/GAME.uef` exists; if not, give more frames.

## BASIC

```
# make-tape.mon   (EMU run --saves dist --monitor make-tape.mon ../src/game.bas)
f 100
t SAVE "GAME"\n
f 20
t \n
f 1500
```
The typed-in listing becomes the tokenised program on tape; it loads with
`CHAIN""`.

## Machine code with a BASIC loader

Every `SAVE`/`*SAVE` becomes its own `.uef`, so save the loader and the code
(in one script or two) and then join them into one tape, loader first:

```
# in the script: the loader, e.g. 10 MODE 1:*RUN GAMECODE
f 100
t 10 MODE 1:*RUN GAMECODE\n
t SAVE "LOADER"\n
f 20
t \n
f 800
l 0E00 ../build-electron/prog.bin
t *SAVE GAMECODE E00 F80 E00\n
f 20
t \n
f 1500
```
```
EMU uefcat dist/GAME.uef dist/LOADER.uef dist/GAMECODE.uef
```
The tape then `CHAIN`s the loader, which `*RUN`s the code from the same tape
(verified). Prefer a single machine code file unless a loader is needed.

## Verify from a cold start — always

```
# verify.mon   (EMU run --png dist/verify.png --monitor verify.mon dist/GAME.uef)
b 0E00          # entry point
g               # fails with ? if the tape never starts the program
c 0E00 ../build-electron/prog.bin   # the code arrived intact
f 100
```
Look at `verify.png`. For BASIC, `f 1000` after loading and check the screen.
Run the regression tests against the tape as well if they don't depend on `l`.

## Hand-off

Report the file, its size, the load/exec addresses, and that it was verified.
Do not commit build output or tapes unless the user asks; never put the ROMs in
a release.
