#include "Monitor.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "HostedMachine.h"

namespace
{

/* how much of everything is shown: instructions on the left and memory on the
right, line for line */
const int LISTING_LINES = 16;
const int BYTES_PER_LINE = 8;
const int STACK_BYTES = 16;
const int MOST_BREAKPOINTS_SHOWN = 8;

/* in a script there is no page to fit on: more bytes to a line, and limits that
are only there to keep a slip of the finger from flooding the output */
const int SCRIPT_BYTES_PER_LINE = 16;
const int SCRIPT_MEMORY_BYTES = 128;
const long MOST_LISTING_LINES = 4096;
const int MOST_DIFFERENCES_SHOWN = 32;

/* how much a trace holds, unless told otherwise, and at most; and how much of a
profile is reported */
const long TRACE_INSTRUCTIONS = 100000;
const long MOST_TRACE_INSTRUCTIONS = 100000000;
const long REPORT_LINES = 20;
const long MOST_REPORT_LINES = 65536;

/* where the report of a profile goes without a script */
const char ReportFile[] = "profile.txt";

/* the width of the column the instructions are in */
const int DISASSEMBLY_WIDTH = 20;

/* the most instructions a single command will run, so that a slip of the finger
does not leave the monitor hanging */
const long MOST_STEPS = 1000000;
const long MOST_FRAMES = 1000000;

/* how long g in a script waits for a breakpoint: a minute of the machine's time */
const long FRAMES_TO_BREAK = 3000;

const char FlagNames[] = "NV-BDIZC";

/* the letters that tell the kinds of breakpoint apart, in a list and in the file */
const struct { uint8_t Kind; char Letter; } BreakpointKinds[] =
{
	{ HostedMachine::BREAK_EXECUTE, 'x' }, { HostedMachine::BREAK_READ, 'r' }, { HostedMachine::BREAK_WRITE, 'w' }
};

/* The instruction at an address as text, and the address of the one after it. */
std::string Instruction(const HostedMachine &Machine, uint16_t Addr, uint16_t &Next)
{
	const uint8_t Bytes[3] = { Machine.Peek(Addr), Machine.Peek((uint16_t)(Addr + 1)), Machine.Peek((uint16_t)(Addr + 2)) };

	int Length;
	const std::string Text = MOS6502::Disassemble(Addr, Bytes, Length);
	Next = (uint16_t)(Addr + Length);
	return Text;
}

/* a hexadecimal number, which may start with a $ or an & */
bool ParseHex(const std::string &Text, unsigned int Limit, unsigned int &Value)
{
	const char *Start = Text.c_str();
	if(*Start == '$' || *Start == '&') Start++;
	if(!*Start || !SDL_isxdigit(*Start)) return false;

	char *End;
	const unsigned long Number = strtoul(Start, &End, 16);
	if(*End || Number > Limit) return false;

	Value = (unsigned int)Number;
	return true;
}

/* a count, in decimal, from 1 to Limit */
bool ParseCount(const std::string &Text, long Limit, long &Value)
{
	if(Text.empty()) return false;

	char *End;
	const long Number = strtol(Text.c_str(), &End, 10);
	if(*End || Number < 1 || Number > Limit) return false;

	Value = Number;
	return true;
}

/* The registers and the flags; a flag that is set is shown as a capital. */
std::string Registers(const MOS6502 &CPU)
{
	char FlagText[sizeof(FlagNames)];
	for(int c = 0; c < 8; c++) FlagText[c] = (CPU.P & (0x80 >> c)) ? FlagNames[c] : (char)SDL_tolower(FlagNames[c]);
	FlagText[8] = 0;

	char Text[96];
	snprintf(Text, sizeof(Text), "PC %04X A %02X X %02X Y %02X S %02X P %02X %s cycle %llu",
		CPU.PC, CPU.A, CPU.X, CPU.Y, CPU.S, CPU.P, FlagText, (unsigned long long)CPU.Cycles);
	return Text;
}

/* A line of memory: the address, the bytes and the bytes as characters. */
std::string MemoryLine(const HostedMachine &Machine, uint16_t Addr, int Count)
{
	char Text[8];
	snprintf(Text, sizeof(Text), "%04X", Addr);
	std::string Line = Text, Characters;

	for(int b = 0; b < Count; b++)
	{
		const uint8_t Byte = Machine.Peek((uint16_t)(Addr + b));
		snprintf(Text, sizeof(Text), " %02X", Byte);
		Line += Text;
		Characters += (Byte >= ' ' && Byte <= '~') ? (char)Byte : '.';
	}
	return Line + " " + Characters;
}

/* the text after the first so many words of a line, which may be a file name with
spaces in it */
std::string After(const std::string &Line, int Words)
{
	size_t Position = 0;
	for(int c = 0; c < Words; c++)
	{
		Position = Line.find_first_not_of(" \t", Position);
		Position = Line.find_first_of(" \t", Position);
		if(Position == std::string::npos) return std::string();
	}

	Position = Line.find_first_not_of(" \t", Position);
	if(Position == std::string::npos) return std::string();
	return Line.substr(Position, Line.find_last_not_of(" \t") + 1 - Position);
}

bool ReadFile(const std::string &FileName, std::vector<uint8_t> &Contents)
{
	FILE *const File = fopen(FileName.c_str(), "rb");
	if(!File) return false;

	uint8_t Buffer[4096];
	size_t Length;
	while((Length = fread(Buffer, 1, sizeof(Buffer), File)) > 0)
		Contents.insert(Contents.end(), Buffer, Buffer + Length);

	const bool Complete = !ferror(File);
	fclose(File);
	return Complete;
}

}

