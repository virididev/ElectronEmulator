#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "Host/HostedMachine.h"
#include "Host/Shell.h"

#include "BASIC.h"
#include "Machine.h"

#define OS_ROM_NAME		"os.rom"
#define BASIC_ROM_NAME	"basic.rom"

/* The ULA fills only part of a television picture; what is left around it is
always black. A PAL set shows some 52 of the 64 microseconds of a line, of which
the picture takes 40, and some 288 lines, of which the picture takes 256. In the
pixels of the finest mode, which are 640 to a line and two to a line down, that
leaves this much on either side and above and below */
#define BORDER_X		96
#define BORDER_Y		32

/* A Mac says of its Caps Lock key that it went down and came up again at once,
whenever it is pressed. The Electron looks at its keyboard fifty times a second and
wants to see a key twice before it believes it, so a press of that key is made to
last at least this many frames */
#define CAPS_LOCK_FRAMES	6

/* to be raised whenever what a snapshot holds changes */
const int SNAPSHOT_VERSION = 4;

/* The slots the setting for sideways RAM fills, in this order: those that boards
for the Electron with sockets for sideways RAM usually have, and then the rest */
const int SIDEWAYS_RAM_SLOTS[] = { 4, 5, 6, 7, 0, 1, 2, 3, 12, 13, 14, 15 };

namespace
{

/* the Electron as the shell sees it */
class HostedElectron : public HostedMachine
{
	public:
		HostedElectron() : CapsLockFrames(0), CapsLockDown(false) {}

		const Description &Describe() const
		{
			static const Description Result =
			{
				"Acorn Electron", "acornelectron",
				Video::WIDTH, Video::HEIGHT, 2, BORDER_X, BORDER_Y,
				1,
				Sound::SAMPLE_RATE,
				100,
				"Tape images", "uef;csw",
				SNAPSHOT_VERSION
			};
			return Result;
		}

		/* what to put in the slots the Electron leaves empty */
		int Option(int Count, char *const Arguments[])
		{
			const std::string Argument = Arguments[0];

			SlotOption Slot;
			if(Argument == "--rom" && Count >= 3)
			{
				Slot.Number = atoi(Arguments[1]);
				Slot.FileName = Arguments[2];
				SlotOptions.push_back(Slot);
				return 3;
			}
			if(Argument == "--ram" && Count >= 2)
			{
				Slot.Number = atoi(Arguments[1]);
				SlotOptions.push_back(Slot);
				return 2;
			}
			return 0;
		}

		const char *OptionsHelp() const
		{
			return
				"  --rom <slot> <file>  put a ROM in one of the slots the Electron leaves\n"
				"                       empty: 0 to 7 and 12 to 15\n"
				"  --ram <slot>         put 16 kB of RAM in such a slot (sideways RAM)\n";
		}

		bool LoadROMs(const std::string &Directory, std::string &Problem)
		{
			const std::string OSName = Directory + "/" OS_ROM_NAME, BASICName = Directory + "/" BASIC_ROM_NAME;
			if(!Electron.LoadROMs(OSName.c_str(), BASICName.c_str()))
			{
				Problem = OSName + " and " + BASICName + ": not found, or not 16 kB ROMs";
				return false;
			}

			for(size_t c = 0; c < SlotOptions.size(); c++)
			{
				const SlotOption &Slot = SlotOptions[c];

				if(!Machine::SlotIsFree(Slot.Number))
				{
					Problem = "slot " + std::to_string(Slot.Number) + ": the free slots are 0 to 7 and 12 to 15";
					return false;
				}

				if(Slot.FileName.empty())
					Electron.PutRAMInSlot(Slot.Number);
				else if(!Electron.LoadSlot(Slot.Number, Slot.FileName.c_str()))
				{
					Problem = Slot.FileName + ": not found, or not a ROM of up to 16 kB";
					return false;
				}
			}
			return true;
		}

		void PowerOn() { Electron.PowerOn(); }
		void PressReset() { Electron.PressBreak(); }

		bool RunFrame(const uint8_t *Breakpoints)
		{
			if(CapsLockFrames && !--CapsLockFrames && !CapsLockDown)
				Electron.SetKey(Keyboard::KeyForScancode(SDL_SCANCODE_CAPSLOCK), false);

			return Electron.RunFrame(Breakpoints);
		}

		bool AccessBreak(uint16_t &Addr, bool &Written) const { return Electron.AccessBreak(Addr, Written); }

		MOS6502 &Processor() { return Electron.CPU; }
		void Step() { Electron.Step(); }
		uint8_t Peek(uint16_t Addr) const { return Electron.Peek(Addr); }
		void Poke(uint16_t Addr, uint8_t Data) { Electron.Poke(Addr, Data); }
		void DescribeHardware(std::vector<std::string> &Lines) const { Electron.Describe(Lines); }

