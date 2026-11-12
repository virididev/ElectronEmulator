#include "MOS6502.h"

#include <cstdio>

namespace
{

/* addressing modes */
enum
{
	IMP, ACC, IMM, ZP, ZPX, ZPY, ABS, ABX, ABY, IZX, IZY, REL, IND
};

/* operations; the names of the undocumented ones are those most commonly used */
enum
{
	ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC, CLD, CLI,
	CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP, JSR, LDA, LDX, LDY,
	LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS, SBC, SEC, SED, SEI, STA,
	STX, STY, TAX, TAY, TSX, TXA, TXS, TYA,

	ALR, ANC, ANE, ARR, DCP, ISC, JAM, LAS, LAX, LXA, RLA, RRA, SAX, SBX, SHA, SHX,
	SHY, SLO, SRE, TAS
};

struct Instruction
{
	uint8_t Op, Mode;
};

const Instruction Instructions[256] =
{
	/* 00 */	{BRK, IMP}, {ORA, IZX}, {JAM, IMP}, {SLO, IZX}, {NOP, ZP }, {ORA, ZP }, {ASL, ZP }, {SLO, ZP },
				{PHP, IMP}, {ORA, IMM}, {ASL, ACC}, {ANC, IMM}, {NOP, ABS}, {ORA, ABS}, {ASL, ABS}, {SLO, ABS},
	/* 10 */	{BPL, REL}, {ORA, IZY}, {JAM, IMP}, {SLO, IZY}, {NOP, ZPX}, {ORA, ZPX}, {ASL, ZPX}, {SLO, ZPX},
				{CLC, IMP}, {ORA, ABY}, {NOP, IMP}, {SLO, ABY}, {NOP, ABX}, {ORA, ABX}, {ASL, ABX}, {SLO, ABX},
	/* 20 */	{JSR, ABS}, {AND, IZX}, {JAM, IMP}, {RLA, IZX}, {BIT, ZP }, {AND, ZP }, {ROL, ZP }, {RLA, ZP },
				{PLP, IMP}, {AND, IMM}, {ROL, ACC}, {ANC, IMM}, {BIT, ABS}, {AND, ABS}, {ROL, ABS}, {RLA, ABS},
	/* 30 */	{BMI, REL}, {AND, IZY}, {JAM, IMP}, {RLA, IZY}, {NOP, ZPX}, {AND, ZPX}, {ROL, ZPX}, {RLA, ZPX},
				{SEC, IMP}, {AND, ABY}, {NOP, IMP}, {RLA, ABY}, {NOP, ABX}, {AND, ABX}, {ROL, ABX}, {RLA, ABX},
	/* 40 */	{RTI, IMP}, {EOR, IZX}, {JAM, IMP}, {SRE, IZX}, {NOP, ZP }, {EOR, ZP }, {LSR, ZP }, {SRE, ZP },
				{PHA, IMP}, {EOR, IMM}, {LSR, ACC}, {ALR, IMM}, {JMP, ABS}, {EOR, ABS}, {LSR, ABS}, {SRE, ABS},
	/* 50 */	{BVC, REL}, {EOR, IZY}, {JAM, IMP}, {SRE, IZY}, {NOP, ZPX}, {EOR, ZPX}, {LSR, ZPX}, {SRE, ZPX},
				{CLI, IMP}, {EOR, ABY}, {NOP, IMP}, {SRE, ABY}, {NOP, ABX}, {EOR, ABX}, {LSR, ABX}, {SRE, ABX},
	/* 60 */	{RTS, IMP}, {ADC, IZX}, {JAM, IMP}, {RRA, IZX}, {NOP, ZP }, {ADC, ZP }, {ROR, ZP }, {RRA, ZP },
				{PLA, IMP}, {ADC, IMM}, {ROR, ACC}, {ARR, IMM}, {JMP, IND}, {ADC, ABS}, {ROR, ABS}, {RRA, ABS},
	/* 70 */	{BVS, REL}, {ADC, IZY}, {JAM, IMP}, {RRA, IZY}, {NOP, ZPX}, {ADC, ZPX}, {ROR, ZPX}, {RRA, ZPX},
				{SEI, IMP}, {ADC, ABY}, {NOP, IMP}, {RRA, ABY}, {NOP, ABX}, {ADC, ABX}, {ROR, ABX}, {RRA, ABX},
	/* 80 */	{NOP, IMM}, {STA, IZX}, {NOP, IMM}, {SAX, IZX}, {STY, ZP }, {STA, ZP }, {STX, ZP }, {SAX, ZP },
				{DEY, IMP}, {NOP, IMM}, {TXA, IMP}, {ANE, IMM}, {STY, ABS}, {STA, ABS}, {STX, ABS}, {SAX, ABS},
	/* 90 */	{BCC, REL}, {STA, IZY}, {JAM, IMP}, {SHA, IZY}, {STY, ZPX}, {STA, ZPX}, {STX, ZPY}, {SAX, ZPY},
				{TYA, IMP}, {STA, ABY}, {TXS, IMP}, {TAS, ABY}, {SHY, ABX}, {STA, ABX}, {SHX, ABY}, {SHA, ABY},
	/* A0 */	{LDY, IMM}, {LDA, IZX}, {LDX, IMM}, {LAX, IZX}, {LDY, ZP }, {LDA, ZP }, {LDX, ZP }, {LAX, ZP },
				{TAY, IMP}, {LDA, IMM}, {TAX, IMP}, {LXA, IMM}, {LDY, ABS}, {LDA, ABS}, {LDX, ABS}, {LAX, ABS},
	/* B0 */	{BCS, REL}, {LDA, IZY}, {JAM, IMP}, {LAX, IZY}, {LDY, ZPX}, {LDA, ZPX}, {LDX, ZPY}, {LAX, ZPY},
				{CLV, IMP}, {LDA, ABY}, {TSX, IMP}, {LAS, ABY}, {LDY, ABX}, {LDA, ABX}, {LDX, ABY}, {LAX, ABY},
	/* C0 */	{CPY, IMM}, {CMP, IZX}, {NOP, IMM}, {DCP, IZX}, {CPY, ZP }, {CMP, ZP }, {DEC, ZP }, {DCP, ZP },
				{INY, IMP}, {CMP, IMM}, {DEX, IMP}, {SBX, IMM}, {CPY, ABS}, {CMP, ABS}, {DEC, ABS}, {DCP, ABS},
	/* D0 */	{BNE, REL}, {CMP, IZY}, {JAM, IMP}, {DCP, IZY}, {NOP, ZPX}, {CMP, ZPX}, {DEC, ZPX}, {DCP, ZPX},
				{CLD, IMP}, {CMP, ABY}, {NOP, IMP}, {DCP, ABY}, {NOP, ABX}, {CMP, ABX}, {DEC, ABX}, {DCP, ABX},
	/* E0 */	{CPX, IMM}, {SBC, IZX}, {NOP, IMM}, {ISC, IZX}, {CPX, ZP }, {SBC, ZP }, {INC, ZP }, {ISC, ZP },
				{INX, IMP}, {SBC, IMM}, {NOP, IMP}, {SBC, IMM}, {CPX, ABS}, {SBC, ABS}, {INC, ABS}, {ISC, ABS},
	/* F0 */	{BEQ, REL}, {SBC, IZY}, {JAM, IMP}, {ISC, IZY}, {NOP, ZPX}, {SBC, ZPX}, {INC, ZPX}, {ISC, ZPX},
				{SED, IMP}, {SBC, ABY}, {NOP, IMP}, {ISC, ABY}, {NOP, ABX}, {SBC, ABX}, {INC, ABX}, {ISC, ABX}
};

/* the names of the operations, in the order of the list above */
const char *const Mnemonics[] =
{
	"ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT", "BMI", "BNE", "BPL", "BRK", "BVC", "BVS", "CLC", "CLD", "CLI",
	"CLV", "CMP", "CPX", "CPY", "DEC", "DEX", "DEY", "EOR", "INC", "INX", "INY", "JMP", "JSR", "LDA", "LDX", "LDY",
	"LSR", "NOP", "ORA", "PHA", "PHP", "PLA", "PLP", "ROL", "ROR", "RTI", "RTS", "SBC", "SEC", "SED", "SEI", "STA",
	"STX", "STY", "TAX", "TAY", "TSX", "TXA", "TXS", "TYA",

	"ALR", "ANC", "ANE", "ARR", "DCP", "ISC", "JAM", "LAS", "LAX", "LXA", "RLA", "RRA", "SAX", "SBX", "SHA", "SHX",
	"SHY", "SLO", "SRE", "TAS"
};

/* how many bytes an instruction is in each addressing mode, and how its operand
is written */
const struct { int Length; const char *Format; } ModeDescriptions[] =
{
	/* IMP */ { 1, "" },          /* ACC */ { 1, " A" },        /* IMM */ { 2, " #$%02X" },
	/* ZP  */ { 2, " $%02X" },    /* ZPX */ { 2, " $%02X,X" },  /* ZPY */ { 2, " $%02X,Y" },
	/* ABS */ { 3, " $%04X" },    /* ABX */ { 3, " $%04X,X" },  /* ABY */ { 3, " $%04X,Y" },
	/* IZX */ { 2, " ($%02X,X)" }, /* IZY */ { 2, " ($%02X),Y" }, /* REL */ { 2, " $%04X" },
	/* IND */ { 3, " ($%04X)" }
};

const uint16_t VECTOR_NMI = 0xfffa;
const uint16_t VECTOR_RESET = 0xfffc;
const uint16_t VECTOR_IRQ = 0xfffe;

}