Monitor::Monitor() :
	Flags(0x10000, 0), BreakpointCount(0), DisassemblyAddr(0), MemoryAddr(0), FollowProcessor(true),
	NoteIsFailure(false), AnyFailure(false), Frames(0), ToBreak(false), ToTyped(false)
{
}

/* The file has a line for every address with a breakpoint: the address and the
letters of the kinds of breakpoint it has. */
void Monitor::KeepBreakpointsIn(const std::string &FileName)
{
	BreakpointFile = FileName;

	FILE *const File = fopen(FileName.c_str(), "r");
	if(!File) return;

	unsigned int Addr;
	char Letters[8];
	while(fscanf(File, "%x %7s", &Addr, Letters) == 2)
	{
		if(Addr > 0xffff) continue;

		for(const char *Letter = Letters; *Letter; Letter++)
			for(size_t c = 0; c < sizeof(BreakpointKinds) / sizeof(*BreakpointKinds); c++)
				if(*Letter == BreakpointKinds[c].Letter && !(Flags[Addr] & BreakpointKinds[c].Kind))
					ToggleBreakpoint((uint16_t)Addr, BreakpointKinds[c].Kind);
	}
	fclose(File);
}

void Monitor::SaveBreakpoints() const
{
	if(BreakpointFile.empty()) return;

	FILE *const File = fopen(BreakpointFile.c_str(), "w");
	if(!File) return;

	for(unsigned int Addr = 0; Addr < Flags.size(); Addr++)
	{
		if(!Flags[Addr]) continue;

		fprintf(File, "%04X ", Addr);
		for(size_t c = 0; c < sizeof(BreakpointKinds) / sizeof(*BreakpointKinds); c++)
			if(Flags[Addr] & BreakpointKinds[c].Kind) fputc(BreakpointKinds[c].Letter, File);
		fputc('\n', File);
	}
	fclose(File);
}

void Monitor::ToggleBreakpoint(uint16_t Addr, uint8_t Kind)
{
	const bool Had = Flags[Addr] != 0;
	Flags[Addr] ^= Kind;
	BreakpointCount += (Flags[Addr] != 0) - Had;
}

void Monitor::Opened(HostedMachine &Machine, const std::string &Reason)
{
	Note = Reason;
	Command.clear();
	Follow(Machine);
}

/* Keeps the instruction the processor is at in view. */
void Monitor::Follow(HostedMachine &Machine)
{
	if(!FollowProcessor) return;

	/* as long as the instruction is among those shown, the view stays put */
	const uint16_t PC = Machine.Processor().PC;
	uint16_t Addr = DisassemblyAddr;
	for(int c = 0; c < LISTING_LINES - 2; c++)
	{
		if(Addr == PC) return;
		Instruction(Machine, Addr, Addr);
	}

	DisassemblyAddr = PC;
}

