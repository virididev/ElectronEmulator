#ifndef ELECTRON_ULA_H
#define ELECTRON_ULA_H

#include <cstdint>
#include <string>
#include <vector>

#include "Snapshot/StateStream.h"

class Sound;
class Tape;
class TapeRecorder;
class Video;

/* The Electron's ULA, which is all of the machine but for the processor and the
   memory. Its video and sound circuits are in Video and Sound; here are its
   sixteen registers, the interrupts, the timing of the frame, the paging of ROMs
   and the cassette interface.

   Time is measured in ticks of the 2 MHz clock, half a microsecond each. */
class ULA
{
	public:
		enum
		{
			CLOCK_RATE = 2000000,
			TICKS_PER_LINE = 128,
			LINES_PER_FRAME = 312,
			TICKS_PER_FRAME = TICKS_PER_LINE * LINES_PER_FRAME
		};

		/* bits of the interrupt status and enable registers */
		enum
		{
			IRQ_MASTER = 0x01, IRQ_POWER_ON = 0x02, IRQ_DISPLAY_END = 0x04, IRQ_RTC = 0x08,
			IRQ_RECEIVE = 0x10, IRQ_SEND = 0x20, IRQ_HIGH_TONE = 0x40
		};

		ULA(Video &, Sound &, Tape &, TapeRecorder &);

		/* the machine being switched on, which is the one thing that resets
		the ULA; BREAK does not */
		void PowerOn();

		/* register access, Register is 0 to 15 */
		uint8_t Read(uint8_t Register);
		void Write(uint8_t Register, uint8_t Data);

		/* advances by one tick; true at the end of a frame */
		bool Tick();

		/* state of the interrupt request output */
		bool IRQ() const { return (Status & Enabled) != 0; }

		/* the tick of the frame that is about to pass */
		int FramePosition() const { return Position; }

		/* ticks of the 2 MHz clock since power on */
		uint64_t Time() const { return Ticks; }

		/* true while the ULA is reading the screen out of RAM at such a rate
		that the processor cannot get at it */
		bool RAMBusy() const;

		/* which of the sixteen ROMs is paged in; 8 and 9 are the keyboard, 10
		and 11 BASIC */
		uint8_t PagedROM() const { return Paged; }

		/* true while the ULA is listening to the cassette recorder */
		bool Receiving() const { return Mode == MODE_INPUT; }

		bool CapsLockLED() const { return CapsLock; }

		/* adds lines of text that say what state the chip is in, for a monitor */
		void Describe(std::vector<std::string> &Lines) const;

		/* saves the state of the registers, the interrupts and the cassette interface
		to a stream, or loads it from one */
		void TransferState(StateStream &);

	private:
		enum { MODE_INPUT, MODE_SOUND, MODE_OUTPUT, MODE_OFF };

		Video &TheVideo;
		Sound &TheSound;
		Tape &TheTape;
		TapeRecorder &TheRecorder;

		int Position;
		uint64_t Ticks;
		uint8_t Status, Enabled;
		uint8_t Paged;
		uint8_t Mode;
		bool CapsLock;

		/* cassette interface: the kind of pulse that is coming in and how many
		of them have, the bits that have come in, the last complete byte among
		them, and the countdowns to the next pulse in and to the moment a byte
		being sent is out */
		int PulseKind, PulseCount;
		unsigned int Shifter;
		uint8_t Received;
		int SkipBits, PulseCountdown, SendCountdown;

		void ReceivePulse(int Pulse);
		void ReceiveBit(int Bit);
};

#endif