		/* the processor does not go at a steady speed, the ULA's clock does */
		uint64_t Ticks() { return Electron.TheULA.Time(); }
		int TicksPerMicrosecond() const { return 2; }
		Uint64 FrameNanoseconds() const { return Electron.FrameNanoseconds(); }

		const Uint32 *Pixels() const { return Electron.Screen.Pixels(); }
		std::vector<float> &Samples() { return Electron.Speaker.Samples; }

		void HostKey(SDL_Scancode Scancode, bool Down)
		{
			if(Scancode == SDL_SCANCODE_CAPSLOCK)
			{
				/* it comes up when it has been down for long enough */
				CapsLockDown = Down;
				if(Down) CapsLockFrames = CAPS_LOCK_FRAMES;
				else if(CapsLockFrames) return;
			}

			Electron.SetKey(Keyboard::KeyForScancode(Scancode), Down);
		}

		bool TypeCharacter(char Character, bool Down, bool Modifiers)
		{
			int Key;
			bool Shift, Control;
			if(!Keyboard::KeyForCharacter(Character, Key, Shift, Control)) return false;

			if(!Modifiers)
				Electron.SetKey(Key, Down);
			else
			{
				if(Shift) Electron.SetKey(Keyboard::KEY_SHIFT, Down);
				if(Control) Electron.SetKey(Keyboard::KEY_CTRL, Down);
			}
			return true;
		}

		bool InsertTape(const char *Name) { return Electron.Cassette.Open(Name); }
		void EjectTape() { Electron.Cassette.Eject(); }
		void RewindTape() { Electron.Cassette.Rewind(); }
		bool TapeInserted() const { return Electron.Cassette.Inserted(); }
		bool TapeRunning() const { return Electron.TapeRunning(); }
		std::string TapeName() const { return Electron.Cassette.Name(); }
		int TapePosition() const { return Electron.Cassette.Position(); }

		std::string Indicators() const
		{
			return Electron.TheULA.CapsLockLED() ? "CAPS" : "";
		}

		std::string LoadCommand()
		{
			return Electron.Cassette.StartsWithBASIC() ? "CHAIN\"\"\n" : "*RUN\n";
		}

		bool TakeRecording(std::string &Name, std::vector<Uint8> &Image)
		{
			if(!Electron.Recorder.Take(Name, Image)) return false;
			Name += ".uef";
			return true;
		}

		bool ProgramText(std::string &Text) const
		{
			return BASIC::List(Electron.RAM, Electron.BASIC, Text) && !Text.empty();
		}

		void TransferState(StateStream &Stream)
		{
			Electron.TransferState(Stream);

			/* which slots the setting filled, so that it can empty them again */
			int Count = (int)RAMSlots.size();
			Stream.Value(Count);
			if(Stream.Loading()) RAMSlots.assign(Count >= 0 && Count <= Machine::NUM_SLOTS ? Count : 0, 0);
			for(size_t c = 0; c < RAMSlots.size(); c++) Stream.Value(RAMSlots[c]);
		}

		/* Sideways RAM as the settings have it, in kB: banks of 16 kB are put in
		empty slots or taken out again, the last that went in first. Neither
		needs the machine to be restarted; a game looks for sideways RAM when it
		is loaded. A slot filled from the command line is left as it is */
		int SetSidewaysRAM(int Kilobytes)
		{
			const size_t Banks = Kilobytes > 0 ? (size_t)Kilobytes / 16 : 0;

			while(RAMSlots.size() > Banks)
			{
				Electron.EmptySlot(RAMSlots.back());
				RAMSlots.pop_back();
			}

			const int NumSlots = (int)(sizeof(SIDEWAYS_RAM_SLOTS) / sizeof(*SIDEWAYS_RAM_SLOTS));
			for(int c = 0; c < NumSlots && RAMSlots.size() < Banks; c++)
			{
				const int Slot = SIDEWAYS_RAM_SLOTS[c];
				if(Electron.SlotIsEmpty(Slot) && Electron.PutRAMInSlot(Slot)) RAMSlots.push_back(Slot);
			}

			return (int)RAMSlots.size() * 16;
		}

	private:
		Machine Electron;

		/* the slots that SetSidewaysRAM filled, in the order it did */
		std::vector<int> RAMSlots;

		/* a slot to fill, with a ROM or, if no file is named, with RAM */
		struct SlotOption
		{
			int Number;
			std::string FileName;
		};
		std::vector<SlotOption> SlotOptions;

		/* how much longer Caps Lock has to stay down, in frames, and whether the
		host's key is down */
		int CapsLockFrames;
		bool CapsLockDown;
};

}

int main(int argc, char *argv[])
{
	/* a machine is too large an object for the stack */
	HostedElectron *const Electron = new HostedElectron;
	const int Result = RunEmulator(*Electron, argc, argv);
	delete Electron;
	return Result;
}
