---
name: electron-emulator
description: Base skill for developing software for the Acorn Electron with the acornelectron emulator installed on this computer as the run, test and debug environment. Use whenever writing, running, testing, debugging, profiling or shipping a BBC BASIC or 6502 machine code program for the Electron, or when the emulator has to be found or set up first. Covers the cross-platform helper script, the command-line options, the monitor command language, the memory map and the gotchas; the other electron-* skills build on it.
---

# Acorn Electron development with the emulator

The Acorn Electron emulator (`acornelectron`) can run without a window (`--headless`) and be
driven by a script of monitor commands (`--monitor`). That makes it a complete
test bench for an agent: load a program, type at it, press keys, set
breakpoints, read registers and memory, compare memory with files, trace,
profile, save a screenshot, and get an exit status that says whether all went
as expected. Works the same on macOS, Linux and Windows.

## The development cycle — which skill when

| Phase                         | Skill               |
|-------------------------------|---------------------|
| Find / check the emulator     | this skill (below)  |
| Write & assemble machine code | `electron-asm` (plus the general `6502` skill) |
| Write BASIC                   | the general `bbcbasic` skill, then `electron-run` |
| Run it and look at the screen | `electron-run`      |
| Regression tests              | `electron-test`     |
| Find a bug                    | `electron-debug`    |
| Make it fast enough           | `electron-profile`  |
| Ship it as a tape (`.uef`)    | `electron-release`  |

Work in small steps: change → assemble → run headless → look (screenshot and/or
memory) → add a test for what now works → next step. Never claim a program works
without having run it in the emulator.

## The helper script (cross-platform)

`scripts/emu.py` in this skill's base directory (the directory this SKILL.md
is in, e.g. `~/.claude/skills/electron-emulator/scripts/emu.py`) — standard
library only. Call it with `python3` on macOS/Linux and `py -3` (or `python`)
on Windows, by its full path, from the project you are working in. Below, `EMU`
stands for that command.

```
EMU check                                 # emulator found? ROMs there? does it start?
EMU path                                  # where is the emulator?
EMU setup <program|folder|.app>           # remember a non-standard location
EMU run [--png out.png] [--cwd DIR] [options...]   # headless run (+ PNG screenshot)
EMU test [--png-dir DIR] tests/           # run every *.mon, PASS/FAIL, exit 1 on failure
EMU bmp2png in.bmp out.png
EMU uefcat out.uef a.uef b.uef            # join tapes (loader + code)
```

`run` always adds `--headless`; `--png` asks the emulator for a screenshot and
converts it to PNG so the Read tool can show it. Use the PNG to see what is on
the screen — screen memory holds pixels, not characters.

Prefer writing monitor commands to a `.mon` file over `--type '...'` on the
command line: quoting `\n`, `&`, `"` and `$` differs between zsh/bash, cmd and
PowerShell, a file does not.

## Setting up

The emulator is a program installed on this computer (`acornelectron`); the
project you work in normally does not contain it. Start every session with
`EMU check`.

1. **Finding it.** `EMU` looks, in this order, at `$ELECTRON_EMU`, a path saved
   with `EMU setup`, the PATH, and the usual places:

   | System  | Usual places |
   |---------|--------------|
   | macOS   | `/Applications/Acorn Electron.app`, `~/Applications/Acorn Electron.app` |
   | Windows | `%ProgramFiles%\acornelectron\acornelectron.exe`, `%LOCALAPPDATA%\Programs\acornelectron\`, `%USERPROFILE%\acornelectron\`, `Downloads\acornelectron\` |
   | Linux   | `~/.local/bin/acornelectron`, `/usr/local/bin`, `/opt/acornelectron/`, `~/acornelectron/` |

   (Inside the emulator's own source code it uses the fresh `build/` first.)
   If it is not found, ask the user where it is installed and run
   `EMU setup <path>` — the program, its folder or the `.app`; it is
   remembered for this computer. If it is not installed at all, the user can
   download it from the project's GitHub releases (Windows `.zip`, macOS and
   Linux `.tar.gz`); on macOS the first start may need
   `xattr -dr com.apple.quarantine "Acorn Electron.app"`. Don't install
   anything without asking.
2. **ROMs.** `os.rom` and `basic.rom` (16 kB each) must be in a folder `roms`
   next to the program (in a Mac app: `Contents/Resources/roms`) or in the
   emulator's settings folder:
   `~/Library/Application Support/Emulators/acornelectron/roms` (macOS),
   `%APPDATA%\Emulators\acornelectron\roms` (Windows),
   `~/.local/share/Emulators/acornelectron/roms` (Linux) —
   or pass `--roms <dir>`. `EMU check` says when they are missing. They are
   Acorn's and not distributed with the emulator: ask the user for them, never
   download them yourself.
3. **Smoke test.** `EMU run --png boot.png --frames 150` and look at
   `boot.png`: it should show "Acorn Electron" and "BASIC" with a `>` prompt.

## Command-line options that matter for an agent

| Option | Use |
|---|---|
| `--headless` | no window, no sound (added by `EMU run`) |
| `--monitor <file>` / `--monitor -` | run monitor commands from a file / stdin; machine stands still until `g`/`f` |
| `--frames <n>` | quit after n frames (50 frames = 1 s of machine time) — without `--monitor` |
| `--screenshot <file>` | BMP of the last picture on quitting (`EMU run --png` does this for you) |
| `--type <text>` | type text after start-up; `\n` is RETURN |
| `--saves <dir>` | where `SAVE`/`*SAVE` and relative monitor file names go |
| `--sideways-ram <kB>`, `--rom <slot> <file>`, `--ram <slot>` | memory expansions |
| `--roms <dir>` | where os.rom and basic.rom are |
| a `.uef`/`.csw`, `.state`, `.bas`/`.txt` as last argument | tape (auto `CHAIN""`/`*RUN`), snapshot, or program typed in |

## Monitor commands (in a `.mon` script, one per line)

Numbers are hex (`$`/`&` optional) except the counts of `s`, `f`, `d`.
`#` at the start of a line is a comment.

