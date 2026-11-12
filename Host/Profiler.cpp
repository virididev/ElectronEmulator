#include "Profiler.h"

#include <algorithm>

#include "HostedMachine.h"

namespace
{

/* the most routines a profile keeps track of at once; deeper than this, calls
are counted but not timed */
const size_t MOST_CALLS = 256;

const uint8_t OPCODE_JSR = 0x20;

/* an instruction as text */
std::string Instruction(const HostedMachine &Machine, uint16_t Addr)
{
	const uint8_t Bytes[3] = { Machine.Peek(Addr), Machine.Peek((uint16_t)(Addr + 1)), Machine.Peek((uint16_t)(Addr + 2)) };

	int Length;
	return MOS6502::Disassemble(Addr, Bytes, Length);
}

}

Profiler::Profiler() :
	Machine(NULL), Trace(NULL), Traced(0), MostTraced(0),
	Adding(false), Total(0), LastTicks(0), Frames(0)
{
}

Profiler::~Profiler()
{
	StopTrace();
	StopProfile();
}

/* The processor is watched for as long as there is anything to watch it for. */
void Profiler::Watch()
{
	if(!Machine) return;

	MOS6502 &CPU = Machine->Processor();
	if(Trace || Adding)
		CPU.Observe(this);
	else if(CPU.Observing() == this)
		CPU.Observe(NULL);
}

bool Profiler::StartTrace(HostedMachine &M, const std::string &FileName, long Most)
{
	StopTrace();

	Trace = fopen(FileName.c_str(), "w");
	if(!Trace) return false;

	Machine = &M;
	Traced = 0;
	MostTraced = Most;
	Watch();
	return true;
}

long Profiler::StopTrace()
{
	if(Trace)
	{
		fclose(Trace);
		Trace = NULL;
		Watch();
	}
	return Traced;
}

void Profiler::StartProfile(HostedMachine &M)
{
	Machine = &M;
	Adding = true;

	Instructions.assign(0x10000, Tally());
	Routines.clear();
	Interrupts = Tally();
	Calls.clear();
	Total = Frames = 0;
	LastTicks = Machine->Ticks();
	Watch();
}

void Profiler::StopProfile()
{
	if(!Adding) return;

	/* what is still running has run until now */
	const uint64_t Now = Machine->Ticks();
	for(size_t c = 0; c < Calls.size(); c++) Routines[Calls[c].Routine].Time += Now - Calls[c].Start;
	Calls.clear();

	Adding = false;
	Watch();
}

void Profiler::Executed(MOS6502 &CPU, uint16_t Addr, uint64_t StartCycles, bool Interrupt)
{
	if(Trace)
	{
		if(Interrupt)
			fprintf(Trace, "%10llu %04X interrupt, to %04X\n", (unsigned long long)StartCycles, Addr, CPU.PC);
		else
			fprintf(Trace, "%10llu %04X %-13s A %02X X %02X Y %02X S %02X P %02X\n",
				(unsigned long long)StartCycles, Addr, Instruction(*Machine, Addr).c_str(), CPU.A, CPU.X, CPU.Y, CPU.S, CPU.P);

		if(++Traced >= MostTraced)
		{
			fprintf(Trace, "the trace is full\n");
			fclose(Trace);
			Trace = NULL;
			Watch();
		}
	}

	if(!Adding) return;

	/* time that went backwards is a snapshot that was put back */
	const uint64_t Now = Machine->Ticks();
	const uint64_t Elapsed = Now >= LastTicks ? Now - LastTicks : 0;
	const uint64_t Start = Now - Elapsed;
	LastTicks = Now;
	Total += Elapsed;

	/* a call: an interrupt pushes three bytes, a JSR two */
	uint32_t Routine = 0;
	int Pushed = 0;
	if(Interrupt)
	{
		Interrupts.Time += Elapsed;
		Interrupts.Count++;
		Routine = INTERRUPT | CPU.PC;
		Pushed = 3;
	}
	else
	{
		Instructions[Addr].Time += Elapsed;
		Instructions[Addr].Count++;
		if(Machine->Peek(Addr) == OPCODE_JSR)
		{
			Routine = CPU.PC;
			Pushed = 2;
		}
	}

	if(Pushed)
	{
		Routines[Routine].Count++;

		/* a stack about to wrap around is not one to tell a return by */
		const int Before = CPU.S + Pushed;
		if(Before <= 0xff && Calls.size() < MOST_CALLS)
		{
			const Call New = { Routine, (uint8_t)Before, Start };
			Calls.push_back(New);
		}
	}

	/* the routines that have returned */
	while(!Calls.empty() && CPU.S >= Calls.back().Stack)
	{
		Routines[Calls.back().Routine].Time += Now - Calls.back().Start;
		Calls.pop_back();
	}
}

