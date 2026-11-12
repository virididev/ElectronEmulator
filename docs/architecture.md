# Architecture

## Layers

    +---------------------------------------------------------------+
    |  Host               the shell: main loop, window, sound,      |
    |                     keys, settings, snapshots, monitor (SDL3) |
    +------------------------------+--------------------------------+
                                   |  HostedMachine (filled in once,
                                   |  in src/Main.cpp)
    +------------------------------+--------------------------------+
    |  src                                                          |
    |  Machine: memory, address decoding, wiring                    |
    |  one class per piece of hardware: ULA, Keyboard, Tape, ...    |
    +------------------------------+--------------------------------+
                                   |
    +------------------------------+--------------------------------+
    |  CPU             the 6502 and the Bus it talks to             |
    |  Snapshot        StateStream                                  |
    +---------------------------------------------------------------+

Only `Host` and `src/Main.cpp` know about SDL. The hardware, down from
`Machine`, is plain C++ and can be built and driven without a window, which is
how most of it was tested (see [Testing](testing.md)). The one exception is
`Keyboard`, which uses SDL's scancode numbers to say which host key sits where.

The libraries follow the layers:

| Library    | Contents                         | Depends on        |
|------------|----------------------------------|-------------------|
| `emucpu`   | `CPU/MOS6502.cpp`                | nothing           |
| `emuhost`  | `Host/*.cpp`                     | `emucpu`, SDL3    |

`acornelectron` links `emuhost` and zlib.

## How time is kept

There is no scheduler and no event queue. The processor is the clock.

`MOS6502::Step()` runs one instruction. Every cycle of that instruction is one
call to `Bus::Read` or `Bus::Write`: the opcode fetch, the operand bytes, the
dummy reads and writes a real 6502 makes, and the pushes of an interrupt. The
machine *is* the bus (`class Machine : public Bus`), so it sees every cycle and
does two things in each:

1. decode the address and do the access;
2. advance everything else by the time that cycle took.

Time is counted in ticks of the ULA's 2 MHz clock, and a cycle of
the processor takes one, two or up to eighty of them, depending on what is
accessed and where the beam is (see [Acorn Electron](acorn-electron.md)). Step 2
is `Machine::Advance(Ticks)`.

The consequence is that every chip is in step with the processor to the cycle,
at all times, with no bookkeeping. An interrupt the ULA raises in the middle
of an instruction is raised in that cycle; the 6502 core samples the interrupt
line at the end of every cycle, as the real chip does.

The price is one virtual call and a handful of `Tick()` calls per emulated cycle.
At 2 MHz that is nothing to a current computer: the emulator runs at many
times real speed, which is what makes "full speed" tape loading work.

### Where things are put off

One thing is not done cycle by cycle, because nothing could tell the
difference: **the picture** is drawn up to an exact tick, but lazily:
  `Video::RunTo(Position)` is called just before anything changes that the
  picture depends on (a write to RAM, to the palette, the mode or the screen
address), and at the end of the frame. A change halfway along a line shows up
at the right pixel.

## A frame

The shell runs the machine one frame of its display at a time:

    while not quit:
        handle events (keys, dropped files, answers from file dialogs)
        if paused, in the settings screen or in the monitor:
            redraw and wait 20 ms
        else:
            Typist.Frame()            -- press or release an auto-typed key
            Machine.RunFrame(...)     -- until the ULA says the frame is over
            collect what was saved to tape, write it to a file
            queue the sound, present the picture
            sleep until the frame would have ended on the real machine

`RunFrame` is `while(!FrameDone) CPU.Step();`. `FrameDone` is set by the ULA in
the cycle its beam wraps. A frame therefore ends a few cycles late, at the end
of whichever instruction was running; the next frame is that much shorter.

Sound is produced by the sound hardware into a `std::vector<float>` as it is
ticked; after a frame the shell queues those samples and clears the vector.
The picture is a plain array of `0xAARRGGBB` pixels owned by the machine's video
class.

`FrameNanoseconds()` says how long the frame took on the real machine: 19.968 ms
(312 lines of 64 µs). That is slightly faster than 50 Hz, as the real machine
is.

## Adding a machine

The shell knows nothing of the Electron, so another machine could be added
next to it:

1. Write a `Machine` that derives from `Bus`, owns a `MOS6502` and its hardware,
   and ticks that hardware from `Read` and `Write`.
2. Give every piece of hardware a `Tick()`, a `Describe()` for the monitor and a
   `TransferState()` for snapshots.
3. In `src/Main.cpp`, derive a class from `HostedMachine` that forwards the
   shell's calls to the machine, and call `RunEmulator` from `main`. If the
   processor does not run at a steady 1 MHz, give it `Ticks()` and
   `TicksPerMicrosecond()` from the clock that does keep time, so that profiles
   are in real time.
4. Add an executable for it to `CMakeLists.txt`, like `acornelectron`.

Window, sound, hotkeys, command line, settings screen, snapshots, status bar,
monitor, clipboard pasting, screenshots and the macOS application come with the
shell.

## Conventions in the code

- Tabs; `PascalCase` for types, functions, members and locals; `if(` without a
  space; C style comments, in English.
- One class per piece of hardware, one `.h` and one `.cpp` per class, named
  after the hardware.
- A header says what a thing is and what its interface means; the `.cpp` says
  why the code is the way it is. Constants are named and explained where they
  are defined, in an anonymous namespace at the top of the `.cpp`.
- No exceptions, no RTTI, no allocation in the inner loops. Machines are
  allocated with `new` in `main` because they are too large for the stack.

## The build

`cmake -S . -B build && cmake --build build` builds everything.

After linking, the `roms` directory is copied next to the executable. On a Mac,
`Host/macos/MacApp.cmake` also puts an application
together in the build directory: `Info.plist` from a template, the executable,
`icon/icon.icns` and the ROMs, signed ad hoc. The application is a copy; the
plain executable stays for the command line. See [The shell](shell.md).

`functionaltest` is excluded from the default build; see [Testing](testing.md).
