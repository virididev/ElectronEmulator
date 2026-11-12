#ifndef HOST_MONITOR_H
#define HOST_MONITOR_H

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "Profiler.h"

class HostedMachine;

/* A machine code monitor: a look inside a machine that stands still.

   It shows the registers of the processor, the instructions it is about to
   execute next to a piece of memory, the stack and the state of the rest of the
   hardware, and takes commands typed at the bottom of the screen:

	s [n]				run one instruction, or n of them
	g [addr]			leave the monitor and run on, from an address if one is given
	f [n]				run on for one frame, or n of them, and come back
	b addr				stop when the instruction at an address is about to be run
	br addr				stop when an address has been read
	bw addr				stop when an address has been written
	bc					clear all breakpoints
	d [addr [n]]		show instructions from an address, or follow the processor again
	m [addr [end]]		show memory from an address, or the next piece of it
	w addr byte..		write bytes to memory
	pc addr				make the processor carry on at an address
	r					the registers, the stack, the hardware and the breakpoints
	sv addr end file	save memory from addr up to and including end in a file
	l addr file			load a file into memory from addr on
	c addr file			compare memory from addr on with a file
	kd key..			press keys and hold them down
	ku [key..]			let go of keys, or of all that kd holds down
	t text				type text, \n being RETURN, and come back when it is typed
	tr [n] file			trace every instruction in a file, at most n of them
	tr					stop tracing
	pf [off]			start a profile afresh, or stop adding to it
	pr [n]				the n routines and the n instructions that took the most time
	pr addr end			the time taken by the instructions from addr to end
	q					leave the monitor

   Each of b, br and bw clears the breakpoint if it is already there. Numbers are
   hexadecimal, and may start with $ or &, but for the counts of s, f and d.
   RETURN on an empty line is s. Breakpoints stop f and t as well as g.

   Keys are named as SDL names them, with _ for a space and in capitals or not:
   A, 1, Space, Return, Left, Left_Shift, Left_Ctrl, Escape, Caps_Lock... They are
   the keys of the host's keyboard, which the machine takes as it takes them from
   the keyboard itself. A key held down by kd stays down until ku, while the
   machine runs for f, g or t.

   A trace and a profile are explained in Profiler.h. A trace holds at most
   100000 instructions unless told otherwise. pr reports 20 of each unless told
   otherwise; without a script, it writes its report in profile.txt.

   Breakpoints are kept in a file, so they are still there in a later session.

   The same commands can come from a script: a file of them, a line each, given
   on the command line. What the monitor would show is then written out as text:
   the listing of d, the memory of m, all of r, and after s, and wherever running
   stops, the registers and the next instruction. The machine stands still while
   the script is read; g and f let it run, g until a breakpoint, but for no more
   than a minute of the machine's time, and f for a number of frames. q ends the
   emulator. A line that is empty or starts with # is passed over. A command that
   could not be carried out, a comparison that found a difference and a g that
   did not reach a breakpoint are failures; they are marked with a ? in what is
   written out, and the emulator ends with an exit status of 1. */
class Monitor
{
	public:
		enum Result { STAY, RESUME, QUIT };

		/* the page the monitor shows is at most this many characters wide */
		enum { COLUMNS = 59 };

		Monitor();

		/* names the file the breakpoints are kept in and reads it, if it is there */
		void KeepBreakpointsIn(const std::string &FileName);

		/* the directory a file named without one is in, if not the current one */
		void FilesIn(const std::string &Directory) { FileDirectory = Directory; }

		/* a flag for every address, made up of the BREAK_ bits of HostedMachine,
		or NULL if there are no breakpoints at all */
		const uint8_t *Breakpoints() const { return BreakpointCount ? Flags.data() : NULL; }

		/* to be called when the monitor comes up, with the reason why */
		void Opened(HostedMachine &, const std::string &Reason);

		/* a key pressed while the monitor is up */
		Result Key(HostedMachine &, SDL_Keycode Key);

		/* After RESUME: how many frames the machine is to run before the monitor
		takes over again, or zero if it is to run on for as long as it likes; and
		whether it was meant to stop at a breakpoint before then */
		long RunFrames() const { return Frames; }
		bool RunToBreak() const { return ToBreak; }

		/* After RESUME: text to type, and whether the machine is to run until it
		has been typed */
		std::string TakeText() { std::string Result; Result.swap(ToType); return Result; }
		bool RunToTyped() const { return ToTyped; }

		/* what the monitor shows */
		std::vector<std::string> Lines(HostedMachine &) const;

		/* One line of a script: carries out the command and adds to Output what
		there is to say about it */
		Result Script(HostedMachine &, const std::string &Line, std::vector<std::string> &Output);

		/* to be called when the machine has stopped running for a script, with the
		reason why and whether that is a failure */
		void Halted(HostedMachine &, const std::string &Reason, bool Failure, std::vector<std::string> &Output);

		/* to be called whenever the machine has run a whole frame */
		void Frame() { Watcher.Frame(); }

		/* true if anything in a script failed */
		bool Failed() const { return AnyFailure; }

	private:
		std::vector<uint8_t> Flags;
		int BreakpointCount;
		std::string BreakpointFile, FileDirectory;

		uint16_t DisassemblyAddr, MemoryAddr;
		bool FollowProcessor;

		std::string Command, Note;
		bool NoteIsFailure, AnyFailure;

		long Frames;
		bool ToBreak, ToTyped;
		std::string ToType;

		/* the keys kd holds down */
		std::vector<SDL_Scancode> Held;

		Profiler Watcher;

		/* carries out a command; Output is NULL unless it is from a script */
		Result Execute(HostedMachine &, const std::string &Line, std::vector<std::string> *Output);
		void Fail(const std::string &Problem);
		void Follow(HostedMachine &);
		void ToggleBreakpoint(uint16_t Addr, uint8_t Kind);
		void SaveBreakpoints() const;
		std::string PathOf(const std::string &FileName) const;
		void Where(HostedMachine &, std::vector<std::string> &Output) const;
};

#endif