Monitor::Result Monitor::Key(HostedMachine &Machine, SDL_Keycode Key)
{
	switch(Key)
	{
		case SDLK_ESCAPE:
			Frames = 0;
			ToBreak = ToTyped = false;
		return RESUME;

		case SDLK_RETURN:
		case SDLK_KP_ENTER:
		{
			const std::string Line = Command;
			Command.clear();
			return Execute(Machine, Line, NULL);
		}

		case SDLK_BACKSPACE:
			if(!Command.empty()) Command.erase(Command.size() - 1);
		return STAY;

		default:
			if(Key >= ' ' && Key <= '~' && Command.size() < COLUMNS - 4) Command += (char)SDL_toupper((int)Key);
		return STAY;
	}
}

Monitor::Result Monitor::Script(HostedMachine &Machine, const std::string &Line, std::vector<std::string> &Output)
{
	const size_t Start = Line.find_first_not_of(" \t");
	if(Start == std::string::npos || Line[Start] == '#') return STAY;

	const std::string Text = Line.substr(Start, Line.find_last_not_of(" \t") + 1 - Start);
	Output.push_back("> " + Text);

	const Result Outcome = Execute(Machine, Text, &Output);
	if(!Note.empty()) Output.push_back(NoteIsFailure ? "? " + Note : Note);
	return Outcome;
}

void Monitor::Halted(HostedMachine &Machine, const std::string &Reason, bool Failure, std::vector<std::string> &Output)
{
	if(Failure) AnyFailure = true;
	Output.push_back(Failure ? "? " + Reason : Reason);
	Where(Machine, Output);
	FollowProcessor = true;
	Follow(Machine);
}

/* For a script: the registers and the instruction the processor is at. */
void Monitor::Where(HostedMachine &Machine, std::vector<std::string> &Output) const
{
	const uint16_t PC = Machine.Processor().PC;
	uint16_t Next;
	char Text[16];
	snprintf(Text, sizeof(Text), "%04X ", PC);

	Output.push_back(Registers(Machine.Processor()));
	Output.push_back(Text + Instruction(Machine, PC, Next));
}

void Monitor::Fail(const std::string &Problem)
{
	Note = Problem;
	NoteIsFailure = AnyFailure = true;
}

std::string Monitor::PathOf(const std::string &FileName) const
{
	/* a name that is a path from the root already: /x, and on Windows also \x and C:x */
	const bool Absolute = !FileName.empty() && (FileName[0] == '/' || FileName[0] == '\\' ||
		(FileName.size() >= 2 && FileName[1] == ':' && SDL_isalpha(FileName[0])));
	if(FileDirectory.empty() || Absolute) return FileName;
	return FileDirectory + "/" + FileName;
}

