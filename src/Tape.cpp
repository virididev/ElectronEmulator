#include "Tape.h"

#include <cstring>
#include <zlib.h>

namespace
{

/* a UEF file starts with this, zero terminated, and two bytes of version number */
const char Signature[] = "UEF File!";
const size_t HEADER_LENGTH = sizeof(Signature) + 2;

/* the chunks that make up a tape */
enum
{
	CHUNK_DATA = 0x0100,			/* bytes, each with a start and a stop bit */
	CHUNK_BITS = 0x0102,			/* bits as they are on the tape */
	CHUNK_DEFINED_DATA = 0x0104,	/* bytes in a format of the chunk's choosing */
	CHUNK_CARRIER = 0x0110,			/* carrier tone */
	CHUNK_CARRIER_DUMMY = 0x0111,	/* carrier tone with a dummy byte in it */
	CHUNK_GAP = 0x0112,				/* silence */
	CHUNK_SECURITY = 0x0114,		/* single cycles of tone, as copy protection */
	CHUNK_FLOAT_GAP = 0x0116		/* silence, with its length in seconds */
};

/* UEF measures tone and silence in cycles of the carrier tone, which are two
short pulses each */
const size_t PULSES_PER_CYCLE = 2;

/* a silence longer than this is a mistake in the file */
const double LONGEST_GAP = 600.0;

/* A CSW file starts with this and a byte of 0x1a. Then come the version, the
sample rate and how the lengths of the pulses are stored, laid out differently in
each of the two versions there are; the lengths are in samples */
const char CSWSignature[] = "Compressed Square Wave\x1a";
const size_t CSW_SIGNATURE_LENGTH = sizeof(CSWSignature) - 1;

enum
{
	CSW_MAJOR_VERSION = 0x17,

	CSW1_SAMPLE_RATE = 0x19, CSW1_COMPRESSION = 0x1b, CSW1_DATA = 0x20,

	CSW2_SAMPLE_RATE = 0x19, CSW2_COMPRESSION = 0x21, CSW2_EXTENSION_LENGTH = 0x23, CSW2_DATA = 0x34,

	CSW_RLE = 1, CSW_ZRLE = 2
};

/* A pulse in a recording is as long as the recorder ran fast or slow. One that
lasts less than half a short pulse is noise, one of up to one and a half is short,
one of up to three is long and anything beyond is silence */
const double NOISE_LIMIT = 0.5, SHORT_LIMIT = 1.5, LONG_LIMIT = 3.0;

/* the first byte of every block on an Acorn tape, and as much of a block as it
takes to tell what is in it */
const uint8_t BLOCK_SYNC = 0x2a;

/* what follows the name of the file in a block, and where in it the length of the
data is */
const size_t BLOCK_HEADER_LENGTH = 17, BLOCK_DATA_LENGTH = 10;
const size_t INSPECTED_LENGTH = 512;

unsigned int Word(const uint8_t *Data)
{
	return Data[0] | ((unsigned int)Data[1] << 8);
}

unsigned int LongWord(const uint8_t *Data)
{
	return Word(Data) | (Word(Data + 2) << 16);
}

/* undoes the compression of the second kind of CSW file */
bool Inflate(const uint8_t *Data, size_t Length, std::vector<uint8_t> &Result)
{
	z_stream Stream;
	memset(&Stream, 0, sizeof(Stream));
	if(inflateInit(&Stream) != Z_OK) return false;

	Stream.next_in = const_cast<Bytef *>(Data);
	Stream.avail_in = (uInt)Length;

	int Status;
	do
	{
		uint8_t Buffer[4096];
		Stream.next_out = Buffer;
		Stream.avail_out = sizeof(Buffer);

		Status = inflate(&Stream, Z_NO_FLUSH);
		if(Status != Z_OK && Status != Z_STREAM_END) break;
		Result.insert(Result.end(), Buffer, Buffer + (sizeof(Buffer) - Stream.avail_out));
	}
	while(Status == Z_OK && (Stream.avail_in || !Stream.avail_out));

	inflateEnd(&Stream);

	/* a recording that was cut short is still a recording */
	return Status == Z_STREAM_END || !Result.empty();
}

}

