# The 6502

`CPU/MOS6502.{h,cpp}` and `CPU/Bus.h`.

An NMOS 6502 with all 256 opcodes, the undocumented ones included.

## The bus

```cpp
class Bus
{
	public:
		virtual uint8_t Read(uint16_t Addr) = 0;
		virtual void Write(uint16_t Addr, uint8_t Data) = 0;
};
```

That is the whole interface between the processor and a machine. The core calls
one of the two exactly once per clock cycle, with the address a real 6502 would
put on its address lines in that cycle. This includes:

- the read of the byte after a one byte instruction, which is thrown away;
- the read from the wrong page when an indexed address crosses a page boundary,
  before the read from the right one;
- the write of the unchanged value before the write of the new one, in
  instructions that modify memory (`INC`, `ASL`, ...);
- the three pushes of an interrupt and the two reads of its vector.

These matter. A read of a register of a chip can have side effects whether the
processor wanted the value or not, and on the Electron every cycle has a length
that depends on the address.

## Structure of the core

A table of 256 entries gives every opcode an operation and an addressing mode.
`Step()` fetches the opcode and switches on the operation; `Address(Mode, Type)`
does the cycles of the addressing mode and returns the address to operate on.
`Type` says whether the instruction only reads, because that decides whether
the extra cycle of an indexed mode is always taken or only when a page is
crossed.

The private `Read` and `Write` wrap the bus and call `EndCycle()`, which counts
the cycle and samples the interrupt lines. Everything else is built on those.

Public state is the registers (`PC`, `A`, `X`, `Y`, `S`, `P`) and `Cycles`, the
number of cycles since power on. Machines use `Cycles` as a timestamp; the
monitor reads and writes the registers.

## Interrupts

`SetIRQ(bool)` sets the level of the IRQ line; a machine calls it after every
cycle with the state of whatever can interrupt. `SetNMI(bool)` is edge
triggered: going from not asserted to asserted latches a pending NMI.

The lines are sampled at the end of every cycle. Whether an interrupt sequence
follows an instruction is decided by the sample taken at the end of its second
to last cycle, as on the real chip. Two things follow from that without any
special code:

- an interrupt that arrives in the last cycle of an instruction is taken one
  instruction later;
- `CLI`, `SEI` and `PLP` take effect one instruction late, because they change
  the flag in their last cycle.

`Step()` therefore does one of three things: one cycle of nothing if the
processor is jammed, the seven cycles of an interrupt sequence if one is due, or
one instruction.

The vector is chosen late in the sequence, so an NMI that arrives during the
first cycles of an IRQ sequence, or during a `BRK`, takes it over, as it does on
the real chip. The first instruction of a handler always runs before another
interrupt is taken.

## Decimal mode and undocumented opcodes

`ADC` and `SBC` in decimal mode behave as the NMOS chip does, including the
flags, which are set from the binary result in ways no one intended. The
undocumented opcodes are implemented under the names in common use (`LAX`,
`SAX`, `DCP`, `ISC`, `SLO`, `RLA`, `SRE`, `RRA`, `ANC`, `ALR`, `ARR`, `SBX`, and
the unstable `ANE`, `LXA`, `SHA`, `SHX`, `SHY`, `TAS`, `LAS`). The twelve `JAM`
opcodes lock the processor up until a reset; `Jammed()` says so.

## Reset and power on

`PowerOn()` only sets the registers; `Reset()` runs the seven cycle reset
sequence, with its bus accesses, and loads `PC` from `FFFC`. A machine calls
both when it is switched on, and `Reset()` alone for a reset line.

## For the monitor

`static std::string Disassemble(uint16_t Addr, const uint8_t Bytes[3], int &Length)`
turns up to three bytes into the text of an instruction and says how many of
them it used. It is static and takes bytes rather than a bus so that looking at
memory never has side effects; machines offer `Peek` for getting the bytes.

`Observe(Observer *)` hands the processor one `MOS6502::Observer`, whose
`Executed(CPU, Addr, StartCycles, Interrupt)` is called after every instruction
and every interrupt sequence, with the address it started at and the cycle count
before it. That is how the monitor traces and profiles, without the machines
knowing. `Step` itself is then no more than a test for an observer around `Run`,
which is the core; without one, it costs that one test.

## Verification

Klaus Dormann's functional test runs to its end in 96,241,367 cycles; see
[Testing](testing.md). The cycle count matters as much as the result: it only
comes out right if every instruction takes the right number of cycles.
