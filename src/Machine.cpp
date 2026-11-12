#include "Machine.h"

#include <cstdio>
#include <cstring>

/* Memory map:

	0000 - 7fff		RAM, the top of which holds the screen
	8000 - bfff		one of sixteen paged ROMs, of which a bare Electron has two:
					BASIC and, strangely, the keyboard; the other slots are
					empty unless something was put in them
	c000 - ffff		the operating system ROM, except for
	fc00 - fdff		expansion hardware, of which there is none here
	fe00 - feff		the ULA, sixteen times over */

#define ROM_START		0x8000
#define OS_START		0xc000
#define EXPANSION_START	0xfc00
#define ULA_PAGE		0xfe00

/* what the paging register holds when these are paged in; both occupy two slots */
#define PAGE_KEYBOARD(p)	(((p) & 0x0e) == 0x08)
#define PAGE_BASIC(p)		(((p) & 0x0e) == 0x0a)

/* what an empty bus reads as */
#define FLOATING		0xff

namespace
{

/* Reads a ROM and returns its length, which is zero if it is too long to be one. */
size_t LoadROM(const char *Name, uint8_t *Target)
{
	FILE *File = fopen(Name, "rb");
	if(!File) return 0;

	const size_t Length = fread(Target, 1, Machine::ROM_SIZE, File);
	const bool Complete = fgetc(File) == EOF;
	fclose(File);
	return Complete ? Length : 0;
}

}

Machine::Machine() : CPU(*this), Screen(RAM), TheULA(Screen, Speaker, Cassette, Recorder), FrameDone(false), AccessFlags(NULL), AccessHit(ACCESS_NONE)
{
	memset(OS, FLOATING, sizeof(OS));
	memset(BASIC, FLOATING, sizeof(BASIC));
	for(int c = 0; c < NUM_SLOTS; c++) SlotIsRAM[c] = false;
	PowerOn();
}

bool Machine::LoadROMs(const char *OSName, const char *BASICName)
{
	return LoadROM(OSName, OS) == ROM_SIZE && LoadROM(BASICName, BASIC) == ROM_SIZE;
}

bool Machine::SlotIsFree(int Slot)
{
	return Slot >= 0 && Slot < NUM_SLOTS && !PAGE_KEYBOARD(Slot) && !PAGE_BASIC(Slot);
}

bool Machine::LoadSlot(int Slot, const char *Name)
{
	if(!SlotIsFree(Slot)) return false;

	std::vector<uint8_t> Contents(ROM_SIZE, FLOATING);
	const size_t Length = LoadROM(Name, Contents.data());
	if(!Length) return false;

	/* a ROM of less than 16 kB shows up more than once in the space of one */
	for(size_t c = Length; c < ROM_SIZE; c++) Contents[c] = Contents[c % Length];

	Slots[Slot].swap(Contents);
	SlotIsRAM[Slot] = false;
	return true;
}

bool Machine::PutRAMInSlot(int Slot)
{
	if(!SlotIsFree(Slot)) return false;

	Slots[Slot].assign(ROM_SIZE, 0);
	SlotIsRAM[Slot] = true;
	return true;
}

void Machine::EmptySlot(int Slot)
{
	if(!SlotIsFree(Slot)) return;

	Slots[Slot].clear();
	SlotIsRAM[Slot] = false;
}

void Machine::PowerOn()
{
	memset(RAM, 0, sizeof(RAM));
	for(int c = 0; c < NUM_SLOTS; c++)
		if(SlotIsRAM[c]) Slots[c].assign(ROM_SIZE, 0);

	Keys.Reset();
	TheULA.PowerOn();

	CPU.PowerOn();
	CPU.Reset();
}

void Machine::PressBreak()
{
	CPU.Reset();
}

bool Machine::RunFrame(const uint8_t *Breakpoints)
{
	FrameDone = false;
	AccessHit = ACCESS_NONE;

	if(!Breakpoints)
	{
		while(!FrameDone) CPU.Step();
		return false;
	}

	/* The checks come after the instruction: for an address that is executed so
	that running on from a breakpoint gets past it, for one that is read or
	written because an instruction is not something to stop halfway through */
	AccessFlags = Breakpoints;
	bool Stopped = false;
	while(!FrameDone && !Stopped)
	{
		CPU.Step();
		Stopped = (Breakpoints[CPU.PC] & BREAK_EXECUTE) || AccessHit != ACCESS_NONE;
	}
	AccessFlags = NULL;

	if(Stopped) Screen.RunTo(TheULA.FramePosition());
	return Stopped;
}

bool Machine::AccessBreak(uint16_t &Addr, bool &Written) const
{
	if(AccessHit == ACCESS_NONE) return false;

	Addr = (uint16_t)AccessHit;
	Written = (AccessHit & ACCESS_WRITTEN) != 0;
	return true;
}

void Machine::Step()
{
	CPU.Step();
	Screen.RunTo(TheULA.FramePosition());
}

uint8_t Machine::Peek(uint16_t Addr) const
{
	if(Addr < ROM_START) return RAM[Addr];
	if(Addr < OS_START)
	{
		if(PAGE_BASIC(TheULA.PagedROM())) return BASIC[Addr - ROM_START];
		return PagedSlot().empty() ? FLOATING : PagedSlot()[Addr - ROM_START];
	}
	if(Addr >= EXPANSION_START && Addr < ULA_PAGE + 0x100) return FLOATING;
	return OS[Addr - OS_START];
}

