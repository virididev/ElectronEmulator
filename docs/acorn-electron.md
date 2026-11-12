# Acorn Electron

`src`. An Electron is a 6502, 32 kB of RAM, two ROMs and one large
chip that does everything else: the ULA. Here the ULA is split over three
classes, because video and sound are so much code of their own.

| File           | What it is                                                     |
|----------------|----------------------------------------------------------------|
| `Machine`      | memory, address decoding, the ROM slots, stretching the clock  |
| `ULA`          | the ULA's registers, interrupts, frame timing, ROM paging, cassette interface |
| `Video`        | the ULA's video circuit: seven modes and the palette           |
| `Sound`        | the ULA's sound circuit: one square wave                       |
| `Keyboard`     | the key matrix, and which host key is which Electron key       |
| `Tape`         | the cassette recorder playing: a `.uef` or a `.csw` to pulses  |
| `TapeRecorder` | the cassette recorder recording: bytes to a `.uef`             |
| `BASIC`        | not hardware: lists the BASIC program in memory as text        |
| `Main`         | not hardware: `HostedElectron`, the machine as the shell sees it |

## Time: the 2 MHz clock

Everything is counted in **ticks** of the ULA's 2 MHz clock, half a microsecond
each. A line of the frame is 128 ticks, a frame 312 lines: 39,936 ticks, or
19.968 ms.

The Electron does not run its processor at a steady speed. The ULA stretches the
processor's clock depending on what is being accessed, and that is the heart of
`Machine`:

| Access to                         | Takes                                               |
|-----------------------------------|-----------------------------------------------------|
| ROM (`8000`–`FBFF`, `FF00`–`FFFF`) | 1 tick: the processor runs at 2 MHz                |
| RAM (`0000`–`7FFF`)               | until the end of the next cycle of the 1 MHz clock: 1 or 2 ticks |
| I/O (`FC00`–`FEFF`)               | the same as RAM                                     |
| RAM, in modes 0–3, while the ULA is reading the line from memory | until the ULA is done with the line: up to 80 ticks |

`Machine::AccessTicks(Addr)` works that out from the address and the position
of the beam. `Read` and `Write` then do `Advance(Ticks - 1)`, the access itself,
and `Advance(1)`: the access happens in the last tick of the cycle.

In modes 0 to 3 the ULA needs every memory cycle for the picture during the 80
ticks (40 µs) that a line is on the screen, so the processor can only get at RAM
in the remaining 48, and outside the picture. That makes those modes about half
as fast as modes 4 to 6. In mode 3 the two blank lines after every eight are not
read from memory, nor the six lines below the last row, so mode 3 is somewhat
faster than modes 0 to 2. `ULA::RAMBusy()` answers the question, with the help
of `Video::Fetching(Position)`, which knows which lines are blank.

`Advance` ticks the ULA, which ticks the sound and the cassette interface; after
it the processor's IRQ line is set from `ULA::IRQ()`.

## Memory

| Addresses     | What                                                           |
|---------------|----------------------------------------------------------------|
| `0000`–`7FFF` | RAM; the top holds the screen                                  |
| `8000`–`BFFF` | one of sixteen paged ROMs (see below)                          |
| `C000`–`FBFF` | the operating system ROM                                       |
| `FC00`–`FDFF` | expansion hardware: nothing here                               |
| `FE00`–`FEFF` | the ULA: sixteen registers, repeated sixteen times             |
| `FF00`–`FFFF` | the operating system ROM                                       |

An empty bus reads as `FF`.

### ROM slots

The ULA's paging register selects one of sixteen ROMs. The Electron has two of
its own, each in two slots: the **keyboard** in 8 and 9 and **BASIC** in 10 and
11. The keyboard is not a ROM at all; see below. The other twelve slots (0–7,
12–15) are for what is plugged in at the back.

`Machine` keeps a `std::vector<uint8_t> Slots[16]`, empty or 16 kB each, and
`SlotIsRAM[16]`. `LoadSlot` puts a ROM file in a slot, repeating a ROM shorter
than 16 kB to fill it, as a small ROM in a large socket shows up more than once.
`PutRAMInSlot` puts sideways RAM in one, which is cleared at power on, and
`EmptySlot` takes out what is in one.

**Sideways RAM from the settings.** `HostedElectron::SetSidewaysRAM` puts as
many banks as the setting asks for in empty slots, in the order 4, 5, 6, 7 and
then the others, or takes out the last ones it put in. It remembers which slots
it filled, in the snapshot as well, so that it never empties a slot that
`--rom` or `--ram` filled. Nothing is restarted: the OS takes no notice of RAM
without a ROM header, and games look for sideways RAM themselves when they
start, by writing to a slot and reading back.