MOS6502::MOS6502(Bus &B) : TheBus(B), TheObserver(NULL)
{
	PowerOn();
}

void MOS6502::PowerOn()
{
	PC = 0;
	A = X = Y = S = 0;
	P = FLAG_U | FLAG_I;
	Cycles = 0;
	IRQLine = NMILine = NMIPending = false;
	IsJammed = false;
	PollNow = PollPrevious = false;
}

void MOS6502::Reset()
{
	/* a reset is an interrupt sequence with the write line held inactive, which
	turns the three pushes into reads */
	Read(PC);
	Read(PC);
	Read(0x100 | S--);
	Read(0x100 | S--);
	Read(0x100 | S--);
	P |= FLAG_I;
	const uint8_t Lo = Read(VECTOR_RESET);
	PC = Lo | (Read(VECTOR_RESET + 1) << 8);

	IsJammed = false;
	NMIPending = false;
	PollNow = PollPrevious = false;
}

void MOS6502::SetNMI(bool Asserted)
{
	if(Asserted && !NMILine) NMIPending = true;
	NMILine = Asserted;
}

std::string MOS6502::Disassemble(uint16_t Addr, const uint8_t Bytes[3], int &Length)
{
	const Instruction &In = Instructions[Bytes[0]];
	Length = ModeDescriptions[In.Mode].Length;

	/* a branch is shown with the address it goes to */
	unsigned int Operand = (Length == 3) ? (Bytes[1] | (Bytes[2] << 8)) : Bytes[1];
	if(In.Mode == REL) Operand = (uint16_t)(Addr + 2 + (int8_t)Bytes[1]);

	char Text[32];
	snprintf(Text, sizeof(Text), ModeDescriptions[In.Mode].Format, Operand);
	return std::string(Mnemonics[In.Op]) + Text;
}

