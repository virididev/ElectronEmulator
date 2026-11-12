---
name: electron-profile
description: Measure and optimise the speed of an Acorn Electron program in the emulator — the built-in profiler (time per routine and per instruction, in microseconds a frame), cycle counts between breakpoints and the effect of screen modes on CPU speed. Use when an Electron game or routine is too slow, stutters, has to fit in a frame (20 ms), or when comparing two versions of a routine.
---

# Profiling

Read `electron-emulator` first. `EMU` = the helper `emu.py` from `electron-emulator`
(`python3 <skills>/electron-emulator/scripts/emu.py`, `py -3 ...` on Windows; `<skills>` is
the folder that holds this skill).
Measure first, optimise second, measure again. Use the `6502` skill for the
optimisation techniques themselves.

## Where does the time go?

```
# profile.mon   (EMU run --monitor profile.mon, from the project root)
f 100
l 0E00 build-electron/prog.bin
t CALL &E00\n
f 20           # let it settle into its main loop
pf             # start (or restart) the profile
f 50           # one second of machine time
pf off
pr 20          # 20 most expensive routines and instructions
```
```
routines, with all they call and the interrupts that come meanwhile
us a frame  share     calls  routine
   15485.7  77.6%      2265  0420
instructions, by themselves
us a frame  share     count  instruction
     921.6   4.8%      2048  E250 RTS
```
- A frame is 20 000 µs. A game's per-frame work must stay below that (minus the
  OS interrupt time) or it drops frames.
- A "routine" is a `JSR` target, inclusive of what it calls and of interrupts
  meanwhile; interrupt handlers appear as routines of their own.
- `pr 0E40 0E7F` totals the instructions in one address range (one routine or
  loop).
- Map addresses to names with the `.lbl` file.

## Exact cycles for one routine

Every stop prints `cycle N`. Break before and after:
```
b 0E40
g              # note the cycle
b 0E40         # toggle off
b 0E5A
g              # cycle difference = cost incl. memory contention
```
The emulator times every bus access: RAM costs more than ROM and, in modes 0–3,
RAM accesses wait for the video — so this count is what the real machine does.

## Electron specifics

- **Screen mode matters**: in modes 0–2 the CPU gets RAM only outside the
  displayed part of each line — about half speed; mode 3 slightly better;
  modes 4–6 much faster. Profile in the mode the program really uses.
- Code and tables in RAM are slower than in ROM; zero page accesses are one
  cycle shorter than absolute ones.
- Compare versions on the same input with the same script; keep the numbers
  (before → after) in your report to the user.