The paging register has a quirk that is implemented in `ULA::Write`: while one
of slots 8 to 11 is paged in, a write of 0 to 7 is ignored; to get to those, one
of 12 to 15 has to be paged in first.

## The ULA's registers

`ULA::Read` and `ULA::Write`. All but two can only be written.

| Register | Read                          | Write                                          |
|----------|-------------------------------|------------------------------------------------|
| `FE00`   | interrupt status              | interrupt enable                               |
| `FE02`, `FE03` | –                       | address of the top left of the screen          |
| `FE04`   | the byte received from tape   | a byte to send to tape                         |
| `FE05`   | –                             | ROM paging; bits 4–6 clear interrupts          |
| `FE06`   | –                             | the counter: pitch of the sound                |
| `FE07`   | –                             | screen mode, cassette/sound mode, motor, caps lock light |
| `FE08`–`FE0F` | –                        | the palette                                    |

## Interrupts

The ULA has one interrupt output and six sources, of which a program enables the
ones it wants:

| Bit  | Source          | When                                                        |
|------|-----------------|-------------------------------------------------------------|
| 2    | display end     | as the last line of the picture has been drawn: after 256 lines, or after 250 in modes 3 and 6 |
| 3    | real time clock | a hundred lines into the picture                            |
| 4    | receive         | a byte has come in from tape                                |
| 5    | send            | the byte being sent to tape is out                          |
| 6    | high tone       | the carrier tone of a tape is being heard                   |

Bit 1 is the power on bit, set when the machine is switched on and cleared the
first time the status is read; it is how the operating system tells power on
from BREAK. Bit 0 says whether any enabled interrupt is pending.

The first two tick fifty times a second each, half a frame apart, which is how
the operating system gets its hundred ticks a second.

**The frame is taken to end where the picture ends**, not at the vertical sync.
Position 0 of a frame is the tick after the last line of a 256 line picture; the
picture starts at line 56. This puts the display end interrupt at the wrap and
the clock interrupt at line 155.

BREAK is not a key in the matrix: it is the processor's reset line.
`PressBreak()` resets the processor and nothing else; the ULA is only reset by
power on.

## Video

`Video`. The picture is always drawn at the resolution of the finest mode, 640
by 256, with pixels twice as tall as wide. In one tick the ULA puts eight of
those pixels on the screen, so the 80 active ticks of a line are 640 pixels.

| Mode | Bytes per line | Colours | Kind                                    |
|------|----------------|---------|-----------------------------------------|
| 0    | 80             | 2       | graphics, 640 pixels                    |
| 1    | 80             | 4       | graphics, 320 pixels                    |
| 2    | 80             | 16      | graphics, 160 pixels                    |
| 3    | 80             | 2       | text, 80 columns, 25 rows of 8 + 2 lines |
| 4    | 40             | 2       | graphics, 320 pixels                    |
| 5    | 40             | 4       | graphics, 160 pixels                    |
| 6    | 40             | 2       | text, 40 columns, 25 rows of 8 + 2 lines |

Memory is laid out for text: the eight lines of a character cell follow each
other, then come those of the cell to its right. So the address of a byte is the
address of the row, plus eight times the column, plus the line within the row.
The address of the top left comes from two registers and is taken once a frame;
when the address runs past `8000` it wraps to the start of the screen for that
mode, which is how the operating system scrolls without moving memory.

The bits of a pixel are spread over the byte: with four pixels to the byte the
leftmost is bits 7 and 3, with two it is bits 7, 5, 3 and 1. The four bits of a
pixel index sixteen colours; modes with fewer colours use the same four bits
with the missing ones at zero, which is why they use colours 0, 2, 8 and 10.

**The palette** is eight registers in which the red, green and blue of each of
the sixteen colours sit as single bits in an arrangement that follows no
pattern; `ColourMap` in `Video.cpp` is the table of it. A set bit switches that
part of the colour off. `SetPalette` recomputes all sixteen colours.

**Drawing is exact to the tick, and lazy.** `RunTo(Position)` draws from where
it had got to up to a tick of the frame. It is called before every change that
affects the picture: `Machine::Write` to RAM, `ULA::Write` to the mode, the
palette or the screen address, and at the end of the frame. `DrawColumn` draws
the eight pixels of one tick; in the 40 byte modes a byte lasts two ticks.
`EndLine` moves on to the next line of the row, or the next row.

The black border is the shell's: 96 pixels left and right and 32 lines above
and below, which is what a PAL television shows around the Electron's picture.

## Sound

`Sound`. A single square wave of fixed volume. The counter register sets its
frequency: 1 MHz / (32 × (counter + 1)). The counter only drives the speaker in
one of the four modes of the control register; the other three are cassette
input, cassette output and off.

