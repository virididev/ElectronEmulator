#ifndef HOST_PROFILER_H
#define HOST_PROFILER_H

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "CPU/MOS6502.h"

class HostedMachine;

/* What the processor of a machine does, instruction by instruction: written to a
   file, a trace, or added up, a profile. The monitor has one of these.

   A trace has a line for every instruction: the processor's cycle count before
   it, its address, the instruction, and the registers after it. An interrupt has
   a line of its own.

   A profile adds up the time spent in every instruction and in every routine, as
   the machine keeps time: in microseconds, however fast or slow the processor is
   made to go. A routine is what a JSR goes to, and its time includes everything
   it calls, and the interrupts that come while it runs; an interrupt handler is
   a routine of its own. A routine ends when the stack is back where it was
   before the JSR, as it is after its RTS, or after a program has thrown the
   return address away. Time is only counted while the machine runs, so the
   monitor standing still makes no difference to it.

   Both watch the processor through MOS6502::Observer, which they take for as long
   as either of them is on. */
class Profiler : private MOS6502::Observer
{
	public:
		Profiler();
		~Profiler();

		/* starts a trace in a file, of no more than Most instructions; false if
		the file could not be made. Stopping returns how many were traced */
		bool StartTrace(HostedMachine &, const std::string &FileName, long Most);
		long StopTrace();
		bool Tracing() const { return Trace != NULL; }

		/* starts a profile afresh, or stops adding to it; what has been added up
		can be reported until the next start */
		void StartProfile(HostedMachine &);
		void StopProfile();
		bool Profiling() const { return Adding; }

		/* to be called whenever the machine has run a whole frame */
		void Frame() { if(Adding) Frames++; }

		/* The routines and the instructions that took the most time, at most
		Most of each */
		void Report(std::vector<std::string> &Lines, int Most);

		/* the time taken by the instructions from Start up to and including End */
		void ReportRange(std::vector<std::string> &Lines, uint16_t Start, uint16_t End);

	private:
		HostedMachine *Machine;

		/* the trace */
		FILE *Trace;
		long Traced, MostTraced;

		/* the profile: time and count for every address, for every routine and
		for every interrupt handler, the latter told apart by INTERRUPT */
		enum { INTERRUPT = 0x10000 };
		struct Tally
		{
			uint64_t Time, Count;
			Tally() : Time(0), Count(0) {}
		};
		bool Adding;
		std::vector<Tally> Instructions;
		std::map<uint32_t, Tally> Routines;
		Tally Interrupts;
		uint64_t Total, LastTicks, Frames;

		/* the routines that are running: which, where the stack was before they
		were called and when they were */
		struct Call
		{
			uint32_t Routine;
			uint8_t Stack;
			uint64_t Start;
		};
		std::vector<Call> Calls;

		void Executed(MOS6502 &, uint16_t Addr, uint64_t StartCycles, bool Interrupt);
		void Watch();
		std::string Time(uint64_t Ticks) const;
};

#endif
