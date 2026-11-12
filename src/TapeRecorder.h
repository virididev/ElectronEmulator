#ifndef ELECTRON_TAPERECORDER_H
#define ELECTRON_TAPERECORDER_H

#include <cstdint>
#include <string>
#include <vector>

/* The recording half of the cassette recorder. The ULA hands it the bytes it
   sends; when sending stops it has a UEF file of them, with carrier tone wherever
   the ULA was sending but had nothing to send. */
class TapeRecorder
{
	public:
		TapeRecorder();
		void Reset();

		/* a byte being sent, and the tick of the 2 MHz clock it starts at */
		void Byte(uint8_t Data, uint64_t Tick);

		/* the ULA no longer sending, or the motor stopping, which ends a
		recording */
		void Stopped();

		/* hands over a finished recording, as the contents of a UEF file and the
		name the Electron gave the first file on it; false if there is none */
		bool Take(std::string &Name, std::vector<uint8_t> &Image);

	private:
		/* the bytes sent so far, in blocks with carrier tone between them */
		std::vector< std::vector<uint8_t> > Blocks;
		uint64_t LastByte;

		bool Finished;
		std::string FinishedName;
		std::vector<uint8_t> FinishedImage;
};

#endif