A sample is the average of the speaker's level over 32 ticks, which gives 62,500
samples a second. The averaging matters: programs set the highest possible
frequency as a way of being silent, and that must not turn into a tone.

## Keyboard

`Keyboard`. A matrix of fourteen columns by four rows; a key is
`column * 4 + row`. It has no chip. With slot 8 or 9 paged in, a read from
`8000`–`BFFF` reads the keyboard: each of the low fourteen address lines selects
a column when it is low, and the four rows appear on the low four bits of the
data bus.

The mapping from the host is positional. COPY is on End, the colon on the key to
the right of the semicolon, and CAPS LK, which is also FUNC, on Caps Lock and on
Alt. `KeyForCharacter` says which key types a character and whether it needs
SHIFT or CTRL; five keys have a third character that takes CTRL (`{ } ] \ ~`).

A Mac reports its Caps Lock key as pressed and released in the same instant.
`HostedElectron::HostKey` therefore keeps that key down for at least six frames;
the operating system needs to see a key in two scans.

## Tape: playing

`Tape`. The sound on an Acorn tape is a square wave of which every half is a
**pulse**: a short one of 1/4800 s, as in the 2400 Hz carrier tone, or a long
one of twice that. A 1 is four short pulses and a 0 two long ones, so a bit
lasts 1/1200 s either way. A tape is kept as a vector of pulses, each `SHORT`,
`LONG` or `SILENCE` (a piece of silence as long as a short pulse).

A **UEF** file describes a tape as chunks; `ParseUEF` turns each into pulses:

| Chunk    | What it is                                 | Becomes                         |
|----------|--------------------------------------------|---------------------------------|
| `0x0100` | bytes                                      | start bit, eight bits, stop bit each |
| `0x0102` | bits as they are on the tape               | those bits                      |
| `0x0104` | bytes in a format of the chunk's choosing  | bits, parity and stop bits as it says |
| `0x0110` | carrier tone, in cycles                    | two short pulses per cycle      |
| `0x0111` | carrier tone with a dummy byte in it       | tone, the byte `AA`, tone       |
| `0x0112` | silence, in cycles                         | silence                         |
| `0x0114` | security cycles: single waves, each short or long, possibly starting or ending with half a wave | exactly those pulses |
| `0x0116` | silence, in seconds                        | silence                         |

UEF files are usually compressed; they are read through zlib, which reads files
that are not compressed just as well.

A **CSW** file is a recording of a tape as the lengths of its pulses, in
samples. `ParseCSW` reads both versions of the format and both kinds of
compression, and classifies every pulse by its length: under half a short pulse
is noise, up to one and a half is short, up to three is long, anything longer is
silence. The tape is then played at its nominal speed, which makes a recording
from a recorder that ran a few percent fast or slow load like any other.

After parsing, `InspectFirstBlock` reads the first block off the tape the way
the ULA would and looks at its data to tell a BASIC program from machine code,
so that the shell can type `CHAIN""` or `*RUN`.

## Tape: the ULA's side

In `ULA::Tick`, while the ULA is in cassette input mode and the tape is running,
a countdown runs to the end of the current pulse. A short pulse is not a whole
number of ticks, so the countdown is in parts of a tick and loses nothing.

`ReceivePulse` makes bits of pulses: four short ones in a row are a 1, two long
ones a 0. Pulses that do not add up to a bit before the other kind comes along
are dropped, which puts the receiver back in step after anything out of the
ordinary. Silence clears the receiver.

`ReceiveBit` shifts bits into an eleven bit register. When it holds a stop bit,
a start bit, eight data bits and a stop bit, a byte is in: it goes to the
receive register and raises the receive interrupt. Eleven ones are carrier tone
and raise the high tone interrupt. After a byte the next nine bits cannot
complete another, so they are skipped.

Sending is simpler: a write to `FE04` in cassette output mode hands the byte to
`TapeRecorder` and starts a countdown of ten bit times, after which the send
interrupt says the byte is out.

## Tape: recording

`TapeRecorder` gets bytes, not a signal. It starts a new block whenever more
than three byte times have passed since the last byte, since that means carrier
tone in between. When the ULA leaves output mode or the motor stops, it writes a
UEF file: carrier tone, a data chunk per block, carrier tone, a gap. The name is
taken from the first block.

## The machine as the shell sees it

`Main.cpp` holds `HostedElectron`. Besides forwarding, it has the things that
are about the emulator: the options `--rom` and `--ram` (which are only recorded
when the command line is read, and carried out in `LoadROMs`, where a problem
can be reported), sideways RAM from the settings, the Caps Lock hold, the size
of the border, and the snapshot version.
