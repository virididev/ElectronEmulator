---
name: electron-asm
description: Assemble 6502 machine code for the Acorn Electron so the emulator can run it — with ca65/ld65 (cc65, available on macOS, Linux and Windows) and the bundled Electron linker config, or with BBC BASIC's built-in assembler when no assembler is installed. Use when creating or changing an assembly program for the Electron, setting up its build, or producing the .bin and label file that the run/test/debug skills load.
---

# Assembling for the Electron

Read `electron-emulator` first for the helper and memory map. For 6502 itself
(opcodes, addressing modes, cycles) use the general `6502` skill.

## Choose the toolchain

1. `ca65`/`cl65` on the PATH? Use it (preferred: real files, labels, macros).
   - macOS `brew install cc65` · Debian/Ubuntu `sudo apt install cc65` ·
     Fedora `sudo dnf install cc65` · Windows: the cc65 Windows snapshot from
     cc65.github.io (add its `bin` to PATH) or MSYS2 `pacman -S mingw-w64-x86_64-cc65`.
   - Ask the user before installing anything.
2. Otherwise: BBC BASIC's inline assembler inside the emulator (no tools needed),
   see below.

## ca65 build

The linker config `electron.cfg` in this skill's base directory puts
`ZEROPAGE` at `&70–&8F` (free for users) and code/data from the start address
up to `&5800`. Copy it into the project (e.g. `electron.cfg` next to the
sources) so the project builds on its own, also without these skills:

```
cl65 -t none -g -C electron.cfg --start-addr 0x0E00 \
     -o build-electron/prog.bin -Ln build-electron/prog.lbl src/prog.s
```

(On Windows cmd put it on one line; `^` instead of `\` continues a line in cmd,
`` ` `` in PowerShell.)

- `--start-addr` = load and run address. `0x0E00` is PAGE, where a BASIC
  program lives: fine for code that takes over the machine or is tested on its
  own with `CALL &E00`, but it overwrites any BASIC program (including a
  loader). To keep BASIC intact, load higher, e.g. `0x2000` (keep the end below
  the screen: `&3000` in modes 0–2, `&5800` in 4–5, `&6000` in 6).
- `-g -Ln prog.lbl` writes a label file (`al 000E02 .loop`): use it to put
  breakpoints on labels (`b 0E02`) and to read traces.
- Keep build output out of the sources (e.g. `build-electron/`), and if the
  project grows, put the command in a `Makefile` or a small Python build script
  so it works the same on all three systems.

Skeleton:

```asm
OSASCI = $FFE3
OSBYTE = $FFF4
.zeropage
ptr:    .res 2
.code
start:  ldx #0
@loop:  lda msg,x
        beq @done
        jsr OSASCI
        inx
        bne @loop
@done:  rts                 ; back to BASIC when started with CALL &E00
.rodata
msg:    .byte "HELLO", 13, 0
```

Check the result: `xxd`/`od -An -tx1` (macOS/Linux) or
`Format-Hex` (PowerShell) on the `.bin`, and the emulator's disassembler:

```
# check.mon
f 100
l 0E00 prog.bin
d 0E00 20
```

## Without an assembler: BBC BASIC inline assembler

Write a `.bas` that assembles into memory, then save the bytes with the monitor:

```basic
10 DIM code% 256
20 FOR pass%=0 TO 2 STEP 2
30 P%=&E00
40 [OPT pass%
50 .start LDX #0
60 .loop LDA msg,X:BEQ done:JSR &FFE3:INX:BNE loop
70 .done RTS
80 .msg EQUS "HELLO":EQUB 13:EQUB 0
90 ]
100 NEXT
110 PRINT ~P%
```

Two passes are required (forward references). Load it with
`EMU run --monitor asm.mon prog.bas` where `asm.mon` does `f 400`, `t RUN\n`,
`f 20`, then `sv 0E00 <end> prog.bin` with the end printed by line 110 (look at
the screenshot or `m` the area). For larger programs prefer ca65.

## Next

Run it → `electron-run`; lock in behaviour → `electron-test`.
