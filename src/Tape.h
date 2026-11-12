#ifndef ELECTRON_TAPE_H
#define ELECTRON_TAPE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Snapshot/StateStream.h"

/* The cassette recorder, playing a UEF or a CSW file.

   The sound on an Acorn tape is a square wave of which every half is short or
   long: a pulse of a 4800th of a second, as in the 2400 Hz carrier tone, or one of
   twice that. A 1 is four short pulses and a 0 two long ones. A tape is kept here
   as its pulses, with stretches of silence between them, which is what the ULA
   gets to hear.

   A UEF file describes a tape as a row of chunks: data, stretches of the carrier
   tone that precedes and follows every block, and silences. A CSW file is a
   recording of a tape, as the lengths of the halves of its waves.

   Recording is the work of TapeRecorder. */
class Tape
{
	public:
		/* what a tape is made of; silence comes in pieces as long as a short
		pulse */
		enum { PULSE_SHORT = 0, PULSE_LONG = 1, PULSE_SILENCE = 2, PULSE_END = -1 };

		/* the pulses to a second, if all are short, and to each bit */
		enum { PULSE_RATE = 4800, SHORT_PULSES_PER_BIT = 4, LONG_PULSES_PER_BIT = 2 };

		/* how long a pulse lasts, in short pulses */
		static int Length(int Pulse) { return Pulse == PULSE_LONG ? 2 : 1; }

		Tape();

		/* reads a UEF file, compressed or not, or a CSW file */
		bool Open(const char *Name);
		void Eject();
		void Rewind();

		bool Inserted() const { return !Pulses.empty(); }
		const std::string &Name() const { return FileName; }

		/* how far the tape has played, 0 to 100 */
		int Position() const;

		/* the motor is under control of the machine; the tape is running while
		the motor is on and there is tape left */
		void SetMotor(bool On) { Motor = On; }
		bool MotorOn() const { return Motor; }
		bool Running() const { return Motor && Next < Pulses.size(); }

		/* the next pulse off a running tape, or PULSE_END */
		int NextPulse();

		/* true if the first file on the tape is a BASIC program, to be loaded
		with CHAIN, rather than machine code, to be loaded with *RUN */
		bool StartsWithBASIC() const { return FirstFileIsBASIC; }

		/* saves the state to a stream, or loads it from one; the tape itself is part
		of that state, so a snapshot does not depend on the file still being there */
		void TransferState(StateStream &);

	private:
		std::vector<uint8_t> Pulses;
		std::string FileName;
		size_t Next;
		bool Motor, FirstFileIsBASIC;

		static bool ReadFile(const char *Name, std::vector<uint8_t> &Contents);
		bool ParseUEF(const std::vector<uint8_t> &Contents);
		bool ParseCSW(const std::vector<uint8_t> &Contents);

		void AddPulses(int Pulse, size_t Count);
		void AddBits(int Bit, size_t Count);
		void AddByte(uint8_t Byte, int DataBits, char Parity, int StopBits);
		void AddRecordedPulse(double Seconds);

		void ReadFirstBlock(std::vector<uint8_t> &Block) const;
		void InspectFirstBlock();
};

#endif