void MOS6502::TransferState(StateStream &Stream)
{
	Stream.Value(PC);
	Stream.Value(A);
	Stream.Value(X);
	Stream.Value(Y);
	Stream.Value(S);
	Stream.Value(P);
	Stream.Value(Cycles);
	Stream.Value(IRQLine);
	Stream.Value(NMILine);
	Stream.Value(NMIPending);
	Stream.Value(IsJammed);
	Stream.Value(PollNow);
	Stream.Value(PollPrevious);
}

/* bus access; one call is one cycle */

uint8_t MOS6502::Read(uint16_t Addr)
{
	const uint8_t Data = TheBus.Read(Addr);
	EndCycle();
	return Data;
}

void MOS6502::Write(uint16_t Addr, uint8_t Data)
{
	TheBus.Write(Addr, Data);
	EndCycle();
}

void MOS6502::EndCycle()
{
	Cycles++;

	/* the interrupt lines are sampled at the end of every cycle; whether an
	interrupt follows an instruction depends on the sample taken at the end of
	its second to last cycle. That is also what makes CLI, SEI and PLP take
	effect one instruction late: they change the flag in their final cycle */
	PollPrevious = PollNow;
	PollNow = NMIPending || (IRQLine && !(P & FLAG_I));
}

/* addressing */