Monitor::Result Monitor::Execute(HostedMachine &Machine, const std::string &Line, std::vector<std::string> *Output)
{
	std::vector<std::string> Words;
	std::istringstream Stream(Line);
	for(std::string Word; Stream >> Word; ) Words.push_back(Word);

	Note.clear();
	NoteIsFailure = false;
	Frames = 0;
	ToBreak = ToTyped = false;

	std::string Name = Words.empty() ? "S" : Words[0];
	for(size_t c = 0; c < Name.size(); c++) Name[c] = (char)SDL_toupper(Name[c]);

	const size_t Arguments = Words.empty() ? 0 : Words.size() - 1;
	unsigned int Addr = 0;
	const bool HasAddr = Arguments >= 1 && ParseHex(Words[1], 0xffff, Addr);
	char Text[64];

	if(Name == "Q" && !Arguments) return Output ? QUIT : RESUME;

	if(Name == "G" && (HasAddr || !Arguments) && Arguments <= 1)
	{
		if(HasAddr) Machine.Processor().PC = (uint16_t)Addr;

		/* a script is not to wait for ever */
		if(Output) Frames = FRAMES_TO_BREAK;
		ToBreak = true;
		return RESUME;
	}

	if(Name == "F" && Arguments <= 1)
	{
		if(!Arguments)
			Frames = 1;
		else if(!ParseCount(Words[1], MOST_FRAMES, Frames))
		{
			Fail("f takes a number from 1 to 1000000");
			return STAY;
		}
		return RESUME;
	}

	if(Name == "T" && Arguments)
	{
		/* a backslash followed by an n stands for RETURN */
		ToType = After(Line, 1);
		size_t Position;
		while((Position = ToType.find("\\n")) != std::string::npos) ToType.replace(Position, 2, "\n");

		Frames = MOST_FRAMES;
		ToTyped = true;
		return RESUME;
	}

	if(Name == "S" && Arguments <= 1)
	{
		long Count = 1;
		if(Arguments && !ParseCount(Words[1], MOST_STEPS, Count))
		{
			Fail("s takes a number from 1 to 1000000");
			return STAY;
		}

		while(Count--)
		{
			Machine.Step();
			if(Count && (Flags[Machine.Processor().PC] & HostedMachine::BREAK_EXECUTE))
			{
				Note = "stopped at a breakpoint";
				break;
			}
		}
		FollowProcessor = true;
		if(Output) Where(Machine, *Output);
	}
	else if((Name == "B" || Name == "BR" || Name == "BW") && HasAddr && Arguments == 1)
	{
		const uint8_t Kind = Name == "B" ? HostedMachine::BREAK_EXECUTE : (Name == "BR" ? HostedMachine::BREAK_READ : HostedMachine::BREAK_WRITE);
		ToggleBreakpoint((uint16_t)Addr, Kind);
		SaveBreakpoints();

		if(Output)
		{
			snprintf(Text, sizeof(Text), "breakpoint at %04X %s", Addr, (Flags[Addr] & Kind) ? "set" : "cleared");
			Note = Text;
		}
	}
	else if(Name == "BC" && !Arguments)
	{
		Flags.assign(Flags.size(), 0);
		BreakpointCount = 0;
		SaveBreakpoints();
	}
	else if(Name == "D" && (HasAddr || !Arguments) && Arguments <= (Output ? 2 : 1))
	{
		long Count = LISTING_LINES;
		if(Arguments == 2 && !ParseCount(Words[2], MOST_LISTING_LINES, Count))
		{
			Fail("the number of instructions is from 1 to 4096");
			return STAY;
		}

		FollowProcessor = !HasAddr;
		if(HasAddr) DisassemblyAddr = (uint16_t)Addr;

		if(Output)
		{
			const MOS6502 &CPU = Machine.Processor();
			uint16_t Listing = HasAddr ? (uint16_t)Addr : CPU.PC;
			while(Count--)
			{
				const uint16_t At = Listing;
				const std::string Instr = Instruction(Machine, Listing, Listing);
				snprintf(Text, sizeof(Text), "%c%c%04X ", (At == CPU.PC) ? '>' : ' ', (Flags[At] & HostedMachine::BREAK_EXECUTE) ? '*' : ' ', At);
				Output->push_back(Text + Instr);
			}
		}
	}
	else if(Name == "M" && (HasAddr || !Arguments) && Arguments <= (Output ? 2 : 1))
	{
		unsigned int End = 0;
		if(Arguments == 2 && (!ParseHex(Words[2], 0xffff, End) || End < Addr))
		{
			Fail("the end is an address from the start on");
			return STAY;
		}

		const int BytesPerLine = Output ? SCRIPT_BYTES_PER_LINE : BYTES_PER_LINE;
		const uint16_t Start = HasAddr ? (uint16_t)Addr : (uint16_t)(MemoryAddr + (Output ? 0 : LISTING_LINES * BYTES_PER_LINE));
		if(Arguments < 2) End = SDL_min((unsigned int)Start + SCRIPT_MEMORY_BYTES - 1, 0xffffu);
		MemoryAddr = Start;

		if(Output)
		{
			for(unsigned int At = Start; At <= End; At += BytesPerLine)
				Output->push_back(MemoryLine(Machine, (uint16_t)At, (int)SDL_min((unsigned int)BytesPerLine, End + 1 - At)));
			MemoryAddr = (uint16_t)(End + 1);
		}
	}
	else if(Name == "W" && HasAddr && Arguments >= 2)
	{
		/* nothing is written unless all of it makes sense */
		std::vector<uint8_t> Bytes;
		for(size_t c = 2; c < Words.size(); c++)
		{
			unsigned int Byte;
			if(!ParseHex(Words[c], 0xff, Byte))
			{
				Fail(Words[c] + " is not a byte");
				return STAY;
			}
			Bytes.push_back((uint8_t)Byte);
		}

		for(size_t c = 0; c < Bytes.size(); c++) Machine.Poke((uint16_t)(Addr + c), Bytes[c]);
		if(Machine.Peek((uint16_t)Addr) != Bytes[0]) Fail("that is not RAM");
	}
	else if(Name == "PC" && HasAddr && Arguments == 1)
	{
		Machine.Processor().PC = (uint16_t)Addr;
		FollowProcessor = true;
	}
	else if(Name == "R" && !Arguments)
	{
		/* on the page all of this is in view already */
		if(Output)
		{
			const MOS6502 &CPU = Machine.Processor();
			Where(Machine, *Output);

			std::string Stack = "stack";
			for(int c = 1; CPU.S + c <= 0xff; c++)
			{
				snprintf(Text, sizeof(Text), " %02X", Machine.Peek((uint16_t)(0x100 + CPU.S + c)));
				Stack += Text;
			}
			Output->push_back(Stack);

			Machine.DescribeHardware(*Output);

			std::string List = "breakpoints";
			if(!BreakpointCount) List += " none";
			for(unsigned int At = 0; At < Flags.size(); At++)
			{
				if(!Flags[At]) continue;

				snprintf(Text, sizeof(Text), " %04X", At);
				List += Text;
				for(size_t c = 0; c < sizeof(BreakpointKinds) / sizeof(*BreakpointKinds); c++)
					if(Flags[At] & BreakpointKinds[c].Kind) List += BreakpointKinds[c].Letter;
			}
			Output->push_back(List);
		}
	}
	else if(Name == "KD" || (Name == "KU" && Arguments))
	{
		/* nothing is pressed or let go unless all the keys are keys */
		std::vector<SDL_Scancode> Keys;
		for(size_t c = 1; c < Words.size(); c++)
		{
			std::string KeyName = Words[c];
			for(size_t i = 0; i < KeyName.size(); i++)
				if(KeyName[i] == '_') KeyName[i] = ' ';

			const SDL_Scancode Key = SDL_GetScancodeFromName(KeyName.c_str());
			if(Key == SDL_SCANCODE_UNKNOWN)
			{
				Fail(Words[c] + " is not a key");
				return STAY;
			}
			Keys.push_back(Key);
		}
		if(Keys.empty())
		{
			Fail("kd takes the keys to press");
			return STAY;
		}

		const bool Down = Name == "KD";
		for(size_t c = 0; c < Keys.size(); c++)
		{
			const std::vector<SDL_Scancode>::iterator Found = std::find(Held.begin(), Held.end(), Keys[c]);
			if(Down && Found == Held.end()) Held.push_back(Keys[c]);
			if(!Down && Found != Held.end()) Held.erase(Found);
			Machine.HostKey(Keys[c], Down);
		}
	}
	else if(Name == "KU")
	{
		for(size_t c = 0; c < Held.size(); c++) Machine.HostKey(Held[c], false);
		Held.clear();
	}
	else if(Name == "TR")
	{
		if(!Arguments)
		{
			if(!Watcher.Tracing())
				Note = "there is no trace to stop";
			else
			{
				snprintf(Text, sizeof(Text), "%ld instructions traced", Watcher.StopTrace());
				Note = Text;
			}
		}
		else
		{
			/* a count first, if there is a file name after it */
			long Most = TRACE_INSTRUCTIONS;
			const bool Counted = Arguments >= 2 && ParseCount(Words[1], MOST_TRACE_INSTRUCTIONS, Most);
			const std::string FileName = After(Line, Counted ? 2 : 1);

			if(!Watcher.StartTrace(Machine, PathOf(FileName), Most))
				Fail(FileName + " could not be made");
			else
			{
				snprintf(Text, sizeof(Text), "tracing at most %ld instructions in ", Most);
				Note = Text + FileName;
			}
		}
	}
	else if(Name == "PF" && Arguments <= 1)
	{
		std::string Off = Arguments ? Words[1] : "";
		for(size_t c = 0; c < Off.size(); c++) Off[c] = (char)SDL_toupper(Off[c]);

		if(Off == "OFF")
		{
			Watcher.StopProfile();
			Note = "the profile is no longer added to";
		}
		else if(Arguments)
			Fail("pf takes nothing, or off");
		else
		{
			Watcher.StartProfile(Machine);
			Note = "profiling";
		}
	}
	else if(Name == "PR" && Arguments <= 2)
	{
		std::vector<std::string> Report;
		long Count = REPORT_LINES;
		unsigned int End = 0;

		if(Arguments == 2)
		{
			if(!HasAddr || !ParseHex(Words[2], 0xffff, End) || End < Addr)
			{
				Fail("pr takes a start and an end address from the start on");
				return STAY;
			}
			Watcher.ReportRange(Report, (uint16_t)Addr, (uint16_t)End);
		}
		else
		{
			if(Arguments && !ParseCount(Words[1], MOST_REPORT_LINES, Count))
			{
				Fail("pr takes a number from 1 to 65536");
				return STAY;
			}
			Watcher.Report(Report, (int)Count);
		}

		if(Output)
			Output->insert(Output->end(), Report.begin(), Report.end());
		else
		{
			FILE *const File = fopen(PathOf(ReportFile).c_str(), "w");
			bool Complete = File != NULL;
			for(size_t c = 0; Complete && c < Report.size(); c++) Complete = fprintf(File, "%s\n", Report[c].c_str()) >= 0;
			if(File && fclose(File)) Complete = false;

			if(Complete)
				Note = std::string("report in ") + ReportFile;
			else
				Fail(std::string(ReportFile) + " could not be written");
		}
	}
	else if(Name == "SV" && HasAddr && Arguments >= 3)
	{
		unsigned int End;
		const std::string FileName = After(Line, 3);
		if(!ParseHex(Words[2], 0xffff, End) || End < Addr)
		{
			Fail("the end is an address from the start on");
			return STAY;
		}

		std::vector<uint8_t> Bytes;
		for(unsigned int At = Addr; At <= End; At++) Bytes.push_back(Machine.Peek((uint16_t)At));

		FILE *const File = fopen(PathOf(FileName).c_str(), "wb");
		const bool Complete = File && fwrite(Bytes.data(), 1, Bytes.size(), File) == Bytes.size();
		const bool Closed = File && !fclose(File);
		if(!Complete || !Closed)
			Fail(FileName + " could not be written");
		else
		{
			snprintf(Text, sizeof(Text), "%04X to %04X saved in ", Addr, End);
			Note = Text + FileName;
		}
	}
	else if((Name == "L" || Name == "C") && HasAddr && Arguments >= 2)
	{
		const std::string FileName = After(Line, 2);
		std::vector<uint8_t> Bytes;
		if(!ReadFile(PathOf(FileName), Bytes))
		{
			Fail(FileName + " could not be read");
			return STAY;
		}
		if(Bytes.empty() || Addr + Bytes.size() > 0x10000)
		{
			Fail(FileName + (Bytes.empty() ? " is empty" : " does not fit in from there"));
			return STAY;
		}
		const unsigned int End = Addr + (unsigned int)Bytes.size() - 1;

		if(Name == "L")
		{
			/* what does not stay is not RAM */
			int Lost = 0;
			for(size_t c = 0; c < Bytes.size(); c++)
			{
				Machine.Poke((uint16_t)(Addr + c), Bytes[c]);
				if(Machine.Peek((uint16_t)(Addr + c)) != Bytes[c]) Lost++;
			}

			if(Lost)
			{
				snprintf(Text, sizeof(Text), "%d of the bytes did not go in: that is not RAM", Lost);
				Fail(Text);
			}
			else
			{
				snprintf(Text, sizeof(Text), "%04X to %04X loaded from ", Addr, End);
				Note = Text + FileName;
			}
		}
		else
		{
			int Differences = 0;
			unsigned int First = 0;
			for(size_t c = 0; c < Bytes.size(); c++)
			{
				const uint16_t At = (uint16_t)(Addr + c);
				const uint8_t Byte = Machine.Peek(At);
				if(Byte == Bytes[c]) continue;

				if(!Differences++) First = At;
				if(Output && Differences <= MOST_DIFFERENCES_SHOWN)
				{
					snprintf(Text, sizeof(Text), "%04X %02X, in the file %02X", At, Byte, Bytes[c]);
					Output->push_back(Text);
				}
			}

			if(Differences)
			{
				snprintf(Text, sizeof(Text), "%d bytes differ, the first at %04X", Differences, First);
				Fail(Text);
			}
			else
			{
				snprintf(Text, sizeof(Text), "%04X to %04X are the same as ", Addr, End);
				Note = Text + FileName;
			}
		}
	}
	else
		Fail("not a command, or not how it is used");

	Follow(Machine);
	return STAY;
}

