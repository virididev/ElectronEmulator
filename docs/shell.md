# The shell

`Host`. Everything the emulator is besides the machine it emulates: the
command line, the window, the sound, the keys that control the emulator, the
settings, snapshots, the monitor, and the loop that keeps the machine running at
the right speed. It knows the Electron only through `HostedMachine`.

| File.             | What it is                                                                        |
|-------------------|-----------------------------------------------------------------------------------|
| `HostedMachine.h` | the interface a machine offers the shell                                          |
| `Shell`           | `RunEmulator`: the command line and the main loop                                 |
| `Display`         | the window: picture, border, status bar, text, effects, file dialogs, screenshots |
| `AudioOutput`     | playing the machine's sound                                                       |
| `FrameLimiter`    | holding the emulation to real speed                                               |
| `AutoType`        | typing text on the machine's keyboard                                             |
| `Settings`        | what the user can set, and the file it is kept in                                 |
| `Monitor`         | the machine code monitor                                                          |
| `Profiler`        | traces and profiles of what the processor does, for the monitor                   |
| `macos/`          | what makes a macOS application out of a program                                   |

## HostedMachine

An emulator's `main` is three lines: make its `HostedMachine`, call
`RunEmulator(Machine, argc, argv)`, delete it. The interface, by subject:

| Subject   | Members                                                                 |
|-----------|-------------------------------------------------------------------------|
| identity  | `Describe()`: names, size of the picture and its border, initial scale, sample rate, how long booting takes, the extensions of its files, the snapshot version |
| options   | `Option(Count, Arguments)`, `OptionsHelp()`: options on the command line the shell does not know |
| start     | `LoadROMs(Directory, Problem)`, `PowerOn()`, `PressReset()`             |
| running   | `RunFrame(Breakpoints)`, `AccessBreak(...)`, `FrameNanoseconds()`       |
| output    | `Pixels()`, `Samples()`, `Indicators()`                                 |
| keyboard  | `HostKey(Scancode, Down)`, `TypeCharacter(Character, Down, Modifiers)` |
| media     | `InsertTape`, `EjectTape`, `RewindTape`, `TapeInserted`, `TapeRunning`, `TapeName`, `TapePosition`, `LoadCommand()` |
| saved by the machine | `TakeRecording(Name, Image)`                                 |
| settings  | `SetSidewaysRAM(Kilobytes)`: at start and when the setting changes      |
| monitor   | `Processor()`, `Step()`, `Peek`, `Poke`, `DescribeHardware(Lines)`      |
| snapshots | `TransferState(Stream)`                                                 |
| listing   | `ProgramText(Text)`: the BASIC program in memory as text                |

A few of these deserve a word:

- **`Describe()`** may depend on the options, so the shell asks for it after it
  has parsed the command line.
- **`Option`** gets the option and everything after it, and returns how many
  arguments it used, or zero if the option is not one of its own. A machine
  should only record what was asked and act on it in `LoadROMs`, where it can
  report a problem.
- **`LoadCommand()`** returns what to type to load and run the tape:
  `CHAIN""` or `*RUN`.
- **`SetSidewaysRAM`** is called once before the machine is switched on, and
  again only when that setting is changed on the settings screen, so that a
  snapshot with other slots filled keeps them until the user says otherwise.
- **`TakeRecording`** is polled once a frame. The machine
  hands over what it has finished; the shell decides where it goes.

## The command line

`ParseOptions` in `Shell.cpp`. The shell's own options are `--roms`, `--saves`,
`--scale`, `--fullscreen`, `--volume`, `--type`, `--no-autoload`,
`--no-fasttape`, `--frames`, `--screenshot`, `--headless` and `--monitor`;
anything else is
offered to the machine. An argument that is not an option is the file to open.

`--headless` runs without a window and without sound; with `--frames` and
`--screenshot` that makes the emulator usable from a script, which is how it was
tested. `--type` types text once the machine has booted. `--monitor` runs a
script of monitor commands; see below.

## Start-up

1. Parse the options. Initialise SDL: video and audio, or nothing if headless.
2. Find the ROMs: `--roms`, or the folder `roms` next to the program
   (`SDL_GetBasePath`, which inside a macOS application is its `Resources`).
3. Find the folder for preferences: `SDL_GetPrefPath("Emulators", ProgramName)`,
   on a Mac `~/Library/Application Support/Emulators/<program>/`. It holds
   `settings.cfg`, `snapshot.state`, `resume.state` and `breakpoints.cfg`.
4. Load the settings and let the command line overrule them for this session.
   Two copies are kept: `Kept`, which is what goes back in the file, and
   `Current`, which is in force. A change on the settings screen changes both.
5. `LoadROMs`, `PowerOn`.
6. Open the file named on the command line: a program as text is queued for
   typing, a snapshot is put back, anything else is inserted and, if the user
   wants tapes loaded at once, `LoadCommand()` is typed. With no file and
   "resume" on, the state kept at the last quit is put back.
7. Open the window and the sound device.