uint16_t MOS6502::AbsoluteIndexed(uint16_t Base, uint8_t Index, AccessType Type)
{
	const uint16_t Addr = Base + Index;

	/* the low byte is added first, so the processor has an address with the
	wrong high byte on the bus for one cycle. Instructions that only read skip
	that cycle if no carry came out of the addition */
	if(Type == ACCESS_WRITE || ((Base ^ Addr) & 0xff00))
		Read((Base & 0xff00) | (Addr & 0x00ff));

	return Addr;
}

uint16_t MOS6502::Address(uint8_t Mode, AccessType Type)
{
	switch(Mode)
	{
		default:
		case IMM: return PC++;

		case ZP: return Fetch();

		case ZPX:
		{
			const uint8_t Base = Fetch();
			Read(Base);
			return (uint8_t)(Base + X);
		}

		case ZPY:
		{
			const uint8_t Base = Fetch();
			Read(Base);
			return (uint8_t)(Base + Y);
		}

		case ABS:
		{
			const uint8_t Lo = Fetch();
			return Lo | (Fetch() << 8);
		}

		case ABX:
		{
			const uint8_t Lo = Fetch();
			const uint16_t Base = Lo | (Fetch() << 8);
			return AbsoluteIndexed(Base, X, Type);
		}

		case ABY:
		{
			const uint8_t Lo = Fetch();
			const uint16_t Base = Lo | (Fetch() << 8);
			return AbsoluteIndexed(Base, Y, Type);
		}

		case IZX:
		{
			uint8_t Pointer = Fetch();
			Read(Pointer);
			Pointer += X;
			const uint8_t Lo = Read(Pointer);
			return Lo | (Read((uint8_t)(Pointer + 1)) << 8);
		}

		case IZY:
		{
			const uint8_t Pointer = Fetch();
			const uint8_t Lo = Read(Pointer);
			const uint16_t Base = Lo | (Read((uint8_t)(Pointer + 1)) << 8);
			return AbsoluteIndexed(Base, Y, Type);
		}
	}
}

/* arithmetic */

void MOS6502::SetNZ(uint8_t Value)
{
	P &= ~(FLAG_N | FLAG_Z);
	P |= Value & FLAG_N;
	if(!Value) P |= FLAG_Z;
}

void MOS6502::Compare(uint8_t Register, uint8_t Value)
{
	P &= ~FLAG_C;
	if(Register >= Value) P |= FLAG_C;
	SetNZ(Register - Value);
}

void MOS6502::AddWithCarry(uint8_t Value)
{
	const unsigned int Carry = P & FLAG_C;
	const unsigned int Sum = A + Value + Carry;

	if(P & FLAG_D)
	{
		/* NMOS decimal mode: Z comes from the binary sum, N and V from the
		result before the high digit is corrected */
		unsigned int Lo = (A & 0x0f) + (Value & 0x0f) + Carry;
		if(Lo > 9) Lo += 6;
		unsigned int Hi = (A >> 4) + (Value >> 4) + (Lo > 0x0f ? 1 : 0);

		P &= ~(FLAG_N | FLAG_Z | FLAG_V | FLAG_C);
		if(!(Sum & 0xff)) P |= FLAG_Z;
		if(Hi & 8) P |= FLAG_N;
		if(~(A ^ Value) & (A ^ (Hi << 4)) & 0x80) P |= FLAG_V;

		if(Hi > 9) Hi += 6;
		if(Hi > 0x0f) P |= FLAG_C;
		A = (uint8_t)((Hi << 4) | (Lo & 0x0f));
	}
	else
	{
		P &= ~(FLAG_V | FLAG_C);
		if(~(A ^ Value) & (A ^ Sum) & 0x80) P |= FLAG_V;
		if(Sum > 0xff) P |= FLAG_C;
		A = (uint8_t)Sum;
		SetNZ(A);
	}
}

void MOS6502::SubtractWithCarry(uint8_t Value)
{
	const unsigned int Borrow = (P & FLAG_C) ? 0 : 1;
	const unsigned int Difference = A - Value - Borrow;
	const uint8_t Before = A;

	/* the flags are those of the binary subtraction, in decimal mode too */
	if(P & FLAG_D)
	{
		int Lo = (A & 0x0f) - (Value & 0x0f) - (int)Borrow;
		int Hi = (A >> 4) - (Value >> 4);
		if(Lo & 0x10) { Lo -= 6; Hi--; }
		if(Hi & 0x10) Hi -= 6;
		A = (uint8_t)((Hi << 4) | (Lo & 0x0f));
	}
	else
		A = (uint8_t)Difference;

	P &= ~(FLAG_V | FLAG_C);
	if((Before ^ Value) & (Before ^ Difference) & 0x80) P |= FLAG_V;
	if(Difference < 0x100) P |= FLAG_C;
	SetNZ((uint8_t)Difference);
}