std::vector<std::string> Monitor::Lines(HostedMachine &Machine) const
{
	std::vector<std::string> Result;
	char Text[160];

	const MOS6502 &CPU = Machine.Processor();

	Result.push_back(std::string("MONITOR ") + Machine.Describe().Name + (Note.empty() ? "" : " - " + Note));
	Result.push_back(Registers(CPU));
	Result.push_back("");

	/* Instructions on the left: an arrow at the one the processor is at, a star
	at every breakpoint. Memory on the right, as bytes and as characters */
	uint16_t Listing = DisassemblyAddr, Memory = MemoryAddr;
	for(int c = 0; c < LISTING_LINES; c++)
	{
		const uint16_t Addr = Listing;
		const std::string Instr = Instruction(Machine, Listing, Listing);
		snprintf(Text, sizeof(Text), "%c%c%04X %-13s", (Addr == CPU.PC) ? '>' : ' ', (Flags[Addr] & HostedMachine::BREAK_EXECUTE) ? '*' : ' ', Addr, Instr.c_str());

		Result.push_back(std::string(Text, DISASSEMBLY_WIDTH) + " " + MemoryLine(Machine, Memory, BYTES_PER_LINE));
		Memory = (uint16_t)(Memory + BYTES_PER_LINE);
	}
	Result.push_back("");

	/* the stack, from the byte that was pushed last */
	std::string Stack = "stack";
	for(int c = 1; c <= STACK_BYTES && CPU.S + c <= 0xff; c++)
	{
		snprintf(Text, sizeof(Text), " %02X", Machine.Peek((uint16_t)(0x100 + CPU.S + c)));
		Stack += Text;
	}
	Result.push_back(Stack);
	Result.push_back("");

	Machine.DescribeHardware(Result);
	Result.push_back("");

	/* breakpoints, each with the letters of its kinds unless it is the usual one */
	std::string List = "breakpoints";
	if(!BreakpointCount) List += " none";
	int Shown = 0;
	for(unsigned int Addr = 0; Addr < Flags.size(); Addr++)
	{
		if(!Flags[Addr]) continue;
		if(++Shown > MOST_BREAKPOINTS_SHOWN)
		{
			snprintf(Text, sizeof(Text), " +%d", BreakpointCount - MOST_BREAKPOINTS_SHOWN);
			List += Text;
			break;
		}

		snprintf(Text, sizeof(Text), " %04X", Addr);
		List += Text;
		if(Flags[Addr] != HostedMachine::BREAK_EXECUTE)
			for(size_t c = 0; c < sizeof(BreakpointKinds) / sizeof(*BreakpointKinds); c++)
				if(Flags[Addr] & BreakpointKinds[c].Kind) List += BreakpointKinds[c].Letter;
	}
	Result.push_back(List);
	Result.push_back("");

	Result.push_back("> " + Command + "_");
	Result.push_back("");
	Result.push_back("s [n] step  g [addr] run  f [n] frames  q or Escape leave");
	Result.push_back("d [addr] code  m [addr] memory  w addr bytes  pc addr");
	Result.push_back("b addr break on run  br on read  bw on write  bc clear");
	Result.push_back("sv addr end file save  l addr file load  c addr file cmp");
	Result.push_back("t text type  kd key.. hold down  ku [key..] let go");
	Result.push_back("tr [n] file trace  tr stop  pf [off] profile  pr [n] report");

	/* nothing may be wider than the page */
	for(size_t c = 0; c < Result.size(); c++)
		if(Result[c].size() > COLUMNS) Result[c].resize(COLUMNS);
	return Result;
}