/* time as it is reported: on average a frame if whole frames were run, else all
of it, in microseconds */
std::string Profiler::Time(uint64_t Ticks) const
{
	char Text[32];
	snprintf(Text, sizeof(Text), "%10.1f", (double)Ticks / Machine->TicksPerMicrosecond() / (Frames ? (double)Frames : 1.0));
	return Text;
}

void Profiler::Report(std::vector<std::string> &Lines, int Most)
{
	/* the tallies are only there once a profile has been started */
	if(Instructions.empty())
	{
		Lines.push_back("there is no profile: pf starts one");
		return;
	}

	char Text[128];
	const double PerMicrosecond = Machine->TicksPerMicrosecond();
	snprintf(Text, sizeof(Text), "profile of %llu frames, %.0f us%s", (unsigned long long)Frames, Total / PerMicrosecond, Adding ? ", still adding up" : "");
	Lines.push_back(Text);
	if(!Total) return;

	const char *const Unit = Frames ? "us a frame" : "        us";

	/* what is still running counts as far as it has come */
	std::map<uint32_t, Tally> Sofar = Routines;
	const uint64_t Now = Adding ? Machine->Ticks() : 0;
	for(size_t c = 0; c < Calls.size(); c++) Sofar[Calls[c].Routine].Time += Now - Calls[c].Start;

	std::vector<std::pair<uint64_t, uint32_t> > Order;
	for(std::map<uint32_t, Tally>::const_iterator Next = Sofar.begin(); Next != Sofar.end(); ++Next)
		Order.push_back(std::make_pair(Next->second.Time, Next->first));
	std::sort(Order.rbegin(), Order.rend());

	Lines.push_back("");
	Lines.push_back("routines, with all they call and the interrupts that come meanwhile");
	snprintf(Text, sizeof(Text), "%s  share     calls  routine", Unit);
	Lines.push_back(Text);
	for(size_t c = 0; c < Order.size() && (int)c < Most; c++)
	{
		const Tally &It = Sofar[Order[c].second];
		snprintf(Text, sizeof(Text), "%s %5.1f%% %9llu  %04X%s", Time(It.Time).c_str(), 100.0 * It.Time / Total,
			(unsigned long long)It.Count, Order[c].second & 0xffff, (Order[c].second & INTERRUPT) ? " interrupt" : "");
		Lines.push_back(Text);
	}
	if(Order.empty()) Lines.push_back("none");

	Order.clear();
	for(uint32_t Addr = 0; Addr < Instructions.size(); Addr++)
		if(Instructions[Addr].Count) Order.push_back(std::make_pair(Instructions[Addr].Time, Addr));
	std::sort(Order.rbegin(), Order.rend());

	Lines.push_back("");
	Lines.push_back("instructions, by themselves");
	snprintf(Text, sizeof(Text), "%s  share     count  instruction", Unit);
	Lines.push_back(Text);
	for(size_t c = 0; c < Order.size() && (int)c < Most; c++)
	{
		const uint16_t Addr = (uint16_t)Order[c].second;
		const Tally &It = Instructions[Addr];
		snprintf(Text, sizeof(Text), "%s %5.1f%% %9llu  %04X %s", Time(It.Time).c_str(), 100.0 * It.Time / Total,
			(unsigned long long)It.Count, Addr, Instruction(*Machine, Addr).c_str());
		Lines.push_back(Text);
	}

	if(Interrupts.Count)
	{
		snprintf(Text, sizeof(Text), "%s %5.1f%% %9llu  starting interrupts", Time(Interrupts.Time).c_str(), 100.0 * Interrupts.Time / Total,
			(unsigned long long)Interrupts.Count);
		Lines.push_back(Text);
	}
}

void Profiler::ReportRange(std::vector<std::string> &Lines, uint16_t Start, uint16_t End)
{
	char Text[128];
	if(Instructions.empty())
	{
		Lines.push_back("there is no profile: pf starts one");
		return;
	}

	Tally Range;
	for(unsigned int Addr = Start; Addr <= End; Addr++)
	{
		Range.Time += Instructions[Addr].Time;
		Range.Count += Instructions[Addr].Count;
	}

	std::string Spent = Time(Range.Time);
	Spent.erase(0, Spent.find_first_not_of(' '));

	snprintf(Text, sizeof(Text), "%04X to %04X: %s %s, %.1f%%, %llu instructions", Start, End,
		Spent.c_str(), Frames ? "us a frame" : "us", 100.0 * Range.Time / Total, (unsigned long long)Range.Count);
	Lines.push_back(Text);
}