uint8_t MOS6502::ShiftLeft(uint8_t Value)
{
	P = (P & ~FLAG_C) | (Value >> 7);
	Value <<= 1;
	SetNZ(Value);
	return Value;
}

uint8_t MOS6502::ShiftRight(uint8_t Value)
{
	P = (P & ~FLAG_C) | (Value & 1);
	Value >>= 1;
	SetNZ(Value);
	return Value;
}

uint8_t MOS6502::RotateLeft(uint8_t Value)
{
	const uint8_t CarryIn = P & FLAG_C;
	P = (P & ~FLAG_C) | (Value >> 7);
	Value = (uint8_t)((Value << 1) | CarryIn);
	SetNZ(Value);
	return Value;
}

uint8_t MOS6502::RotateRight(uint8_t Value)
{
	const uint8_t CarryIn = P & FLAG_C;
	P = (P & ~FLAG_C) | (Value & 1);
	Value = (uint8_t)((Value >> 1) | (CarryIn << 7));
	SetNZ(Value);
	return Value;
}

/* flow of control */

void MOS6502::Branch(bool Taken)
{
	const int8_t Offset = (int8_t)Fetch();
	if(!Taken) return;

	Read(PC);
	const uint16_t Target = PC + Offset;
	if((Target ^ PC) & 0xff00) Read((PC & 0xff00) | (Target & 0x00ff));
	PC = Target;
}

void MOS6502::Interrupt()
{
	Read(PC);
	Read(PC);
	Push(PC >> 8);
	Push(PC & 0xff);

	/* the vector is chosen this late, so an NMI that arrives during the first
	cycles of an IRQ sequence takes the sequence over */
	uint16_t Vector = VECTOR_IRQ;
	if(NMIPending)
	{
		NMIPending = false;
		Vector = VECTOR_NMI;
	}

	Push(P & ~FLAG_B);
	P |= FLAG_I;
	const uint8_t Lo = Read(Vector);
	PC = Lo | (Read(Vector + 1) << 8);

	/* the first instruction of a handler always runs */
	PollPrevious = false;
}

void MOS6502::Step()
{
	if(!TheObserver)
	{
		Run();
		return;
	}

	const uint16_t Addr = PC;
	const uint64_t Start = Cycles;
	const bool Interrupting = PollPrevious && !IsJammed;
	Run();
	TheObserver->Executed(*this, Addr, Start, Interrupting);
}

