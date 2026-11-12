# Snapshots

`Snapshot/StateStream.h`, and the snapshot functions in `Host/Shell.cpp`.

A snapshot is the whole state of a machine: put back, the machine carries on
exactly as it would have. This was tested by comparing a run that went through a
snapshot with one that did not, byte for byte; see [Testing](testing.md).

## One function for both directions

A `StateStream` is a row of bytes that is either being written or being read.
A component passes its state through it with the same calls in the same order
either way:

```cpp
void Sound::TransferState(StateStream &Stream)
{
	Stream.Value(Enabled);
	Stream.Value(Level);
	Stream.Value(HalfPeriod);
	...
}
```

There is no separate "save" and "load" to keep in step. Where loading needs
something extra, a component asks `Stream.Loading()`: `Video` recomputes its
colours from the palette registers, `Machine` forgets a recording that was
being made.

| Call                 | Stores                                                    |
|----------------------|-----------------------------------------------------------|
| `Value(x)`           | a whole number of any size, low byte first, in as many bytes as its type has; a `bool` as one byte; a `float` as its four bytes |
| `Bytes(p, n)`        | n bytes as they are                                       |
| `Size(n)`            | a `size_t` as eight bytes                                 |
| `Vector(v)`          | eight bytes of length, then the bytes                     |
| `String(s)`          | the same, for text                                        |
| `Expect(x)`          | a number or a text that must be the same on loading as it was on saving |

Numbers have a fixed layout whatever the host does in its own memory, so a
snapshot does not depend on the kind of computer it was made on.

## Failing

A stream that is asked for more than it holds, or in which an `Expect` does not
match, has failed: `Failed()` is true and nothing is read from then on. A
component can also declare failure itself with `Fail()`, when what it reads
makes no sense. Components do not have to check after every
call; the shell checks once at the end.

## What is in a snapshot

`Machine::TransferState` passes RAM and then every component in turn. Things
worth knowing:

- **The tape is in it**, contents and position, so a snapshot does not need the
  tape file again.
- **The Electron's two ROMs are not in it**; what is in its ROM slots is.
- **Sideways RAM is in it**, contents and all, and so is which slots the
  setting filled.
- **The picture is not in it.** It is drawn anew every frame; the position of
  the beam is.
- **Sound waiting to be played is not in it**, nor is a recording that was being
  made, nor are the keys that are held down, which are the host's.
- **The processor's state is that between two instructions**, which is why a
  snapshot is only made or put back between frames.

## The file

A snapshot file is a `StateStream` that starts with three `Expect`s:

1. the text `EMUSNAP`;
2. the name of the program, `acornelectron`;
3. the snapshot version, an `int`.

then the machine's state. `IsSnapshot` only looks for the signature, so that a
snapshot of another emulator or another version is recognised as a snapshot and
refused with a message that says so, rather than being taken for a broken tape.

Loading has four outcomes (`LoadSnapshot`): loaded; no such file; "unfit", when
the header does not match, in which case the machine has not been touched; and
"broken", when the state itself ran out, in which case the machine is neither
what it was nor what it should be and is switched off and on.

## Versions

`SNAPSHOT_VERSION` in each `Main.cpp` has to be raised whenever what
`TransferState` passes changes, in any component of that machine. There is no
conversion of old snapshots: an old one is refused.

## Where they are used

- **F3 and F4** keep and put back `snapshot.state` in the folder for
  preferences.
- **Shift+F3 and Shift+F4** do the same with a file of the user's choice, with
  the extension `.state`.
- **Opening** a snapshot file like a tape puts it back.
- **Resume**: with that setting on, the state is written to `resume.state` on
  quitting and put back at the next start; with it off, the file is removed.