| Command | What it does |
|---|---|
| `f [n]` | run n frames (50 = 1 s) |
| `g [addr]` | run until a breakpoint (at most 3000 frames, else `?` = failure) |
| `s [n]` | step n instructions |
| `b addr` / `br addr` / `bw addr` / `bc` | break on execute / read / write / clear all (repeating one toggles it off) |
| `pc addr` | set the program counter |
| `m [addr [end]]` | dump memory |
| `d [addr [n]]` | disassemble |
| `r` | registers, stack, hardware, breakpoints |
| `w addr byte..` | write memory |
| `l addr file` | load a file into memory |
| `sv addr end file` | save memory (end inclusive) to a file |
| `c addr file` | compare memory with a file (difference = failure) |
| `t text` | type text (`\n` = RETURN), returns when typed |
| `kd key..` / `ku [key..]` | hold keys down / release (SDL names: `A`, `Space`, `Return`, `Left`, `Left_Shift`, `Escape`, `_` for space) |
| `tr [n] file` / `tr` | trace instructions to a file / stop |
| `pf` / `pf off` / `pr [n]` / `pr addr end` | profile start / stop / report |
| `q` | quit |

Anything that fails prints a line starting with `?` and makes the emulator exit
with status 1 — so a script is a test.

## Memory map and conventions

- `0000–7FFF` RAM. User zero page `&70–&8F`. `PAGE` (start of BASIC program /
  usual machine code load address) = `&0E00`. Screen memory: mode 0–2 from
  `&3000`, mode 3 `&4000`, mode 4–5 `&5800`, mode 6 `&6000` (start-up mode is 6).
- `8000–BFFF` paged ROM (BASIC in slot 10/11), `C000–FFFF` OS.
- OS entry points: `OSWRCH &FFEE`, `OSASCI &FFE3` (CR → CR+LF), `OSNEWL &FFE7`,
  `OSRDCH &FFE0`, `OSBYTE &FFF4`, `OSWORD &FFF1`, `OSCLI &FFF7`.
- The machine is a 32K Electron, OS 1.00, BASIC II — no discs (no `*DISC`,
  no Plus 1/Plus 3), only tape.

## Gotchas (all verified)

- **Give it time to boot.** Start every script with `f 100` (or more) before
  `t`, `l` or `kd`; text typed before the `>` prompt is lost.
- **Typed text arrives in capitals** (caps lock is on at start-up).
- **Running machine code:** prefer `t CALL &E00\n` from BASIC, so `RTS` returns
  to BASIC. After `pc 0E00` + `g` an `RTS` returns into garbage: set a breakpoint
  on the last instruction instead.
- **A breakpoint also stops `t` and `f`.** With `b X` set, `t CALL &E00\n`
  already stops at X; a `g` straight after it waits for the *next* hit and
  fails after 3000 frames. Look at the output to see where it stopped.
- **Printing with `OSWRCH` and only CR (13)** returns to column 0 and BASIC's
  next prompt overwrites the line; use `OSASCI` or `OSNEWL`.
- **Tape takes machine time.** Loading a small tape ~250 frames; `SAVE` needs
  RETURN after "RECORD then RETURN" and several hundred frames before the
  `.uef` appears. Wait with `b <entry>` + `g` rather than guessing `f n`.
- **BASIC inline assembler needs two passes** (`FOR pass%=0 TO 2 STEP 2`), else
  forward branches assemble wrongly — a hang is not always the emulator's fault.
- **Where files go** (verified, and not quite what the emulator's README says):
  - file names in a script's `l`, `sv`, `c`, `tr` are relative to the
    directory the emulator runs in: where you run `EMU run` from (run it from
    the project root), or the script's own directory for `EMU test`;
  - tapes written by `SAVE`/`*SAVE` go to `--saves`. `EMU` passes the same
    directory unless you give `--saves` yourself — never run the emulator
    without it: the Mac application then saves into `~/Documents`.
- **Read the output for `?` lines.** A failed `l` (wrong path) does not stop
  the script: everything after it works on whatever was in memory — a
  `*SAVE` then puts the wrong bytes on tape.
- Breakpoints set by a script are not kept; the snapshot and settings of the
  interactive emulator are not touched by headless script runs except as noted
  in the README.
