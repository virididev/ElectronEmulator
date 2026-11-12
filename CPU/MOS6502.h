#ifndef CPU_MOS6502_H
#define CPU_MOS6502_H

#include <cstdint>
#include <string>

#include "Bus.h"
#include "Snapshot/StateStream.h"

/* NMOS 6502, including the undocumented opcodes.

   The core is cycle exact in the sense that matters to the rest of a machine: every
   cycle of every instruction is a real access on the Bus, dummy reads and writes
   included, and the interrupt lines are sampled in the cycle the real chip samples
   them in. */
class MOS6502
{
	public:
		enum
		{
			FLAG_C = 0x01, FLAG_Z = 0x02, FLAG_I = 0x04, FLAG_D = 0x08,
			FLAG_B = 0x10, FLAG_U = 0x20, FLAG_V = 0x40, FLAG_N = 0x80
		};

		/* Something that wants to know of every instruction the processor runs,
		and of every interrupt sequence: it is told after each, with the address
		it started at, which for an interrupt is that of the instruction it came
		before, and the count of cycles before it */
		class Observer
		{
			public:
				virtual ~Observer() {}
				virtual void Executed(MOS6502 &, uint16_t Addr, uint64_t StartCycles, bool Interrupt) = 0;
		};

		explicit MOS6502(Bus &);

		/* the one observer there may be, or NULL for none, which costs nothing */
		void Observe(Observer *O) { TheObserver = O; }
		Observer *Observing() const { return TheObserver; }

		/* puts the registers in their power on state; no bus activity */
		void PowerOn();

		/* runs the seven cycle reset sequence */
		void Reset();

		/* runs one instruction, or one interrupt sequence if an interrupt is due */
		void Step();

		/* IRQ is level sensitive, NMI is triggered by its falling edge - here: by
		the change from not asserted to asserted */
		void SetIRQ(bool Asserted) { IRQLine = Asserted; }
		void SetNMI(bool Asserted);

		/* The instruction that three bytes at an address make up, as text, and the
		number of those bytes that belong to it */
		static std::string Disassemble(uint16_t Addr, const uint8_t Bytes[3], int &Length);

		/* true once a JAM opcode has locked the processor up; only a reset helps */
		bool Jammed() const { return IsJammed; }

		/* saves the state, which is that between two instructions, to a stream, or loads it from one */
		void TransferState(StateStream &);

		uint16_t PC;
		uint8_t A, X, Y, S, P;

		/* total number of cycles run since PowerOn */
		uint64_t Cycles;

	private:
		Bus &TheBus;
		Observer *TheObserver;

		/* what Step does, but for telling the observer */
		void Run();

		bool IRQLine, NMILine, NMIPending;
		bool IsJammed;

		/* interrupt request as sampled at the end of the last cycle and of the
		cycle before that; the latter decides what follows an instruction */
		bool PollNow, PollPrevious;

		enum AccessType { ACCESS_READ, ACCESS_WRITE };

		uint8_t Read(uint16_t Addr);
		void Write(uint16_t Addr, uint8_t Data);
		void EndCycle();

		uint8_t Fetch() { return Read(PC++); }
		void Push(uint8_t Data) { Write(0x100 | S--, Data); }
		uint8_t Pull() { return Read(0x100 | ++S); }

		uint16_t Address(uint8_t Mode, AccessType);
		uint16_t AbsoluteIndexed(uint16_t Base, uint8_t Index, AccessType);

		void Interrupt();
		void Branch(bool Taken);
		void SetNZ(uint8_t Value);
		void Compare(uint8_t Register, uint8_t Value);
		void AddWithCarry(uint8_t Value);
		void SubtractWithCarry(uint8_t Value);
		uint8_t ShiftLeft(uint8_t Value);
		uint8_t ShiftRight(uint8_t Value);
		uint8_t RotateLeft(uint8_t Value);
		uint8_t RotateRight(uint8_t Value);
};

#endif
