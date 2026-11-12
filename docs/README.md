# Documentation

How the emulator works, for someone who wants to read or change the code.
What a user needs is in [the README](../README.md).

Read in this order for the whole picture; each page also stands by itself.

| Page                                     | What it is about                                              |
|------------------------------------------|------------------------------------------------------------   |
| [Architecture](architecture.md)          | the layers, how time is kept, what a frame is, the build      |
| [The 6502](cpu.md)                       | the processor core and its bus                                |
| [Acorn Electron](acorn-electron.md)      | the machine: ULA, clock stretching, video, sound, tape, sideways RAM |
| [The shell](shell.md)                    | everything around a machine: window, sound, keys, monitor     |
| [Snapshots](snapshots.md)                | how the state of a machine is saved and loaded                |
| [File formats](file-formats.md)          | every kind of file the emulator reads or writes               |
| [Testing](testing.md)                    | how things were verified, and what is known to be simplified  |

## The idea in a paragraph

A machine is the bus its processor is on. The 6502 core makes exactly one bus
access per clock cycle, dummy accesses included, and with every access the
machine advances all its other hardware by as long as that access takes. Nothing
is scheduled and nothing catches up later, except where drawing the picture is
put off until something that affects it changes. Around a machine sits a shell
that knows nothing of the hardware: it runs the machine a frame at a time, shows
the picture, plays the sound and handles the keys, and knows nothing of the
Electron beyond what the machine tells it through one interface.
