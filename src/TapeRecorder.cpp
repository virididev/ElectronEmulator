#include "TapeRecorder.h"

namespace
{

/* the ticks a byte takes to send: ten bits at 1200 bits per second; bytes that
are further apart than a few of those have carrier tone between them */
const uint64_t TICKS_PER_BYTE = 10 * 2000000 / 1200;
const uint64_t LONGEST_PAUSE = 3 * TICKS_PER_BYTE;

/* a UEF file starts with this, zero terminated, and two bytes of version number */
const char Signature[] = "UEF File!";
const uint8_t VERSION_MINOR = 10, VERSION_MAJOR = 0;

enum { CHUNK_DATA = 0x0100, CHUNK_CARRIER = 0x0110, CHUNK_GAP = 0x0112 };

/* Lengths of carrier tone and silence in cycles of 2400 Hz. The Electron writes
some five seconds of tone before a file and about a second between blocks; much
less will do for a file that has no tape to stretch or motor to get up to speed */
const unsigned int FIRST_CARRIER = 3600;
const unsigned int BLOCK_CARRIER = 1200;
const unsigned int FINAL_GAP = 2400;

/* the first byte of every block on an Acorn tape */
const uint8_t BLOCK_SYNC = 0x2a;

void AddChunk(std::vector<uint8_t> &Image, unsigned int Type, const uint8_t *Data, size_t Length)
{
	Image.push_back((uint8_t)Type);
	Image.push_back((uint8_t)(Type >> 8));
	for(int Shift = 0; Shift < 32; Shift += 8) Image.push_back((uint8_t)(Length >> Shift));
	Image.insert(Image.end(), Data, Data + Length);
}

void AddChunk(std::vector<uint8_t> &Image, unsigned int Type, unsigned int Cycles)
{
	const uint8_t Data[2] = { (uint8_t)Cycles, (uint8_t)(Cycles >> 8) };
	AddChunk(Image, Type, Data, sizeof(Data));
}

}

TapeRecorder::TapeRecorder()
{
	Reset();
	Finished = false;
}

void TapeRecorder::Reset()
{
	Blocks.clear();
	LastByte = 0;
}

void TapeRecorder::Byte(uint8_t Data, uint64_t Tick)
{
	if(Blocks.empty() || Tick - LastByte > LONGEST_PAUSE) Blocks.push_back(std::vector<uint8_t>());
	Blocks.back().push_back(Data);
	LastByte = Tick;
}

void TapeRecorder::Stopped()
{
	if(Blocks.empty()) return;

	/* the name of the file follows the first byte of the first block */
	FinishedName.clear();
	const std::vector<uint8_t> &First = Blocks[0];
	if(First[0] == BLOCK_SYNC)
		for(size_t c = 1; c < First.size() && First[c]; c++) FinishedName += (char)First[c];

	FinishedImage.assign(Signature, Signature + sizeof(Signature));
	FinishedImage.push_back(VERSION_MINOR);
	FinishedImage.push_back(VERSION_MAJOR);

	for(size_t c = 0; c < Blocks.size(); c++)
	{
		AddChunk(FinishedImage, CHUNK_CARRIER, c ? BLOCK_CARRIER : FIRST_CARRIER);
		AddChunk(FinishedImage, CHUNK_DATA, Blocks[c].data(), Blocks[c].size());
	}
	AddChunk(FinishedImage, CHUNK_CARRIER, BLOCK_CARRIER);
	AddChunk(FinishedImage, CHUNK_GAP, FINAL_GAP);

	Finished = true;
	Reset();
}

bool TapeRecorder::Take(std::string &Name, std::vector<uint8_t> &Image)
{
	if(!Finished) return false;

	Name = FinishedName;
	Image.swap(FinishedImage);
	FinishedImage.clear();
	Finished = false;
	return true;
}
