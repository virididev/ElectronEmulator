# Acorn Electron

An emulator of a bare Acorn Electron (32K, OS 1.00, BASIC II) with its cassette
recorder, built on SDL3. Sideways RAM can be switched on for games that use it,
and ROMs and RAM can be put in the slots for ROMs that the Electron leaves empty.

## Building

CMake (3.21 or later) and a C++17 compiler are needed, and SDL3 and zlib. If
SDL3 or zlib is not installed, the build downloads it and builds it in, so on
every system the steps are the same, from this directory:

    cmake -S . -B build
    cmake --build build --config Release

`-DEMULATOR_DOWNLOAD_DEPENDENCIES=ON` on the first line makes it download them
also when they are installed.

The ROMs are not part of the sources. If `roms/os.rom` and `roms/basic.rom`
are there when building, they are copied to sit next to the program; if not, see
[The ROMs](#the-roms).

### macOS

With Homebrew:

    brew install sdl3 cmake

The program is `./build/acornelectron [options] [tape.uef]`.

### Windows

Install [CMake](https://cmake.org/download/) and Visual Studio (the Community
edition will do) with "Desktop development with C++". In a Developer Command
Prompt, or any prompt in which `cmake` can be found, the two commands above make
`build\Release\acornelectron.exe`, with the ROMs and any DLLs it needs beside it.
It has the icon and can be started from Explorer; a tape can be dropped on it.

MinGW-w64 (MSYS2) works as well: `cmake -S . -B build -G Ninja` (or
`-G "MinGW Makefiles"`) and then `cmake --build build`.

### Linux

The compiler, CMake and zlib, and, for building SDL3 when the distribution has
no SDL3 package, the headers of the display and sound systems. On Debian and
Ubuntu:

    sudo apt install build-essential cmake zlib1g-dev \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev \
        libxtst-dev libwayland-dev libxkbcommon-dev libegl1-mesa-dev \
        libgl1-mesa-dev libasound2-dev libpulse-dev libudev-dev libdbus-1-dev

On Fedora: `sudo dnf install gcc-c++ cmake zlib-devel SDL3-devel`. Distributions
that have SDL3 (Debian 13, Ubuntu 25.04, Fedora 42, Arch and later) can install
it, `libsdl3-dev` on Debian and Ubuntu, instead of the list of headers.

The program is `./build/acornelectron [options] [tape.uef]`.

On a Mac the build also makes an application, `build/Acorn Electron.app`, with the
ROMs and an icon in it. It can be started from the Finder and moved to
Applications; a tape can be opened with it or dropped on it. Started that way, it
keeps what the machine saves in the Documents folder. The icon is drawn in
`icon/icon.svg`; `Host/macos/make-icon.sh` makes the `.icns` of it again
if the drawing changes, and `Host/windows/make-icon.sh` the `.ico` for Windows.

## The ROMs

The emulator needs the ROMs of the Electron, `os.rom` and `basic.rom`, 16 kB
each. It looks for them in a folder `roms`, first next to the program (in the
application on a Mac: `Acorn Electron.app/Contents/Resources/roms`), and then in
the folder where it keeps its settings:

| System  | Folder                                                      |
|---------|-------------------------------------------------------------|
| Windows | `%APPDATA%\Emulators\acornelectron\roms`                    |
| macOS   | `~/Library/Application Support/Emulators/acornelectron/roms` |
| Linux   | `~/.local/share/Emulators/acornelectron/roms`               |

The second place is the one for a program that was downloaded: the ROMs stay
there when a new version of the program takes the place of the old one. If the
ROMs are in neither place, the emulator says so, with the folders, and makes the
second folder, so that it is there to put them in. `--roms <directory>` names
another folder instead.

## Use

| Key | Function                                               |
|-----|--------------------------------------------------------|
| F1  | open a tape image, a snapshot or a program as text; with Shift: save the BASIC program as text |
| F2  | settings (also Cmd+comma on a Mac)                     |
| F3  | keep the state of the machine; with Shift: in a file   |
| F4  | put the state of F3 back; with Shift: from a file      |
| F5  | BREAK                                                  |
| F6  | power off and on                                       |
| F7  | rewind the tape; with Shift: take the tape out         |
| F8  | full speed on and off                                  |
| F9  | type the text on the clipboard (also Cmd+V on a Mac)   |
| F10 | pause on and off; with Shift: the monitor              |
| F11 | full screen                                            |
| F12 | save a picture of the screen in the Pictures folder    |

On a Mac, Cmd+O, Cmd+B, Cmd+comma, Cmd+S, Shift+Cmd+S, Cmd+L, Shift+Cmd+L, Cmd+V,
Cmd+P, Cmd+D, Cmd+G and Cmd+E do the same as F1, Shift+F1, F2, F3, Shift+F3, F4,
Shift+F4, F9, F10, Shift+F10, F12 and Shift+F7. `acornelectron --help` lists the
options.

The keyboard is positional: a key does what the Electron key in the same place
does. The exceptions: COPY is on End (and on `]`), the colon is on the key to the
right of the semicolon, and CAPS LK, which is also FUNC, is on Caps Lock and on
Alt. As on the Electron, Shift with that key switches capitals on and off. A Mac
does not say how long its Caps Lock key is held down, so there it will do for
switching capitals but not as FUNC; Alt does both.

A `.uef` or a `.csw` given on the command line, opened with F1 or dropped on the
window is loaded: with `CHAIN""` if the first file on it is a BASIC program,
otherwise with `*RUN`. While the tape runs, the emulator runs at full speed. A UEF
file describes what is on a tape; a CSW file is a recording of one, and loads as
long as the bits in it can be told apart, also if the recorder ran somewhat fast
or slow.

F9 makes the emulator type the text that is on the clipboard, at full speed, which
is handy for a BASIC listing. Escape stops it. With the setting "Fast pasting",
which is on to begin with, the text is not typed key by key but put straight in
the Electron's keyboard buffer, as fast as BASIC takes it in: a long listing is
in within seconds, and lower case letters stay lower case. With it off, each key
is pressed in turn, which takes a while, and lower case letters arrive as
capitals.

## Sideways RAM

Some games need sideways RAM: 16 kB banks of extra memory in the slots for ROMs,
which on a real Electron came on a board plugged in at the back. The setting
"Sideways RAM" on the settings screen (F2) puts 16, 32, 48 or 64 kB of it in the
machine, in slots 4, 5, 6 and 7, in that order. It is there at once, without a
restart; switch it on before loading the game, since a game looks for sideways
RAM as it starts. It stays on in later sessions until it is switched off again.
`--sideways-ram <kB>` does the same for one session only.

## ROMs in the free slots

The Electron can page in sixteen ROMs and has two of its own, BASIC and the
keyboard, which take up slots 8 to 11. The other slots are for what is plugged in
at the back. Besides the setting above, the command line can fill them:

    acornelectron --rom 12 view.rom --ram 13

`--rom <slot> <file>` puts a ROM of up to 16 kB in slot 0 to 7 or 12 to 15, and
`--ram <slot>` puts 16 kB of sideways RAM in a slot of your choice. Both can be
given more than once; the setting then uses the slots that are still empty. What
is in the slots is part of a snapshot. Power off and on empties the RAM.

## BASIC programs as text

Shift+F1 saves the BASIC program that is in memory as a text file, with the
extension `.bas`: what `LIST` would show, a line of text to a line of program.

Such a file, or any `.txt`, can be opened like a tape image: with F1, by dropping
it on the window, or by naming it on the command line. The emulator types it in,
at full speed and as the setting "Fast pasting" says, as it does the text on the
clipboard; Escape stops it. What was in
memory stays there unless the text starts with `NEW`.

## Status bar

Below the picture is a status bar. On the left it says which tape is in the
recorder and how far it has played, or, for a few seconds, what the emulator has
to tell you, such as where a picture or a saved program went. On the right are
the Caps Lock light of the Electron (`CAPS`), and `PAUSED` or `FULL SPEED` when
that is the case. It can be switched off on the
settings screen; what it says then goes in the title bar of the window instead.

## Snapshots

F3 keeps the state of the whole machine, F4 puts the machine back in that state,
as often as you like. That snapshot is kept in a file next to the settings, so it
is still there the next time you start the emulator.

For more than that one, Shift+F3 keeps the state in a file of your choice, with
the extension `.state`, and Shift+F4 puts the machine back in the state from
such a file. A snapshot file can also be opened like a tape image: with F1, by
dropping it on the window, or by naming it on the command line.

With the setting "Resume where I left off" on, the emulator keeps the state when
you quit and carries on from it the next time, unless you start it with a tape
image or a snapshot.

The tape that was in the recorder is part of a snapshot, and so is how far it had
played; the file it came from is not needed again. A snapshot does not depend on
the kind of computer it was made on.

## Monitor

Shift+F10 stops the machine and shows the monitor: the registers of the
processor, the instructions it is about to execute, a piece of memory, the stack
and the state of the rest of the hardware. Commands are typed at the bottom:

| Command          | What it does                                             |
|------------------|----------------------------------------------------------|
| `s [n]`          | run one instruction, or n of them                        |
| `g [addr]`       | leave the monitor and run on, from an address if given   |
| `f [n]`          | run on for one frame, or n of them, and come back        |
| `b addr`         | stop when the instruction at an address is about to run  |
| `br addr`        | stop when an address has been read                       |
| `bw addr`        | stop when an address has been written                    |
| `bc`             | clear all breakpoints                                    |
| `d [addr [n]]`   | show instructions from an address, or follow the processor again |
| `m [addr [end]]` | show memory from an address, or the next piece of it     |
| `w addr byte..`  | write bytes to memory                                    |
| `pc addr`        | make the processor carry on at an address                |
| `r`              | the registers, the stack, the hardware and the breakpoints |
| `sv addr end file` | save memory from addr up to and including end in a file |
| `l addr file`    | load a file into memory from addr on                     |
| `c addr file`    | compare memory from addr on with a file                  |
| `kd key..`       | press keys and hold them down                            |
| `ku [key..]`     | let go of keys, or of all that `kd` holds down           |
| `t text`         | type text, `\n` being Return, and come back when it is typed |
| `tr [n] file`    | trace every instruction in a file, at most n of them (100000) |
| `tr`             | stop tracing                                             |
| `pf [off]`       | start a profile afresh, or stop adding to it             |
| `pr [n]`         | the n (20) routines and instructions that took the most time |
| `pr addr end`    | the time taken by the instructions from addr to end      |
| `q` or Escape    | leave the monitor                                        |

Numbers are hexadecimal, and may start with `$` or `&`, except the counts of
`s`, `f` and `d`. A breakpoint stops `f` and `t` as well as `g`. A file named without a
directory is in the directory of `--saves`, or the current one. Return on an empty line runs
one instruction. Each of `b`, `br` and `bw` clears the breakpoint if it is
already there. In the list of instructions `>` marks the one the processor is at
and `*` a breakpoint; in the list of breakpoints an `r` or a `w` after an address
says it is one for reading or writing.

When the machine reaches a breakpoint while it is running, the monitor comes up
by itself and says why. Breakpoints are kept in a file next to the settings, so
they are still there the next time you start the emulator; `bc` gets rid of them.

### From a script

With `--monitor <file>` the emulator carries out the monitor commands in a file,
one to a line, and writes out what they show; `--monitor -` reads them from the
standard input. Together with `--headless` that lets a program, or an AI agent,
look inside the machine: load a program, set breakpoints, run, and look at the
registers and memory or save them to files.

    # let the machine start up, put a program in memory and start it
    f 150
    l 0400 game.bin
    pc 0400
    # run up to the end of its first frame and look
    b 0480
    g
    m 0070 007F
    sv 0400 04FF after.bin
    c 0070 expected.bin

The machine starts out standing still and runs only for `g` and `f`. Wherever
it stops, and after `s`, the registers and the next instruction are written out;
`d`, `m` and `r` write out what they would show. `g` waits for a breakpoint for no
more than 3000 frames, a minute of the machine's time. `q` ends the emulator, and
so does the end of the script without a window. Lines that are empty or start
with `#` are passed over; a `#` further on in a line is part of it, as BASIC
may need it to be.

Keys are those of the keyboard of the computer the emulator runs on, named as SDL
names them, with `_` for a space: `A`, `1`, `Space`, `Return`, `Left`, `Up`,
`Left_Shift`, `Left_Ctrl`, `Escape`. The machine takes them as it takes the keys
you press. A key held down by `kd` stays down while the machine runs, until `ku`,
which is how a game is played from a script:

    # type, and run until it has been typed
    t RUN\n
    # start the game
    kd Space
    f 10
    ku
    # walk left for a second, and see where that ends
    kd Left
    f 50
    ku
    m 0070 0071

Give the machine time to start up before typing: until the prompt is there,
what is typed is lost.

Breakpoints set by a script are its own and are not kept.

### Traces and profiles

`tr` writes a line for every instruction the processor runs: its cycle count
before it, the address, the instruction and the registers after it.

    2985603 0400 JSR $0410     A 00 X 00 Y 00 S EB P 22
    2985609 0410 LDX #$10      A 00 X 10 Y 00 S EB P 20

`pf` adds up how long every instruction and every routine takes, in the machine's
own time, while it runs, and `pr` says where the time went: in microseconds a
frame, and as a share of all of it. That says whether a game's work fits in a
frame, and where it does not:

    profile of 50 frames, 998398 us
    routines, with all they call and the interrupts that come meanwhile
    us a frame  share     calls  routine
       15485.7  77.6%      2265  0420
        4346.4  21.8%      2265  0410

A routine is what a JSR goes to, timed until it has returned; its time includes
the routines it calls and the interrupts that come in the meantime, and an
interrupt handler is listed as a routine of its own. `pr 0410 0415` adds up the
instructions in a stretch of memory instead. Without a script, `pr` writes its
report in `profile.txt`.

What fails is marked with a `?`: a command that could not be carried out, a
comparison that found a difference, a `g` that reached no breakpoint, a `t` that
did not get its text typed. The
emulator then ends with exit status 1, so that a script can serve as a test.

## Settings

F2 opens the settings screen; the machine stands still meanwhile. Up and down
choose a setting, left and right (or Return) change it, Escape closes the screen.

| Setting                 | Meaning                                         |
|-------------------------|-------------------------------------------------|
| Fast tape loading       | run at full speed while a tape is running       |
| Fast pasting            | pasted text straight into the keyboard buffer   |
| Load tapes at once      | load and run a tape as soon as it is inserted   |
| Full screen             | full screen                                     |
| Status bar              | a line of information below the picture         |
| Scanlines               | dark lines between the lines of the picture     |
| Vertical lines          | dark lines between its columns                  |
| Vignette                | the picture darker towards its corners          |
| Smooth picture          | pixels that run into each other                 |
| Resume where I left off | keep the state on quitting and carry on from it |
| Sideways RAM            | 0, 16, 32, 48 or 64 kB, see above               |
| Volume                  | loudness of the sound, 0 to 100                 |
| Window size             | size of the window, 1x to 6x                    |

Scanlines, vertical lines, vignette and smooth picture make the picture look more
like that of the picture tube these machines were connected to. Each can be
switched on by itself; all are off to begin with.

The settings are kept in `settings.cfg` in the folder for application settings
(on a Mac `~/Library/Application Support/Emulators/acornelectron/`). The
options `--no-fasttape`, `--no-fastpaste`, `--no-autoload`, `--fullscreen`, `--volume`, `--scale`
and `--sideways-ram` overrule them for that one session, without changing what is kept. F11 likewise
switches full screen for the moment only.

## Saving

`SAVE "NAME"` keeps the program as `NAME.uef` in the directory the emulator was
started from, or in the one given by `--saves <dir>`. The Electron first asks for
RECORD then RETURN; press Return. The status bar says where the file is. A file
that was already there is not overwritten; the new one is then called
`NAME 2.uef`. Saving again under the same name within one session does replace
your own earlier version.

## With Claude Code

The folder `plugins/acorn-electron` is a plugin for Claude Code with skills
that let it write programs for the Electron and use this emulator, as it is
installed on the computer, to run them without a window, test, debug and
profile them, and put them on a tape. In Claude Code:

    /plugin install acorn-electron --marketplace virididev/ElectronEmulator

The skills look for the emulator where it is usually installed
(`/Applications/Acorn Electron.app` on a Mac, `%ProgramFiles%\acornelectron` or
`%LOCALAPPDATA%\Programs\acornelectron` on Windows, `~/.local/bin`,
`/usr/local/bin` or `/opt/acornelectron` on Linux) and on the PATH; if it is
somewhere else, Claude asks where and remembers it. They need Python 3, and
for programs in machine code the assembler of cc65.

## Structure

| File                    | Hardware                                               |
|-------------------------|--------------------------------------------------------|
| `src/Machine`           | memory, address decoding, stretching the CPU clock     |
| `src/ULA`               | registers, interrupts, ROM paging, cassette interface  |
| `src/Video`             | video: the seven screen modes and the palette          |
| `src/Sound`             | the tone generator                                     |
| `src/Keyboard`          | keyboard matrix                                        |
| `src/Tape`              | cassette recorder, plays a `.uef` or a `.csw`          |
| `src/TapeRecorder`      | recording: turns the bytes sent into a `.uef`          |
| `src/BASIC`             | not hardware: lists the BASIC program in memory        |
| `src/Main`              | not hardware: connects the machine to the shell        |
| `CPU/MOS6502`           | processor: every clock cycle is an access on the bus, undocumented opcodes included |
| `Host`                  | the shell: window, sound, keys, command line, settings, snapshots, status bar, monitor |
| `Snapshot`              | how the state of the machine is written to a file and read back |
| `Tests`                 | a program that runs Klaus Dormann's test of the 6502   |

Everything runs off the ULA's 2 MHz clock. A bus access by the processor takes one
tick for ROM, two for RAM and the ULA, and in modes 0 to 3 up to eighty ticks when
the ULA needs the memory itself for the picture. The picture is drawn to the tick,
so that palette changes halfway down the screen come out right.

The picture has a black border around it as wide as a PAL television shows, which
makes the whole 832 by 576.

Not emulated: expansions such as the Plus 1 and the Plus 3, and so no discs;
only sideways RAM and ROMs in the free slots.

## How it works

[`docs/`](docs/README.md) describes the workings for someone who wants to read or
change the code: the architecture, the processor, the machine, the
shell, snapshots, the file formats and how things were tested.

## The test of the processor

    cmake --build build --target functionaltest
    ./build/functionaltest 6502_functional_test.bin

The file to test with is not included; it is `6502_functional_test.bin` from
[https://github.com/Klaus2m5/6502_65C02_functional_tests](https://github.com/Klaus2m5/6502_65C02_functional_tests). The test passes when it
stops at address `3469`.

## With thanks to

ElectrEm, an Acorn Electron emulator by Thomas Harte, licensed under the GNU GPL
version 2, was a starting point for this one, and was looked at for how the
machine behaves while writing it. The code here is written anew and is not taken
from it.

## What is not ours

The ROMs of the Electron are Acorn's. The acorn in the icon is the one from
Acorn's logo.
