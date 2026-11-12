#ifndef HOST_HOSTEDMACHINE_H
#define HOST_HOSTEDMACHINE_H

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "CPU/MOS6502.h"
#include "Snapshot/StateStream.h"

/* What an emulated machine has to offer for the shell to be able to run it: see
   Shell.h. The emulator implements this once, as a thin layer over its hardware,
   and gets its window, its sound, its keys and its command line from the shell. */
class HostedMachine
{
	public:
		/* the kinds of breakpoint an address can have, as bits of its flag */
		enum { BREAK_EXECUTE = 0x01, BREAK_READ = 0x02, BREAK_WRITE = 0x04 };

		struct Description
		{
			/* name for the title bar, and the name of the program */
			const char *Name, *ProgramName;

			/* size of the picture; PixelHeight is how many times taller than wide
			a pixel is, and BorderX and BorderY are the margins left and right of
			the picture and above and below it, measured in pixel widths */
			int Width, Height, PixelHeight, BorderX, BorderY;

			/* initial size of the window, as a multiple of the picture's size */
			int Scale;

			/* rate of the samples the machine produces, in Hz */
			int SampleRate;

			/* time the machine needs after power on before it reads its
			keyboard, in frames */
			int BootFrames;

			/* what its tape images are called, and the extension of their
			file names */
			const char *TapeImageName, *TapeImageExtension;

			/* to be raised whenever what TransferState saves changes, so that
			a snapshot made before the change is not mistaken for one made
			after it */
			int SnapshotVersion;
		};

		virtual ~HostedMachine() {}

		virtual const Description &Describe() const = 0;

		/* loads the ROMs from a directory; on failure, Problem says what is wrong */
		virtual bool LoadROMs(const std::string &Directory, std::string &Problem) = 0;

		/* power off and on, and the machine's reset button or key */
		virtual void PowerOn() = 0;
		virtual void PressReset() = 0;

		/* Runs the machine for one frame of its display. If Breakpoints is not
		NULL it holds a flag for every address, made up of the BREAK_ bits, and
		the machine also stops when its processor is about to execute an
		instruction at an address with BREAK_EXECUTE, or has just run one that
		read an address with BREAK_READ or wrote one with BREAK_WRITE. In that
		case this returns true and the next call carries on with what is left of
		the frame */
		virtual bool RunFrame(const uint8_t *Breakpoints) = 0;

		/* if it was an address being read or written that stopped RunFrame, which
		address and which of the two */
		virtual bool AccessBreak(uint16_t &Addr, bool &Written) const = 0;

		/* how long a frame takes on the real machine */
		virtual Uint64 FrameNanoseconds() const = 0;

		/* For the monitor: the processor, running a single instruction, memory as
		the processor sees it - read without anything happening as a result, and
		written if it is RAM - and lines of text that say what state the rest of
		the hardware is in */
		virtual MOS6502 &Processor() = 0;
		virtual void Step() = 0;
		virtual uint8_t Peek(uint16_t Addr) const = 0;
		virtual void Poke(uint16_t Addr, uint8_t Data) = 0;
		virtual void DescribeHardware(std::vector<std::string> &Lines) const = 0;

		/* For the profiler: time as the machine keeps it, in ticks of its master
		clock, which runs on at the same rate however fast the processor goes; and
		how many of them make a microsecond. If the processor goes at a steady
		1 MHz, its cycles will do */
		virtual uint64_t Ticks() { return Processor().Cycles; }
		virtual int TicksPerMicrosecond() const { return 1; }

		/* the picture, Width by Height pixels of 0xAARRGGBB */
		virtual const Uint32 *Pixels() const = 0;

		/* the sound made since it was last cleared, in the range -1 to 1; the
		shell clears it */
		virtual std::vector<float> &Samples() = 0;

		/* a key of the host's keyboard going down or up */
		virtual void HostKey(SDL_Scancode, bool Down) = 0;

		/* Presses or releases whatever keys type a character on the machine, a
		newline being its RETURN key: with Modifiers the keys such as SHIFT that
		are held down with it, if any, and otherwise the key itself. False if it
		has no such keys. The modifiers go down a frame before the key and come up
		a frame after it, as they do when a person types: a machine that reads
		them at another moment than the key could otherwise miss them */
		virtual bool TypeCharacter(char Character, bool Down, bool Modifiers) = 0;

		/* the cassette recorder, or whatever else the machine takes its software
		from: a machine that has more than tapes tells by the file what it is */
		virtual bool InsertTape(const char *Name) = 0;
		virtual void EjectTape() = 0;
		virtual void RewindTape() = 0;
		virtual bool TapeInserted() const = 0;
		virtual bool TapeRunning() const = 0;
		virtual std::string TapeName() const = 0;
		virtual int TapePosition() const = 0;

		/* what the machine's own lights say, if it has any that are lit; it goes
		on the status bar */
		virtual std::string Indicators() const = 0;

		/* what to type to load and run the inserted tape; a machine that starts
		what was inserted in some other way does so now and has nothing typed */
		virtual std::string LoadCommand() = 0;

		/* Saves the state of the whole machine to a stream, or loads it from one.
		Called between frames */
		virtual void TransferState(StateStream &) = 0;

		/* An option on the command line that the shell does not know, with all
		that follows it on the command line. If it is one of the machine's own,
		the machine takes it to heart and returns how many of the arguments made
		up the option; if not, it returns zero. And the lines for those options
		in the list the shell shows the user */
		virtual int Option(int, char *const []) { return 0; }
		virtual const char *OptionsHelp() const { return ""; }

		/* The BASIC program that is in the machine's memory, as text that can be
		typed in again; false if there is none or the machine cannot tell */
		virtual bool ProgramText(std::string &) const { return false; }

		/* Puts this many kB of sideways RAM in the machine, in banks of 16, or
		takes it out; returns how much it now has from this setting */
		virtual int SetSidewaysRAM(int Kilobytes) = 0;

		/* Hands over what the machine has saved to tape since this was last
		called, if it has: a tape image and a name for the file to keep it in,
		extension included. The name is the one the machine gave what it saved
		and may be empty but for the extension */
		virtual bool TakeRecording(std::string &Name, std::vector<Uint8> &Image) = 0;
};

#endif
