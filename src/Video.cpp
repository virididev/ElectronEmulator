#include "Video.h"

#include <cstdio>

namespace
{

struct ModeDescription
{
	/* where the screen starts, which is also where the ULA carries on after
	reaching the end of RAM */
	uint16_t Base;

	int BytesPerLine, BitsPerPixel;

	/* text modes leave two blank lines after every eight and show 250 lines
	rather than 256 */
	bool Text;
};

const ModeDescription Modes[7] =
{
	{ 0x3000, 80, 1, false },
	{ 0x3000, 80, 2, false },
	{ 0x3000, 80, 4, false },
	{ 0x4000, 80, 1, true  },
	{ 0x5800, 40, 1, false },
	{ 0x5800, 40, 2, false },
	{ 0x6000, 40, 1, true  }
};

const int TEXT_LINES = Video::TEXT_HEIGHT;

/* the lines of a row of text that show something, and all of its lines */
const int TEXT_ROW_LINES = 8, TEXT_ROW_HEIGHT = 10;
const uint32_t Black = 0xff000000;

/* Where the red, green and blue of each colour are in the palette registers; a
set bit turns that part of the colour off. A colour is looked up by the four bits
of a pixel in the sixteen colour mode. The modes with fewer colours use the same
four bits with the missing ones at zero, which is why they only use colours 0, 2,
8 and 10 - all of which are in the first pair of registers. */
struct ColourBits
{
	uint8_t Colour;
	uint8_t RedRegister, RedBit, GreenRegister, GreenBit, BlueRegister, BlueBit;
};

const ColourBits ColourMap[16] =
{
	{  0, 1, 0x01, 1, 0x10, 0, 0x10 }, {  2, 1, 0x02, 1, 0x20, 0, 0x20 },
	{  8, 1, 0x04, 0, 0x04, 0, 0x40 }, { 10, 1, 0x08, 0, 0x08, 0, 0x80 },

	{  4, 3, 0x01, 3, 0x10, 2, 0x10 }, {  6, 3, 0x02, 3, 0x20, 2, 0x20 },
	{ 12, 3, 0x04, 2, 0x04, 2, 0x40 }, { 14, 3, 0x08, 2, 0x08, 2, 0x80 },

	{  5, 5, 0x01, 5, 0x10, 4, 0x10 }, {  7, 5, 0x02, 5, 0x20, 4, 0x20 },
	{ 13, 5, 0x04, 4, 0x04, 4, 0x40 }, { 15, 5, 0x08, 4, 0x08, 4, 0x80 },

	{  1, 7, 0x01, 7, 0x10, 6, 0x10 }, {  3, 7, 0x02, 7, 0x20, 6, 0x20 },
	{  9, 7, 0x04, 6, 0x04, 6, 0x40 }, { 11, 7, 0x08, 6, 0x08, 6, 0x80 }
};

}

Video::Video(const uint8_t *Memory) : RAM(Memory)
{
	Reset();
}

void Video::Reset()
{
	CurrentMode = 0;
	StartLow = StartHigh = 0;
	for(int c = 0; c < 8; c++) Palette[c] = 0;
	SetPalette(0, 0);

	Rendered = 0;
	RowAddr = 0;
	RowLine = 0;
	Byte = 0;

	for(int c = 0; c < WIDTH * HEIGHT; c++) Frame[c] = Black;
}

void Video::Describe(std::vector<std::string> &Lines) const
{
	char Text[128];
	snprintf(Text, sizeof(Text), "VIDEO mode %d  screen at %04X", CurrentMode, ((StartHigh & 0x3f) << 9) | ((StartLow & 0xe0) << 1));
	Lines.push_back(Text);

	snprintf(Text, sizeof(Text), "      palette %02X %02X %02X %02X %02X %02X %02X %02X",
		Palette[0], Palette[1], Palette[2], Palette[3], Palette[4], Palette[5], Palette[6], Palette[7]);
	Lines.push_back(Text);
}

void Video::TransferState(StateStream &Stream)
{
	Stream.Value(CurrentMode);
	Stream.Value(StartLow);
	Stream.Value(StartHigh);
	Stream.Bytes(Palette, sizeof(Palette));
	Stream.Value(Rendered);
	Stream.Value(RowAddr);
	Stream.Value(RowLine);
	Stream.Value(Byte);

	/* the colours follow from the palette registers */
	if(Stream.Loading() && !Stream.Failed()) SetPalette(0, Palette[0]);
}

void Video::SetMode(uint8_t Mode)
{
	/* there is an eighth mode, which is the same as the fifth */
	CurrentMode = (Mode & 7) == 7 ? 4 : (Mode & 7);
}

bool Video::TextMode() const
{
	return Modes[CurrentMode].Text;
}