## The main loop

Each pass:

1. **Events.** A key goes to the monitor if that is open, to the settings screen
   if that is open, and otherwise is first checked against the emulator's own
   keys (F1–F12, and on a Mac the same things with Cmd) and then passed to the
   machine with `HostKey`. Key presses only set flags (`Open`, `KeepState`,
   `Paste`, ...); the work is done after all events have been read.
2. **The flags**, in a fixed order: snapshots, saving the program, opening a
   file, ejecting, settings, menu and monitor, screenshot, pause.
3. If paused, or the settings screen or the monitor is up: redraw, wait 20 ms,
   and start over. The machine stands still.
4. If there is a script that the machine is standing still for, read and carry
   out its lines until one says to run (or to quit).
5. Otherwise: `Typist.Frame()`, `Machine.RunFrame(...)`, see whether a
   breakpoint or the end of `f` hands the machine to the monitor or the script,
   collect a recording and printed output, then show the frame, queue the sound
   and wait.

**Files chosen in a dialog** arrive as events. SDL's file dialogs are
asynchronous; `Display::AskForFile` and `AskForNewFile` pass SDL a callback that
pushes a user event with the name of the file, and the loop picks it up like any
other event. Three event types are registered: a file to open, a file to keep a
snapshot in, and a file to keep a program in. A file dropped on the window, or
opened with the application from the Finder, arrives as SDL's drop event and
takes the same path as one opened with F1.

**What kind of file it is** is decided in the loop: a `.bas` or `.txt` is text
to type, a file that starts with the snapshot signature is a snapshot, anything
else goes to `InsertTape`.

**Full speed** ("warp") is on when the user asked for it (F8), while text is
being pasted, and, if the setting says so, while a tape is running. At full
speed there is no waiting, the sound is thrown away, and only every tenth frame
is shown.

**Messages** for the user (`Message`, `MessageCountdown`) are shown for 150
frames on the status bar, or in the title bar if there is no status bar.

## Display

One window, one renderer, one streaming texture of the machine's picture.

**Coordinates.** Everything is drawn in a logical coordinate system that SDL
fits into the window (`SDL_SetRenderLogicalPresentation`, letterboxed). Its size
is the picture plus its border, times `Unit`: a whole number chosen so that the
logical width is at least 700. That lets text be smaller than the machine's
pixels on a machine with few of them. Pixels of the machine can
be taller than wide (`PixelHeight`; 2 for the Electron).

**The window** is created with `SDL_WINDOW_HIGH_PIXEL_DENSITY`, so it uses every
pixel a Retina screen has. Vertical sync is off: the emulation keeps its own
time, which is not the monitor's.

**The border** is the part of the logical area the picture does not cover,
cleared to black. Its size comes from the machine.

**The status bar** is one line below the picture: text on the left and on the
right. If the two do not fit, the left one is shortened, in its middle if it
ends in the name of a file.

**Text** is SDL's built-in debug font of eight by eight pixels, scaled by a
whole number. The settings screen is drawn over a dimmed picture. The monitor's
page is different: it is drawn in the pixels of the screen rather than the
logical ones, as large as lets all lines fit, so it is as sharp as the screen
allows.

**Effects.** Scanlines and vertical lines are textures, one pixel wide or high,
of alternating transparent and partly black pixels, stretched over the picture.
The vignette is a 128 by 128 texture that gets darker with the square of the
distance from the middle, stretched over picture and border. "Smooth" switches
the scaling of the picture from nearest to linear.

**Screenshots** (`SaveWindowBMP`, `SavePageBMP`) draw the frame as usual and
read the pixels back before they are shown, so the file is exactly what the
window shows, at the window's size. `SaveBMP` is a static function that writes a
frame without a window, for `--screenshot`.

## Sound

`AudioOutput` opens an SDL audio stream of mono floats at the machine's own
sample rate (62,500 Hz for the Electron) and lets SDL
convert to what the sound card wants. Once a frame the machine's samples are
queued.

The emulation and the sound card each run on a clock of their own, so the queue
would slowly fill up or run dry. `Queue` measures how much sound is waiting and
plays it up to half a percent faster or slower to keep about 60 ms queued. If
more than a quarter of a second is waiting, the emulation has run away from the
sound card and the queue is cleared.

## Timing

`FrameLimiter::Wait(FrameNanoseconds)` adds the length of the frame to a
deadline and sleeps until then (`SDL_DelayPrecise`). Because the deadline is
accumulated, a frame that took too long is made up for by the next. If the
emulator has fallen more than four frames behind, the host was busy elsewhere
and the deadline is reset. `Restart()` is called after anything that kept the
machine from running normally: a pause, full speed, a snapshot put back.

## Typing

`AutoType` types text on the machine's keyboard: each character is held down for
four frames and released for three, and after RETURN it waits thirty frames,
since a machine does not look at its keyboard while it is busy. Shift and the
like go down a frame before the key and come up a frame after it, as they do
under a person's fingers. Pressed together, a frame could end in the middle of
the scan of the keyboard, after the ROM had looked at SHIFT and before it looked
at the key, and it would see the key without it. Characters the
machine has no key for are skipped. It is used for `--type`, for the load
command, for pasting from the clipboard and for programs opened as text.