Tape::Tape() : Next(0), Motor(false), FirstFileIsBASIC(true)
{
}

/* zlib reads files that are not compressed just as well as those that are */
bool Tape::ReadFile(const char *Name, std::vector<uint8_t> &Contents)
{
	gzFile File = gzopen(Name, "rb");
	if(!File) return false;

	uint8_t Buffer[4096];
	int Length;
	while((Length = gzread(File, Buffer, sizeof(Buffer))) > 0)
		Contents.insert(Contents.end(), Buffer, Buffer + Length);

	gzclose(File);
	return Length == 0;
}

bool Tape::Open(const char *Name)
{
	std::vector<uint8_t> Contents;
	if(!ReadFile(Name, Contents)) return false;

	/* the tape that is in the recorder stays there if this one is no good */
	Tape Candidate;
	if(!Candidate.ParseUEF(Contents) && !Candidate.ParseCSW(Contents)) return false;
	if(Candidate.Pulses.empty()) return false;
	Candidate.InspectFirstBlock();

	Pulses.swap(Candidate.Pulses);
	FirstFileIsBASIC = Candidate.FirstFileIsBASIC;

	FileName = Name;
	const size_t Slash = FileName.find_last_of("/\\");
	if(Slash != std::string::npos) FileName.erase(0, Slash + 1);

	Rewind();
	return true;
}

void Tape::TransferState(StateStream &Stream)
{
	Stream.Vector(Pulses);
	Stream.String(FileName);
	Stream.Size(Next);
	Stream.Value(Motor);
	Stream.Value(FirstFileIsBASIC);
}

void Tape::Eject()
{
	Pulses.clear();
	FileName.clear();
	Rewind();
}

void Tape::Rewind()
{
	Next = 0;
}

int Tape::Position() const
{
	return Pulses.empty() ? 0 : (int)(Next * 100 / Pulses.size());
}

int Tape::NextPulse()
{
	return (Next < Pulses.size()) ? Pulses[Next++] : (int)PULSE_END;
}

void Tape::AddPulses(int Pulse, size_t Count)
{
	Pulses.insert(Pulses.end(), Count, (uint8_t)Pulse);
}

void Tape::AddBits(int Bit, size_t Count)
{
	if(Bit)
		AddPulses(PULSE_SHORT, Count * SHORT_PULSES_PER_BIT);
	else
		AddPulses(PULSE_LONG, Count * LONG_PULSES_PER_BIT);
}

void Tape::AddByte(uint8_t Byte, int DataBits, char Parity, int StopBits)
{
	AddBits(0, 1);

	int Ones = 0;
	for(int c = 0; c < DataBits; c++)
	{
		const int Bit = (Byte >> c) & 1;
		Ones += Bit;
		AddBits(Bit, 1);
	}

	if(Parity == 'E') AddBits(Ones & 1, 1);
	if(Parity == 'O') AddBits((Ones & 1) ^ 1, 1);

	AddBits(1, (size_t)StopBits);
}

/* Adds a pulse from a recording, by its length. */
void Tape::AddRecordedPulse(double Seconds)
{
	const double Length = Seconds * (double)PULSE_RATE;

	if(Length < NOISE_LIMIT) return;

	if(Length < SHORT_LIMIT) AddPulses(PULSE_SHORT, 1);
	else if(Length < LONG_LIMIT) AddPulses(PULSE_LONG, 1);
	else if(Seconds < LONGEST_GAP) AddPulses(PULSE_SILENCE, (size_t)(Length + 0.5));
}

