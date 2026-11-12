---
name: electron-debug
description: Debug an Acorn Electron program in the emulator headless — breakpoints on execute/read/write, single stepping, register and memory dumps, disassembly, instruction traces and patching memory — using monitor scripts, on macOS, Linux or Windows. Use when an Electron program crashes, hangs, draws or prints the wrong thing, or a test from electron-test fails and the cause is not obvious.
---

# Debugging with the monitor

Read `electron-emulator` first. `EMU` = the helper `emu.py` from `electron-emulator`
(`python3 <skills>/electron-emulator/scripts/emu.py`, `py -3 ...` on Windows; `<skills>` is
the folder that holds this skill).
Work like a scientist: one hypothesis, one small script that confirms or
refutes it, then the next. Don't change code before you know the cause.

## 1. Reproduce

Turn the failure into a script (often the failing test itself) and run it:
`EMU run --png fail.png --monitor repro.mon`. Look at the PNG and the `?` lines.

## 2. Locate

Use the label file from `electron-asm` (`-g -Ln prog.lbl`) to map names to
addresses.

| Question | Commands |
|---|---|
| Does the code get to X? | `b X` + `g` (failure `?` if not reached) |
| Who writes this variable / screen byte? | `bw 0072` + `g` → PC and registers at the write |
| Who reads it? | `br 0072` + `g` |
| What happens step by step? | `b X`, `g`, then `s 1` repeatedly or `s 20` |
| What is around here? | `d 0E00 30`, `m 0070 008F`, `r` |
| Where did it go wild? | `tr 5000 trace.txt` + `g`/`f 1`, then read the end of the trace |
| Is it hanging? | `f 50`, then `r` and `d` — PC in a tight loop shows where |

Each stop prints `PC .. A .. X .. Y .. S .. P ..` and the next instruction.
A breakpoint stops `g`, `f` and `t`. `b`/`br`/`bw` on an address that already
has one removes it; `bc` clears all.

Trace format (cycle, address, instruction, registers after):
```
   7706925 ED8F LDY #$FF      A 08 X 07 Y FF S E1 P A4
```
Traces are large: always give a limit (`tr 2000 t.txt`), and search them with
grep/Select-String instead of reading them whole. Addresses `C000–FFFF` are the
OS; `8000–BFFF` BASIC — usually not the bug.

## 3. Test a fix without rebuilding

`w addr bytes..` patches memory (e.g. `w 0E10 EA EA` = two NOPs), `pc addr`
moves the processor. Confirm the hypothesis this way, then fix the source,
rebuild, and rerun the original script.

## Typical Electron causes

- Stack imbalance (`PHA` without `PLA`, `JSR` left by `JMP`) → `RTS` to a wrong
  address; look at `S` and the stack in `r`.
- Zero page clash with the OS/BASIC: use only `&70–&8F` from BASIC.
- Writing over the screen or over the program: `bw` on the damaged address.
- Code above HIMEM overwritten when the mode changes (mode 0–2 screen starts at
  `&3000`).
- Decimal flag left set (`SED` without `CLD`) breaks later arithmetic.
- Interrupt handler not preserving registers or not chaining to the old vector.
- BASIC assembler with one pass: forward branches wrong.
- Timing: modes 0–3 slow the CPU to about half; a loop tuned in mode 6 may not
  fit in a frame in mode 1 (see `electron-profile`).

## 4. Lock it in

Add a test for the bug (`electron-test`) that fails without the fix and passes
with it.