void MOS6502::Run()
{
	if(IsJammed)
	{
		Read(PC);
		return;
	}

	if(PollPrevious)
	{
		Interrupt();
		return;
	}

	const Instruction &In = Instructions[Fetch()];
	const uint8_t Mode = In.Mode;

	switch(In.Op)
	{
		/* loads, and everything else that only reads its operand */

		case LDA: A = Read(Address(Mode, ACCESS_READ)); SetNZ(A); break;
		case LDX: X = Read(Address(Mode, ACCESS_READ)); SetNZ(X); break;
		case LDY: Y = Read(Address(Mode, ACCESS_READ)); SetNZ(Y); break;
		case LAX: A = X = Read(Address(Mode, ACCESS_READ)); SetNZ(A); break;

		case ORA: A |= Read(Address(Mode, ACCESS_READ)); SetNZ(A); break;
		case AND: A &= Read(Address(Mode, ACCESS_READ)); SetNZ(A); break;
		case EOR: A ^= Read(Address(Mode, ACCESS_READ)); SetNZ(A); break;
		case ADC: AddWithCarry(Read(Address(Mode, ACCESS_READ))); break;
		case SBC: SubtractWithCarry(Read(Address(Mode, ACCESS_READ))); break;
		case CMP: Compare(A, Read(Address(Mode, ACCESS_READ))); break;
		case CPX: Compare(X, Read(Address(Mode, ACCESS_READ))); break;
		case CPY: Compare(Y, Read(Address(Mode, ACCESS_READ))); break;

		case BIT:
		{
			const uint8_t Value = Read(Address(Mode, ACCESS_READ));
			P &= ~(FLAG_N | FLAG_V | FLAG_Z);
			P |= Value & (FLAG_N | FLAG_V);
			if(!(A & Value)) P |= FLAG_Z;
		}
		break;

		case NOP:
			if(Mode == IMP)
				Read(PC);
			else
				Read(Address(Mode, ACCESS_READ));
		break;

		case ANC:
			A &= Read(Address(Mode, ACCESS_READ));
			SetNZ(A);
			P = (P & ~FLAG_C) | (A >> 7);
		break;

		case ALR:
			A &= Read(Address(Mode, ACCESS_READ));
			A = ShiftRight(A);
		break;

		case ARR:
			A &= Read(Address(Mode, ACCESS_READ));
			A = (uint8_t)((A >> 1) | ((P & FLAG_C) << 7));
			SetNZ(A);
			P &= ~(FLAG_C | FLAG_V);
			if(A & 0x40) P |= FLAG_C;
			if((A ^ (A << 1)) & 0x40) P |= FLAG_V;
		break;

		case SBX:
		{
			const uint8_t Value = Read(Address(Mode, ACCESS_READ));
			const uint8_t Left = A & X;
			Compare(Left, Value);
			X = Left - Value;
		}
		break;

		/* the next three depend on analogue effects in the real chip; these are
		the values usually seen */
		case ANE:
			A = (A | 0xee) & X & Read(Address(Mode, ACCESS_READ));
			SetNZ(A);
		break;

		case LXA:
			A = X = (A | 0xee) & Read(Address(Mode, ACCESS_READ));
			SetNZ(A);
		break;

		case LAS:
			A = X = S = S & Read(Address(Mode, ACCESS_READ));
			SetNZ(A);
		break;

		/* stores */

		case STA: Write(Address(Mode, ACCESS_WRITE), A); break;
		case STX: Write(Address(Mode, ACCESS_WRITE), X); break;
		case STY: Write(Address(Mode, ACCESS_WRITE), Y); break;
		case SAX: Write(Address(Mode, ACCESS_WRITE), A & X); break;

		/* these store a register ANDed with the high byte of the base address
		plus one */
		case SHA:
		{
			const uint16_t Addr = Address(Mode, ACCESS_WRITE);
			Write(Addr, A & X & (uint8_t)(((Addr - Y) >> 8) + 1));
		}
		break;

		case SHX:
		{
			const uint16_t Addr = Address(Mode, ACCESS_WRITE);
			Write(Addr, X & (uint8_t)(((Addr - Y) >> 8) + 1));
		}
		break;

		case SHY:
		{
			const uint16_t Addr = Address(Mode, ACCESS_WRITE);
			Write(Addr, Y & (uint8_t)(((Addr - X) >> 8) + 1));
		}
		break;

		case TAS:
		{
			const uint16_t Addr = Address(Mode, ACCESS_WRITE);
			S = A & X;
			Write(Addr, S & (uint8_t)(((Addr - Y) >> 8) + 1));
		}
		break;

		/* read, modify, write: the unmodified value is written back first */

		case ASL: case LSR: case ROL: case ROR:
		case INC: case DEC:
		case SLO: case RLA: case SRE: case RRA: case DCP: case ISC:
		{
			if(Mode == ACC)
			{
				Read(PC);
				switch(In.Op)
				{
					case ASL: A = ShiftLeft(A); break;
					case LSR: A = ShiftRight(A); break;
					case ROL: A = RotateLeft(A); break;
					default: A = RotateRight(A); break;
				}
				break;
			}

			const uint16_t Addr = Address(Mode, ACCESS_WRITE);
			uint8_t Value = Read(Addr);
			Write(Addr, Value);

			switch(In.Op)
			{
				case ASL: case SLO: Value = ShiftLeft(Value); break;
				case LSR: case SRE: Value = ShiftRight(Value); break;
				case ROL: case RLA: Value = RotateLeft(Value); break;
				case ROR: case RRA: Value = RotateRight(Value); break;
				case INC: Value++; SetNZ(Value); break;
				case DEC: Value--; SetNZ(Value); break;
				case ISC: Value++; break;
				default: Value--; break;
			}

			Write(Addr, Value);

			switch(In.Op)
			{
				case SLO: A |= Value; SetNZ(A); break;
				case RLA: A &= Value; SetNZ(A); break;
				case SRE: A ^= Value; SetNZ(A); break;
				case RRA: AddWithCarry(Value); break;
				case DCP: Compare(A, Value); break;
				case ISC: SubtractWithCarry(Value); break;
				default: break;
			}
		}
		break;

		/* implied; all of them spend their second cycle rereading the next opcode */

		case TAX: Read(PC); X = A; SetNZ(X); break;
		case TAY: Read(PC); Y = A; SetNZ(Y); break;
		case TXA: Read(PC); A = X; SetNZ(A); break;
		case TYA: Read(PC); A = Y; SetNZ(A); break;
		case TSX: Read(PC); X = S; SetNZ(X); break;
		case TXS: Read(PC); S = X; break;
		case INX: Read(PC); X++; SetNZ(X); break;
		case INY: Read(PC); Y++; SetNZ(Y); break;
		case DEX: Read(PC); X--; SetNZ(X); break;
		case DEY: Read(PC); Y--; SetNZ(Y); break;
		case CLC: Read(PC); P &= ~FLAG_C; break;
		case SEC: Read(PC); P |= FLAG_C; break;
		case CLD: Read(PC); P &= ~FLAG_D; break;
		case SED: Read(PC); P |= FLAG_D; break;
		case CLI: Read(PC); P &= ~FLAG_I; break;
		case SEI: Read(PC); P |= FLAG_I; break;
		case CLV: Read(PC); P &= ~FLAG_V; break;

		/* stack */

		case PHA: Read(PC); Push(A); break;
		case PHP: Read(PC); Push(P | FLAG_B); break;

		case PLA:
			Read(PC);
			Read(0x100 | S);
			A = Pull();
			SetNZ(A);
		break;

		case PLP:
			Read(PC);
			Read(0x100 | S);
			P = (Pull() & ~FLAG_B) | FLAG_U;
		break;

		/* flow of control */

		case BPL: Branch(!(P & FLAG_N)); break;
		case BMI: Branch(P & FLAG_N); break;
		case BVC: Branch(!(P & FLAG_V)); break;
		case BVS: Branch(P & FLAG_V); break;
		case BCC: Branch(!(P & FLAG_C)); break;
		case BCS: Branch(P & FLAG_C); break;
		case BNE: Branch(!(P & FLAG_Z)); break;
		case BEQ: Branch(P & FLAG_Z); break;

		case JMP:
		{
			const uint8_t Lo = Fetch();
			const uint16_t Target = Lo | (Fetch() << 8);
			if(Mode == IND)
			{
				/* the pointer's high byte is not incremented at a page end */
				const uint8_t TargetLo = Read(Target);
				PC = TargetLo | (Read((Target & 0xff00) | ((Target + 1) & 0x00ff)) << 8);
			}
			else
				PC = Target;
		}
		break;

		case JSR:
		{
			const uint8_t Lo = Fetch();
			Read(0x100 | S);
			Push(PC >> 8);
			Push(PC & 0xff);
			PC = Lo | (Fetch() << 8);
		}
		break;

		case RTS:
		{
			Read(PC);
			Read(0x100 | S);
			const uint8_t Lo = Pull();
			PC = Lo | (Pull() << 8);
			Read(PC);
			PC++;
		}
		break;

		case RTI:
		{
			Read(PC);
			Read(0x100 | S);
			P = (Pull() & ~FLAG_B) | FLAG_U;
			const uint8_t Lo = Pull();
			PC = Lo | (Pull() << 8);
		}
		break;

		case BRK:
		{
			Fetch();
			Push(PC >> 8);
			Push(PC & 0xff);

			uint16_t Vector = VECTOR_IRQ;
			if(NMIPending)
			{
				NMIPending = false;
				Vector = VECTOR_NMI;
			}

			Push(P | FLAG_B);
			P |= FLAG_I;
			const uint8_t Lo = Read(Vector);
			PC = Lo | (Read(Vector + 1) << 8);
			PollPrevious = false;
		}
		break;

		default:
		case JAM:
			PC--;
			IsJammed = true;
		break;
	}
}