/* Reads the start of the first block on the tape the way the ULA would: bits out
of pulses, and bytes out of bits wherever a start bit follows a stop bit. */
void Tape::ReadFirstBlock(std::vector<uint8_t> &Block) const
{
	int Kind = PULSE_SILENCE, Count = 0;

	/* the bits of the byte that is coming in, a start bit first; none of them
	as long as it is carrier tone that is coming in */
	unsigned int Byte = 0;
	int BitCount = 0;

	for(size_t c = 0; c < Pulses.size() && Block.size() < INSPECTED_LENGTH; c++)
	{
		const int Pulse = Pulses[c];
		if(Pulse != Kind)
		{
			Kind = Pulse;
			Count = 0;
		}

		if(Pulse == PULSE_SILENCE)
		{
			/* the block ends where the sound does */
			if(!Block.empty()) return;
			BitCount = 0;
			continue;
		}

		if(++Count < (Pulse == PULSE_SHORT ? (int)SHORT_PULSES_PER_BIT : (int)LONG_PULSES_PER_BIT)) continue;
		Count = 0;

		const unsigned int Bit = (Pulse == PULSE_SHORT) ? 1 : 0;
		if(!BitCount && Bit) continue;

		Byte |= Bit << BitCount;
		if(++BitCount < 10) continue;

		/* what came before the block starts is of no interest */
		const uint8_t Data = (uint8_t)(Byte >> 1);
		if(!Block.empty() || Data == BLOCK_SYNC) Block.push_back(Data);
		Byte = 0;
		BitCount = 0;
	}
}

/* Looks at the first block of the tape to tell a BASIC program from machine code.
A block is a name, seventeen bytes about the file and the block, a checksum and
then the data; a BASIC program is a row of lines that each start with a carriage
return, the number of the line and its length. */
void Tape::InspectFirstBlock()
{
	std::vector<uint8_t> Block;
	ReadFirstBlock(Block);

	FirstFileIsBASIC = true;

	size_t Index = 1;
	while(Index < Block.size() && Block[Index]) Index++;
	Index += 1 + BLOCK_HEADER_LENGTH + 2;
	if(Index > Block.size()) return;

	/* the data is as long as the header says, and a checksum follows it */
	size_t End = Index + Word(&Block[Index - 2 - BLOCK_HEADER_LENGTH + BLOCK_DATA_LENGTH]);
	if(End > Block.size()) End = Block.size();

	while(Index + 3 < End)
	{
		if(Block[Index] != 0x0d)
		{
			FirstFileIsBASIC = false;
			return;
		}

		/* a line number of 0xff and up marks the end of the program */
		if(Block[Index + 1] == 0xff || !Block[Index + 3]) return;
		Index += Block[Index + 3];
	}
}

bool Tape::ParseCSW(const std::vector<uint8_t> &Contents)
{
	if(Contents.size() < CSW2_DATA || memcmp(Contents.data(), CSWSignature, CSW_SIGNATURE_LENGTH)) return false;

	unsigned int SampleRate, Compression;
	size_t Start;

	if(Contents[CSW_MAJOR_VERSION] < 2)
	{
		SampleRate = Word(&Contents[CSW1_SAMPLE_RATE]);
		Compression = Contents[CSW1_COMPRESSION];
		Start = CSW1_DATA;
	}
	else
	{
		SampleRate = LongWord(&Contents[CSW2_SAMPLE_RATE]);
		Compression = Contents[CSW2_COMPRESSION];
		Start = (size_t)CSW2_DATA + Contents[CSW2_EXTENSION_LENGTH];
	}
	if(!SampleRate || Start > Contents.size()) return false;

	std::vector<uint8_t> Unpacked;
	const uint8_t *Data = Contents.data() + Start;
	size_t Length = Contents.size() - Start;

	if(Compression == CSW_ZRLE)
	{
		if(!Inflate(Data, Length, Unpacked)) return false;
		Data = Unpacked.data();
		Length = Unpacked.size();
	}
	else if(Compression != CSW_RLE)
		return false;

	/* a pulse is a byte, or a zero and four bytes if it is too long for one */
	for(size_t c = 0; c < Length; c++)
	{
		unsigned int Samples = Data[c];
		if(!Samples)
		{
			if(Length - c < 5) break;
			Samples = LongWord(&Data[c + 1]);
			c += 4;
		}

		AddRecordedPulse((double)Samples / (double)SampleRate);
	}

	return true;
}

