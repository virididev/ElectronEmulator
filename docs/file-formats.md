# File formats

Every kind of file the emulator reads or writes, and which class deals with it.

| File            | Read | Written | Class                              |
|-----------------|------|---------|------------------------------------|
| `.uef`          | yes  | yes     | `Tape`, `TapeRecorder`             |
| `.csw`          | yes  | no      | `Tape`                             |
| `.rom`          | yes  | no      | `Machine`                          |
| `.state`        | yes  | yes     | `StateStream`, `Shell`             |
| `.bas`, `.txt`  | yes  | yes (`.bas`) | `Shell`, `BASIC`              |
| `.bmp`          | no   | yes     | `Display`                          |
| `settings.cfg`  | yes  | yes     | `Settings`                         |
| `breakpoints.cfg` | yes  | yes     | `Monitor`                          |

## `.uef`

`UEF File!`, a zero, two bytes of version, and then chunks: two bytes of type,
four bytes of length, and the data. Usually the whole file is gzip compressed.
The chunks that are understood are listed in
[Acorn Electron](acorn-electron.md); others are skipped. `TapeRecorder` writes
uncompressed files with chunks `0x0110` (carrier), `0x0100` (data) and `0x0112`
(gap), and version 0.10.

What is in the data chunks is the Acorn tape format. A block is:

| Bytes     | What                                                             |
|-----------|------------------------------------------------------------------|
| `2A`      | synchronisation                                                  |
| name, `00`| the name of the file                                             |
| 4 + 4     | load address and execution address                               |
| 2 + 2     | number of the block and length of its data                       |
| 1         | flags; bit 7 marks the last block                                |
| 4         | unused                                                           |
| 2         | checksum of the header                                           |
| data, 2   | up to 256 bytes, and their checksum                              |

The emulator does not need to know this to load a tape: the operating system
does the reading. It is used in two places: to tell whether the first file is
BASIC (`Tape::InspectFirstBlock`), and to name a recording
(`TapeRecorder::Stopped`).

## `.csw`

"Compressed Square Wave": a recording of a tape as the lengths of its pulses in
samples. After the signature `Compressed Square Wave` and a byte `1A` comes a
header, laid out differently in the two versions:

| Field            | Version 1          | Version 2                         |
|------------------|--------------------|-----------------------------------|
| version          | bytes `17`, `18`   | bytes `17`, `18`                  |
| sample rate      | 2 bytes at `19`    | 4 bytes at `19`                   |
| compression      | byte `1B`          | byte `21`: 1 plain, 2 zlib        |
| extension length | –                  | byte `23`                         |
| data starts at   | `20`               | `34` + extension length           |

A pulse is one byte, or a zero followed by four bytes if it is longer than 255
samples.

## ROMs

Plain images. The Electron's are two of 16 kB (`os.rom`, `basic.rom`). A ROM for a slot of the
Electron may be shorter and is repeated to fill 16 kB.

## Snapshots, `.state`

See [Snapshots](snapshots.md).

## Programs as text, `.bas` and `.txt`

Plain text, a line of text to a line of program, as `LIST` shows it. Opened,
such a file is typed in; nothing is tokenised by the emulator. Written, it comes
from `HostedMachine::ProgramText`, for which `BASIC::List` walks the program in
memory and expands the tokens.

A program is a row of lines from PAGE (the high byte of which is at `18`). A
line is a carriage return, its number with the high byte first, its length and
its text. The keywords are in a table in the BASIC ROM as name, token, flags. A
line number in the text, as after `GOTO`, is the token `8D` and three bytes that
encode it; five keywords have a second token for when they are assigned to.

The table is found by looking for its first entries in the ROM rather than by a
fixed address, and tokens between quotes are not expanded.

## Pictures, `.bmp`

Written by SDL. F12 saves what the window shows; `--screenshot` saves the last
frame without border, at the size given by `--scale`.

## `settings.cfg`

Lines of `name = value`, in the folder for preferences:

    FastTape = true
    AutoLoad = true
    FullScreen = false
    StatusBar = true
    Scanlines = false
    VerticalLines = false
    Vignette = false
    Smooth = false
    Resume = false
    Volume = 100
    Scale = 3

What is not in the file keeps its default. A `Scale` of 0 means the size the
machine suggests.

## `breakpoints.cfg`

One breakpoint per line: the address in hexadecimal and letters for its kinds.