void Machine::Poke(uint16_t Addr, uint8_t Data)
{
	if(Addr < ROM_START)
	{
		Screen.RunTo(TheULA.FramePosition());
		RAM[Addr] = Data;
	}
	else if(Addr < OS_START && SlotIsRAM[TheULA.PagedROM()])
		Slots[TheULA.PagedROM()][Addr - ROM_START] = Data;
}

void Machine::Describe(std::vector<std::string> &Lines) const
{
	TheULA.Describe(Lines);
	Screen.Describe(Lines);
	Speaker.Describe(Lines);

	/* the slots that were filled, if any were */
	std::string Filled;
	for(int c = 0; c < NUM_SLOTS; c++)
	{
		if(Slots[c].empty()) continue;

		char Text[16];
		snprintf(Text, sizeof(Text), " %d %s", c, SlotIsRAM[c] ? "RAM" : "ROM");
		Filled += Text;
	}
	if(!Filled.empty()) Lines.push_back("SLOTS" + Filled);
}

uint64_t Machine::FrameNanoseconds() const
{
	return (uint64_t)ULA::TICKS_PER_FRAME * 1000000000 / ULA::CLOCK_RATE;
}

void Machine::TransferState(StateStream &Stream)
{
	Stream.Bytes(RAM, sizeof(RAM));

	CPU.TransferState(Stream);
	TheULA.TransferState(Stream);
	Screen.TransferState(Stream);
	Speaker.TransferState(Stream);
	Cassette.TransferState(Stream);

	/* what is in the free slots is part of the machine as much as its RAM is */
	for(int c = 0; c < NUM_SLOTS; c++)
	{
		bool Filled = !Slots[c].empty();
		Stream.Value(Filled);
		Stream.Value(SlotIsRAM[c]);

		if(Stream.Failed()) break;
		if(Stream.Loading()) Slots[c].assign(Filled ? (size_t)ROM_SIZE : 0, FLOATING);
		if(Filled) Stream.Bytes(Slots[c].data(), ROM_SIZE);
	}

	/* a recording that was being made is lost, and the keyboard is the host's,
	so whatever is held down now stays held down */
	if(Stream.Loading()) Recorder.Reset();
}

/* How long the ULA makes the processor's clock cycle for an access to an address.

ROM is read at 2 MHz, a tick a cycle. RAM and the ULA itself are on a 1 MHz clock,
so a cycle that reaches them lasts until the next cycle of that clock is over. And
when the ULA is reading RAM for the picture at full speed, the processor has to
wait until the ULA is done with the line. */
int Machine::AccessTicks(uint16_t Addr) const
{
	const int Column = TheULA.FramePosition() % ULA::TICKS_PER_LINE;

	if(Addr < ROM_START)
		return TheULA.RAMBusy() ? Video::ACTIVE_TICKS - Column : 2 - (Column & 1);

	/* so is whatever is plugged in at the back */
	if(Addr >= EXPANSION_START && Addr < ULA_PAGE + 0x100)
		return 2 - (Column & 1);

	return 1;
}

void Machine::Advance(int Ticks)
{
	while(Ticks--)
		if(TheULA.Tick()) FrameDone = true;

	CPU.SetIRQ(TheULA.IRQ());
}

uint8_t Machine::Read(uint16_t Addr)
{
	if(AccessFlags && (AccessFlags[Addr] & BREAK_READ)) AccessHit = Addr;

	/* the access itself happens in the last tick of the cycle */
	Advance(AccessTicks(Addr) - 1);

	uint8_t Data;
	if(Addr < ROM_START)
		Data = RAM[Addr];
	else if(Addr < OS_START)
	{
		const uint8_t Paged = TheULA.PagedROM();

		if(PAGE_KEYBOARD(Paged)) Data = Keys.Read(Addr);
		else if(PAGE_BASIC(Paged)) Data = BASIC[Addr - ROM_START];
		else if(!PagedSlot().empty()) Data = PagedSlot()[Addr - ROM_START];
		else Data = FLOATING;
	}
	else if(Addr < EXPANSION_START || Addr >= ULA_PAGE + 0x100)
		Data = OS[Addr - OS_START];
	else if(Addr >= ULA_PAGE)
		Data = TheULA.Read(Addr & 0x0f);
	else
		Data = FLOATING;

	Advance(1);
	return Data;
}

void Machine::Write(uint16_t Addr, uint8_t Data)
{
	if(AccessFlags && (AccessFlags[Addr] & BREAK_WRITE)) AccessHit = Addr | ACCESS_WRITTEN;

	Advance(AccessTicks(Addr) - 1);

	if(Addr < ROM_START)
	{
		/* the picture is drawn from RAM, so what has gone by of it has to be
		drawn before RAM changes */
		Screen.RunTo(TheULA.FramePosition());
		RAM[Addr] = Data;
	}
	else if((Addr & 0xff00) == ULA_PAGE)
		TheULA.Write(Addr & 0x0f, Data);
	else if(Addr < OS_START && SlotIsRAM[TheULA.PagedROM()])
		Slots[TheULA.PagedROM()][Addr - ROM_START] = Data;

	Advance(1);
}