bool Tape::ParseUEF(const std::vector<uint8_t> &Contents)
{
	if(Contents.size() < HEADER_LENGTH || memcmp(Contents.data(), Signature, sizeof(Signature))) return false;

	size_t Index = HEADER_LENGTH;

	while(Index + 6 <= Contents.size())
	{
		const unsigned int Type = Word(&Contents[Index]);
		const size_t Length = LongWord(&Contents[Index + 2]);
		Index += 6;
		if(Length > Contents.size() - Index) return false;

		const uint8_t *const Data = Contents.data() + Index;
		Index += Length;

		switch(Type)
		{
			case CHUNK_DATA:
				for(size_t c = 0; c < Length; c++) AddByte(Data[c], 8, 'N', 1);
			break;

			case CHUNK_DEFINED_DATA:
			{
				if(Length < 3) break;

				/* a negative number of stop bits means an extra short wave
				after them */
				const int StopBits = (int8_t)Data[2];
				for(size_t c = 3; c < Length; c++)
				{
					AddByte(Data[c], Data[0], (char)Data[1], StopBits < 0 ? -StopBits : StopBits);
					if(StopBits < 0) AddPulses(PULSE_SHORT, PULSES_PER_CYCLE);
				}
			}
			break;

			case CHUNK_BITS:
			{
				if(Length < 1) break;

				/* the first byte tells how many bits of the last one are unused */
				const size_t Total = (Length - 1) * 8;
				const size_t Count = (Data[0] < Total) ? Total - Data[0] : 0;
				for(size_t c = 0; c < Count; c++) AddBits((Data[1 + (c >> 3)] >> (c & 7)) & 1, 1);
			}
			break;

			case CHUNK_CARRIER:
				if(Length >= 2) AddPulses(PULSE_SHORT, Word(Data) * PULSES_PER_CYCLE);
			break;

			case CHUNK_CARRIER_DUMMY:
				if(Length < 4) break;
				AddPulses(PULSE_SHORT, Word(Data) * PULSES_PER_CYCLE);
				AddByte(0xaa, 8, 'N', 1);
				AddPulses(PULSE_SHORT, Word(Data + 2) * PULSES_PER_CYCLE);
			break;

			case CHUNK_GAP:
				if(Length >= 2) AddPulses(PULSE_SILENCE, Word(Data) * PULSES_PER_CYCLE);
			break;

			case CHUNK_FLOAT_GAP:
			{
				if(Length < 4) break;

				float Seconds;
				memcpy(&Seconds, Data, sizeof(Seconds));
				if(Seconds > 0.0f && Seconds < (float)LONGEST_GAP) AddPulses(PULSE_SILENCE, (size_t)(Seconds * (float)PULSE_RATE));
			}
			break;

			case CHUNK_SECURITY:
			{
				/* A number of waves, each of them short or long, a bit to a
				wave. Of the first wave only the second half may be there,
				and of the last one only the first half: a P for a pulse
				says so, where a W says it is a whole wave */
				if(Length < 5) break;

				size_t Waves = Word(Data) | ((size_t)Data[2] << 16);
				if(Waves > (Length - 5) * 8) Waves = (Length - 5) * 8;

				for(size_t c = 0; c < Waves; c++)
				{
					const bool Half = (!c && Data[3] == 'P') || (c == Waves - 1 && Data[4] == 'P');
					const int Short = (Data[5 + (c >> 3)] >> (c & 7)) & 1;
					AddPulses(Short ? PULSE_SHORT : PULSE_LONG, Half ? 1 : 2);
				}
			}
			break;

			default: break;
		}
	}

	return true;
}
