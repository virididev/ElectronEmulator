#ifndef ELECTRON_MACHINE_H
#define ELECTRON_MACHINE_H

#include <cstdint>
#include <vector>

#include "CPU/Bus.h"
#include "CPU/MOS6502.h"

#include "Keyboard.h"
#include "Sound.h"
#include "Tape.h"
#include "TapeRecorder.h"
#include "ULA.h"
#include "Video.h"

/* An Acorn Electron: a 6502, the ULA, 32 kB of RAM and two ROMs.

   The ULA can page in sixteen ROMs, of which the Electron itself has two, each
   taking up two of the sixteen slots. The other twelve are for whatever is
   plugged in at the back; here a ROM or RAM can be put in each of them.

   The machine is the bus the 6502 is on. Unlike most, the Electron does not run
   its processor at a steady speed: the ULA stretches the clock as it sees fit, so
   one access by the processor is one, two or as many as eighty ticks of the 2 MHz
   clock everything else runs on. */
class Machine : public Bus
{
	public:
		enum { RAM_SIZE = 0x8000, ROM_SIZE = 0x4000, NUM_SLOTS = 16 };

		Machine();

		bool LoadROMs(const char *OSName, const char *BASICName);

		/* true if a slot is one the Electron leaves empty */
		static bool SlotIsFree(int Slot);

		/* Puts a ROM of up to 16 kB in a free slot, or 16 kB of RAM, known as
		sideways RAM; false if the slot is not free or the file is no good */
		bool LoadSlot(int Slot, const char *Name);
		bool PutRAMInSlot(int Slot);
		void EmptySlot(int Slot);

		/* true if a slot is free and has nothing in it */
		bool SlotIsEmpty(int Slot) const { return SlotIsFree(Slot) && Slots[Slot].empty(); }

		/* switches the machine off and on again */
		void PowerOn();

		/* the BREAK key, which is the processor's reset line */
		void PressBreak();

		/* the kinds of breakpoint an address can have, as bits of its flag */
		enum { BREAK_EXECUTE = 0x01, BREAK_READ = 0x02, BREAK_WRITE = 0x04 };

		/* Runs until the ULA has finished a frame. If Breakpoints is given, it
		holds a flag for every address, and running also stops when the processor
		is about to execute an instruction at an address with BREAK_EXECUTE, or
		has just run one that read an address with BREAK_READ or wrote one with
		BREAK_WRITE; true if that is what happened */
		bool RunFrame(const uint8_t *Breakpoints = NULL);

		/* if it was an address being read or written that stopped RunFrame, which
		address and which of the two */
		bool AccessBreak(uint16_t &Addr, bool &Written) const;

		/* runs a single instruction, and draws the picture as far as the beam
		has come */
		void Step();

		/* memory as the processor sees it, read without anything happening as a
		result, and written if it is RAM */
		uint8_t Peek(uint16_t Addr) const;
		void Poke(uint16_t Addr, uint8_t Data);

		/* adds lines of text that say what state the hardware is in */
		void Describe(std::vector<std::string> &Lines) const;

		/* the time a real Electron takes over a frame */
		uint64_t FrameNanoseconds() const;

		void SetKey(int Key, bool Down) { Keys.SetKey(Key, Down); }

		/* saves the state of the whole machine to a stream, or loads it from one;
		to be called between frames */
		void TransferState(StateStream &);

		/* true while a tape is being listened to */
		bool TapeRunning() const { return Cassette.Running() && TheULA.Receiving(); }

		uint8_t Read(uint16_t Addr);
		void Write(uint16_t Addr, uint8_t Data);

		uint8_t RAM[RAM_SIZE];
		uint8_t OS[ROM_SIZE], BASIC[ROM_SIZE];

		/* what is in the free slots: 16 kB each, or nothing */
		std::vector<uint8_t> Slots[NUM_SLOTS];
		bool SlotIsRAM[NUM_SLOTS];

		MOS6502 CPU;
		Video Screen;
		Sound Speaker;
		Tape Cassette;
		TapeRecorder Recorder;
		Keyboard Keys;
		ULA TheULA;

	private:
		bool FrameDone;

		/* the flags RunFrame was given, for as long as it runs, and the access
		that should make it stop: an address, with ACCESS_WRITTEN added if it was
		written, or ACCESS_NONE */
		enum { ACCESS_NONE = -1, ACCESS_WRITTEN = 0x10000 };
		const uint8_t *AccessFlags;
		int AccessHit;

		/* what is paged in at 8000 to bfff, if it is anything in a free slot */
		const std::vector<uint8_t> &PagedSlot() const { return Slots[TheULA.PagedROM() & (NUM_SLOTS - 1)]; }

		int AccessTicks(uint16_t Addr) const;
		void Advance(int Ticks);
};

#endif
