#ifndef ELECTRON_VIDEO_H
#define ELECTRON_VIDEO_H

#include <cstdint>
#include <string>
#include <vector>

#include "Snapshot/StateStream.h"

/* The video half of the Electron's ULA. It reads the screen straight out of RAM
   in one of seven modes, at one or two bytes per microsecond, and turns every
   byte into eight, four or two pixels in up to sixteen colours.

   Time is measured in ticks of the machine's 2 MHz clock, counted from the start
   of the frame. The picture is drawn up to an exact tick, so that a program that
   changes the palette or the mode halfway down the screen, or halfway across a
   line, gets what it would on a real Electron. */
class Video
{
	public:
		enum
		{
			/* the picture is always drawn at the resolution of the finest mode;
			its pixels are twice as tall as they are wide */
			WIDTH = 640, HEIGHT = 256,

			/* the lines the picture has in the two modes for text */
			TEXT_HEIGHT = 250,

			TICKS_PER_LINE = 128,

			/* the lines of the frame that carry the picture, and the ticks of
			those lines in which the ULA is reading RAM */
			FIRST_LINE = 56,
			ACTIVE_TICKS = 80
		};

		explicit Video(const uint8_t *RAM);
		void Reset();

		/* draws the picture up to a tick of the frame; to be called before
		anything changes that affects it */
		void RunTo(int Position);

		/* to be called at the end of the frame, after the picture is complete */
		void EndFrame();

		void SetMode(uint8_t Mode);
		uint8_t Mode() const { return CurrentMode; }

		/* true in the two modes that are meant for text: they leave two blank
		lines after every eight and show 250 lines rather than 256 */
		bool TextMode() const;

		/* true if the ULA is reading RAM for the picture in a tick of the frame,
		which is not before the tick that is being drawn */
		bool Fetching(int Position);

		/* the two registers that hold the address of the top left of the screen,
		and the eight that hold the palette */
		void SetStartLow(uint8_t Data) { StartLow = Data; }
		void SetStartHigh(uint8_t Data) { StartHigh = Data; }
		void SetPalette(int Register, uint8_t Data);

		/* the picture, WIDTH by HEIGHT pixels of 0xAARRGGBB */
		const uint32_t *Pixels() const { return Frame; }

		/* adds lines of text that say what state the chip is in, for a monitor */
		void Describe(std::vector<std::string> &Lines) const;

		/* saves the state, not including the picture, which is drawn anew every frame, to a stream, or loads it from one */
		void TransferState(StateStream &);

	private:
		const uint8_t *RAM;
		uint32_t Frame[WIDTH * HEIGHT];

		uint8_t CurrentMode;
		uint8_t StartLow, StartHigh;
		uint8_t Palette[8];
		uint32_t Colours[16];

		/* how far the picture has been drawn */
		int Rendered;

		/* address of the character row being drawn, and the line within it */
		uint16_t RowAddr;
		int RowLine;

		/* the byte being shown, in modes where one lasts two ticks */
		uint8_t Byte;

		void DrawColumn(int Line, int Column);
		void EndLine();
};

#endif