Line endings are normalised first, tabs become spaces, and a last line without
an end gets one.

## Settings

`Settings` is a struct of switches and two numbers, with a table that gives each
its name in the file. `Load` leaves alone what the file does not mention. The
settings screen in `Shell.cpp` is a second table, `MenuItems`, that gives each
setting a label and, through pointers to members, says where it is; adding a
setting is a line in each table and whatever uses it.

## Saving what the machine saves

`SaveRecording` writes a recording to `<name>.<extension>` in the directory for
saved files: the one given with `--saves`, or the current directory, or, in a
macOS application, the Documents folder. The name is the machine's, made safe
for a file name. A file that exists is never overwritten unless this session
wrote it; a number is added instead.

## The monitor

`Monitor`. It is a page of text and a command line; it has no window of its own.
`Lines(Machine)` builds the page from `Processor()`, `Peek` and
`DescribeHardware`; `Key(Machine, Key)` edits the command line and executes it.

**Breakpoints** are a vector of 65,536 flags, one per address, with bits for
"execute", "read" and "write". The shell passes it to `RunFrame`, or `NULL` if
there are none, so a machine without breakpoints pays nothing for them. With
flags, a machine checks after every instruction whether the next one has an
execute flag, and in every bus access whether the address has a read or write
flag. It stops between instructions; `RunFrame` then returns true and the next
call carries on with the rest of the frame. `AccessBreak` says which address
was read or written.

Checking after the instruction rather than before is what lets `g` run on from a
breakpoint without stopping on it again.

Stepping calls `Machine.Step()`, which runs one instruction and, on the
Electron, draws the picture as far as the beam has come.

Breakpoints are kept in `breakpoints.cfg`, one address and its kinds per line.

**Running for a while.** `g` and `f` leave the monitor with `RESUME`; the monitor
then says, through `RunFrames()` and `RunToBreak()`, for how many frames the
machine is to run (none means no limit) and whether that was meant to end at a
breakpoint. The shell counts whole frames only, so a frame broken off by a
breakpoint is not counted twice, and hands the machine back with the reason it
stopped. `FrameNumber`, which `--frames` is about, likewise counts whole frames.

**Scripts.** `--monitor` gives the shell a second `Monitor`, with breakpoints of
its own that are not kept, and a file to read lines from (`stdin` for `-`).
`Script(Machine, Line, Output)` carries out a line through the same `Execute` the
keyboard uses; with `Output` given, `d`, `m`, `r` and `s` write their text to it
instead of to the page, and `q` returns `QUIT`. When running stops the shell calls
`Halted(Machine, Reason, Failure, Output)`, which adds the frame, the reason, the
registers and the next instruction. The shell writes every line to stdout and
flushes it, so a program at the other end of a pipe can answer what it reads.
`Failed()` decides the exit status. While the machine stands still for a script
read from stdin, the window, if there is one, stands still too.

**Traces and profiles.** The monitor owns a `Profiler`, which takes the
processor's `Observer` (see the processor) for as long as a trace or a profile is
on, and gives it back after. A trace is a line per instruction, written as it
runs, and stops itself when it has as many as it was allowed. A profile keeps a
time and a count for every address, and for every routine: what a JSR or an
interrupt goes to. It times a routine from its call until the stack pointer is
back where it was before the call, keeping a stack of the calls that run.

Time is the machine's own: `HostedMachine::Ticks()`, read after every
instruction, the difference being the time it took. By default that is the
processor's cycles; the Electron gives the ticks of its ULA's 2 MHz clock, since
its processor is slowed down by memory and the ULA's own needs. Reported time is
in microseconds, on average a frame when whole frames were run: the shell calls
`Monitor::Frame()` after each.

`sv`, `l` and `c` read and write files with `Peek` and `Poke`, so nothing
happens on the machine but what is written: `l` into ROM writes nothing, and
says so.

## macOS application

`macos/MacApp.cmake` defines `add_mac_app(target "Name" icon.icns extension...)`.
After the program is linked it builds `Name.app` in the build directory:

    Name.app/Contents/Info.plist            from macos/Info.plist.in
    Name.app/Contents/MacOS/<program>       a copy of the executable
    Name.app/Contents/Resources/icon.icns
    Name.app/Contents/Resources/roms/

and signs it ad hoc. The `Info.plist` lists the extensions the application
opens, so files can be dropped on its icon. Nothing in the program is different
when it runs from an application, with one exception: the shell notices from its
own path that it is inside one and saves to the Documents folder instead of the
current directory, which for an application is the root of the disc.

`macos/make-icon.sh icon.svg icon.icns` renders a drawing at all the sizes an
icon needs. The `.icns` files are kept with the sources, so the build does not
need the tools for it.