bool Video::Fetching(int Position)
{
	const int Line = (Position / TICKS_PER_LINE) - FIRST_LINE;
	if(Line < 0 || (Position % TICKS_PER_LINE) >= ACTIVE_TICKS) return false;
	if(!Modes[CurrentMode].Text) return true;

	/* which lines are blank depends on where the rows of text are, and that
	is known once the picture has been drawn up to here */
	RunTo(Position);
	return RowLine < TEXT_ROW_LINES && Line < TEXT_LINES;
}

void Video::SetPalette(int Register, uint8_t Data)
{
	Palette[Register & 7] = Data;

	for(int c = 0; c < 16; c++)
	{
		const ColourBits &Map = ColourMap[c];
		Colours[Map.Colour] = Black |
			((Palette[Map.RedRegister] & Map.RedBit) ? 0 : 0xff0000) |
			((Palette[Map.GreenRegister] & Map.GreenBit) ? 0 : 0x00ff00) |
			((Palette[Map.BlueRegister] & Map.BlueBit) ? 0 : 0x0000ff);
	}
}

void Video::EndFrame()
{
	Rendered = 0;
	RowLine = 0;

	/* the address of the screen is taken from its registers once a frame; it
	is a multiple of 64 */
	RowAddr = (uint16_t)(((StartHigh & 0x3f) << 9) | ((StartLow & 0xe0) << 1));
}

void Video::EndLine()
{
	const ModeDescription &Mode = Modes[CurrentMode];

	if(++RowLine < (Mode.Text ? TEXT_ROW_HEIGHT : TEXT_ROW_LINES)) return;
	RowLine = 0;

	/* a character row is eight bytes for every character across */
	RowAddr += (uint16_t)(Mode.BytesPerLine * 8);
	if(RowAddr >= 0x8000) RowAddr = (uint16_t)(RowAddr - 0x8000 + Mode.Base);
}

/* Draws what the ULA puts on the screen in one tick: eight pixels of the finest
mode. */
void Video::DrawColumn(int Line, int Column)
{
	const ModeDescription &Mode = Modes[CurrentMode];
	uint32_t *Target = &Frame[Line*WIDTH + Column*8];

	if(Mode.Text && (RowLine >= TEXT_ROW_LINES || Line >= TEXT_LINES))
	{
		for(int c = 0; c < 8; c++) Target[c] = Black;
		return;
	}

	/* in the modes with 40 bytes to the line, a byte lasts two ticks */
	const bool Wide = Mode.BytesPerLine == 40;
	const bool SecondHalf = Wide && (Column & 1);

	if(!SecondHalf)
	{
		/* memory is laid out for text: the eight lines of a character follow
		each other, then come those of the character to its right */
		unsigned int Addr = RowAddr + (unsigned int)(Wide ? Column >> 1 : Column)*8 + (unsigned int)RowLine;
		if(Addr >= 0x8000) Addr = Addr - 0x8000 + Mode.Base;
		Byte = RAM[Addr];
	}

	/* the bits of a pixel are spread over the byte: with four pixels to the
	byte the leftmost is bits 7 and 3, with two it is bits 7, 5, 3 and 1 */
	const int PixelsPerByte = 8 / Mode.BitsPerPixel;
	const int PixelsPerTick = Wide ? PixelsPerByte / 2 : PixelsPerByte;
	const int PixelWidth = 8 / PixelsPerTick;
	const int First = SecondHalf ? PixelsPerTick : 0;

	for(int Pixel = First; Pixel < First + PixelsPerTick; Pixel++)
	{
		const unsigned int Bits = (unsigned int)Byte << Pixel;
		unsigned int Colour;

		switch(Mode.BitsPerPixel)
		{
			default:
			case 1: Colour = (Bits & 0x80) >> 4; break;
			case 2: Colour = ((Bits & 0x80) >> 4) | ((Bits & 0x08) >> 2); break;
			case 4: Colour = ((Bits & 0x80) >> 4) | ((Bits & 0x20) >> 3) | ((Bits & 0x08) >> 2) | ((Bits & 0x02) >> 1); break;
		}

		const uint32_t Value = Colours[Colour];
		for(int c = 0; c < PixelWidth; c++) *Target++ = Value;
	}
}

void Video::RunTo(int Position)
{
	while(Rendered < Position)
	{
		const int Line = (Rendered / TICKS_PER_LINE) - FIRST_LINE;
		const int Column = Rendered % TICKS_PER_LINE;

		if(Line < 0)
		{
			/* above the picture */
			const int PictureStart = FIRST_LINE * TICKS_PER_LINE;
			Rendered = (Position < PictureStart) ? Position : PictureStart;
		}
		else if(Column < ACTIVE_TICKS)
		{
			DrawColumn(Line, Column);
			Rendered++;
		}
		else
		{
			/* to the right of the picture */
			const int LineEnd = Rendered - Column + TICKS_PER_LINE;
			if(Position < LineEnd)
				Rendered = Position;
			else
			{
				EndLine();
				Rendered = LineEnd;
			}
		}
	}
}
