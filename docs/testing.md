# Testing

There is no test suite in the repository apart from the test of the processor.
What follows is how each part was verified while it was built, so that it can be
done again, and what is known not to be exact.

## The processor

    cmake --build build --target functionaltest
    ./build/functionaltest 6502_functional_test.bin

`Tests/FunctionalTest.cpp` runs Klaus Dormann's functional test
(`6502_functional_test.bin` from
https://github.com/Klaus2m5/6502_65C02_functional_tests, not included) on a bus
that is nothing but 64 kB of memory. The test ends in a loop at `3469` if every
documented instruction, flag and addressing mode behaves; anywhere else it
loops, something failed. It takes 96,241,367 cycles, which is the number a real
6502 takes.

## How the rest was tested

Two techniques cover almost everything.

**Driving the hardware without a window.** `Machine` and everything below it
needs no SDL window, so a small program can build a machine, load its ROMs, run
frames, press keys through `SetKey`, and look at RAM, registers or the picture.
The tests of timing, tape formats, ROM slots and snapshots were
such programs. A typical one:

    Machine *M = new Machine;
    M->LoadROMs("roms/os.rom", "roms/basic.rom");
    M->PowerOn();
    for(int c = 0; c < 100; c++) { M->RunFrame(); M->Speaker.Samples.clear(); }
    /* press keys, run frames, inspect M->RAM or M->Screen.Pixels() */

Link it against the sources in `src` except `Main.cpp`, and against `emucpu`
and `emuhost`.

**Running the real program headless.** The shell's own options make a test of
the whole emulator a one-liner:

    ./build/acornelectron --headless --frames 200 \
        --type 'PRINT 6*7\n' --screenshot out.bmp

    ./build/acornelectron --headless --frames 9500 \
        --screenshot out.bmp game.uef

To look inside the machine as well, a script of monitor commands
(`--monitor`, see the README) loads programs, sets
breakpoints, runs, writes out registers and memory, and compares memory with
files; the exit status says whether all went as expected:

    printf 'f 150\nl 0E00 prog.bin\npc 0E00\nb 0E0A\ng\nc 5800 expect.bin\n' |
        ./build/acornelectron --headless --monitor -

For what needs the window (the settings screen, the monitor, file dialogs), a
test program included the emulator's `Main.cpp` with `main` renamed, derived
from its `HostedMachine` class, and pushed SDL events from `RunFrame` at chosen
frames. Events have to come from inside the program: macOS does not let another
process send keystrokes without permission.

## What was measured

### The whole machine

- **Snapshots**: a game was loaded from tape with a snapshot taken halfway and
  put back in a freshly started machine; the state at the end was identical,
  byte for byte, to that of an uninterrupted run, and so was the picture.

### Electron

- **The display end interrupt** is set as line 306 of the frame starts in modes
  3 and 6 and as the frame ends in the others; the clock interrupt at line 155.
- **Clock stretching**: a loop in BASIC takes 3.92 s in modes 0–2, 3.19 s in
  mode 3 and 1.94 s in modes 4–6. The time the ULA leaves the processor
  predicts a ratio of 0.82 for mode 3 against mode 0; 0.81 was measured.
- **A change of palette halfway down the screen**, by a program that hooks the
  interrupts: the picture changes colour on line 99, some 70 ticks into the
  line, with a short stretch of the in-between colour between the two register
  writes.
- **Tape**: `SAVE` and `LOAD` round trips of one and three blocks. CSW files
  made from UEF tapes at 11, 22, 44 and 48 kHz, and with the recorder 6% fast
  and 6% slow, load the same program. A UEF with a security cycles chunk gives
  exactly the pulses the chunk describes, and loads.
- **ROM slots**: a ROM of 8 kB made for the purpose is found by the operating
  system and answers `*HELP`; sideways RAM reads back what was written; both
  survive a snapshot.
- **Sideways RAM from the setting**: with `--sideways-ram 32` a routine that
  pages in slot 4, writes a byte at `8000` and reads it back gets the byte; the
  monitor's `r` shows RAM in slots 4 and 5. Without it the read gives `FF`.
- **Sound**: `SOUND 1,-15,89,100` gives 453 Hz, the nearest the counter can get
  to the 440 Hz that pitch stands for. The counter value comes from the real
  operating system, and the formula agrees with that of ElectrEm.
- **Programs as text**: a program with keywords between quotes, line numbers
  after `GOTO`, `GOSUB` and `RESTORE`, `DATA`, and assignments to `TIME`,
  `LOMEM` and `HIMEM` comes back out exactly as it was typed in.

## Known simplifications

### Electron

- How the real ULA makes bits of pulses that are neither four short ones nor
  two long ones in a row is not known here; such pulses are dropped.
- A tape is played at its nominal speed: the pulses of a CSW recording are taken
  as short or long, not as the lengths they had.
- While it sends to tape, the ULA does not hear the tape.
- The sound has not been listened to by someone who knows the machine.
- No expansions are emulated, so there are no discs; only sideways RAM and ROMs
  in the free slots.

- Lower case letters in pasted text arrive as capitals, because typing assumes
  the machine is as it starts up, with caps lock on.
- The macOS application is signed for the Mac they were built on and use the
  SDL3 that is installed there.

## Things that went wrong, and why

Worth knowing before changing the code they concern:

- **Auto-typing lost keys** after RETURN and just after power on. The machine
  does not read the keyboard while busy; hence the pause after RETURN and the
  number of boot frames in the machine's description.
- **The Electron's sound was an octave high.** The divider is 32, not 16.
- **The first block of a tape was read past its end** when deciding between
  BASIC and machine code, which made the answer a matter of luck.
- **A forward branch in a test program assembled to a branch to itself.** BBC
  BASIC's assembler needs two passes; a test that hangs the machine is not
  always the emulator's fault.
