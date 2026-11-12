#include "Shell.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

#include "AudioOutput.h"
#include "AutoType.h"
#include "Display.h"
#include "FrameLimiter.h"
#include "HostedMachine.h"
#include "Monitor.h"
#include "Settings.h"

/* at full speed there is little point in showing every frame */
#define WARP_FRAME_SKIP	10

/* how long a message stays in the title bar, in frames, and how often a paused
emulator looks whether it may carry on, in milliseconds */
#define MESSAGE_FRAMES	150
#define PAUSE_POLL_TIME	20

namespace
{

struct Options
{
	std::string ROMs, Saves, Tape, Text, Screenshot, Script;
	long Frames;
	bool Headless;

	/* these overrule a setting; -1 if not given */
	int Scale, Volume, FastTape, AutoLoad, FullScreen, SidewaysRAM;

	Options() : Frames(0), Headless(false), Scale(-1), Volume(-1), FastTape(-1), AutoLoad(-1), FullScreen(-1), SidewaysRAM(-1) {}
};

void PrintUsage(const HostedMachine &Hosted)
{
	const HostedMachine::Description &Machine = Hosted.Describe();

	printf(
		"usage: %s [options] [tape image, snapshot or program as text]\n"
		"\n"
		"%s"
		"  --roms <directory>   where the ROMs are, instead of the folder 'roms'\n"
		"                       next to the program or next to the settings\n"
		"  --saves <directory>  where to put what the machine saves to tape, instead\n"
		"                       of the current directory\n"
		"  --scale <n>          initial size of the window, default %d\n"
		"  --fullscreen         start full screen\n"
		"  --volume <n>         loudness of the sound, 0 to 100, default 100\n"
		"  --type <text>        type this once the machine has started; \\n is RETURN\n"
		"  --no-autoload        do not load a tape given on the command line\n"
		"  --no-fasttape        load tapes at their real speed\n"
		"  --sideways-ram <kB>  sideways RAM: 0, 16, 32, 48 or 64 kB\n"
		"  --frames <n>         quit after this many frames\n"
		"  --screenshot <file>  save the last picture as a BMP on quitting, at the\n"
		"                       size given by --scale\n"
		"  --headless           no window and no sound; for use with the two above\n"
		"  --monitor <file>     carry out the monitor commands in a file, or with -\n"
		"                       those on the standard input, and write out what\n"
		"                       they show; see the monitor (Shift+F10)\n"
		"\n"
		"The options for the window, the sound, tapes and sideways RAM overrule,\n"
		"for this once, what is set on the settings screen.\n"
		"\n"
		"keys: F1 open a tape image, a snapshot or a program as text, Shift+F1 save\n"
		"      the BASIC program as text, F2 settings, F3 keep the state of the\n"
		"      machine, Shift+F3 keep it in a file of your choice, F4 put\n"
		"      the state of F3 back, Shift+F4 put back the state from a file of\n"
		"      your choice, F5 reset, F6 power off and on, F7 rewind tape,\n"
		"      Shift+F7 take the tape out, F8 full speed, F9 type the text on the\n"
		"      clipboard (Escape stops it), F10 pause, Shift+F10 the monitor,\n"
		"      F11 full screen, F12 save a picture of the screen. A tape image\n"
		"      opened with F1 or dropped on the window is inserted and loaded; a\n"
		"      snapshot opened that way puts the machine back in the state it\n"
		"      holds; a program as text (.bas or .txt) is typed in.\n",
		Machine.ProgramName, Hosted.OptionsHelp(), Machine.Scale);
}

bool ParseOptions(int argc, char *argv[], HostedMachine &Machine, Options &Result)
{
	for(int c = 1; c < argc; c++)
	{
		const std::string Argument = argv[c];
		const bool HasValue = c + 1 < argc;
		int Used;

		if(Argument == "--roms" && HasValue) Result.ROMs = argv[++c];
		else if(Argument == "--saves" && HasValue) Result.Saves = argv[++c];
		else if(Argument == "--scale" && HasValue) Result.Scale = atoi(argv[++c]);
		else if(Argument == "--volume" && HasValue) Result.Volume = atoi(argv[++c]);
		else if(Argument == "--fullscreen") Result.FullScreen = 1;
		else if(Argument == "--frames" && HasValue) Result.Frames = atol(argv[++c]);
		else if(Argument == "--screenshot" && HasValue) Result.Screenshot = argv[++c];
		else if(Argument == "--monitor" && HasValue) Result.Script = argv[++c];
		else if(Argument == "--type" && HasValue)
		{
			/* a backslash followed by an n stands for RETURN */
			Result.Text = argv[++c];
			size_t Position;
			while((Position = Result.Text.find("\\n")) != std::string::npos)
				Result.Text.replace(Position, 2, "\n");
		}
		else if(Argument == "--headless") Result.Headless = true;
		else if(Argument == "--no-fasttape") Result.FastTape = 0;
		else if(Argument == "--no-autoload") Result.AutoLoad = 0;
		else if(Argument == "--sideways-ram" && HasValue)
		{
			Result.SidewaysRAM = atoi(argv[++c]);
			if(Result.SidewaysRAM < 0 || Result.SidewaysRAM > 64 || Result.SidewaysRAM % 16) return false;
		}
		else if((Used = Machine.Option(argc - c, argv + c)) > 0) c += Used - 1;
		else if(!Argument.empty() && Argument[0] != '-' && Result.Tape.empty()) Result.Tape = Argument;
		else return false;
	}

	if(Result.Volume > 100) Result.Volume = 100;
	return true;
}

/* The settings screen: a list of settings of which one is picked out. */
const struct MenuItem
{
	const char *Label;
	bool Settings::*Switch;

	/* for a number: where it is, its range and the size of a step */
	int Settings::*Number;
	int Minimum, Maximum, Step;
	const char *Unit;
} MenuItems[] =
{
	{ "Fast tape loading",    &Settings::FastTape,   NULL, 0, 0, 0, "" },
	{ "Load tapes at once",   &Settings::AutoLoad,   NULL, 0, 0, 0, "" },
	{ "Full screen",          &Settings::FullScreen, NULL, 0, 0, 0, "" },
	{ "Status bar",           &Settings::StatusBar,  NULL, 0, 0, 0, "" },
	{ "Scanlines",            &Settings::Scanlines,  NULL, 0, 0, 0, "" },
	{ "Vertical lines",       &Settings::VerticalLines, NULL, 0, 0, 0, "" },
	{ "Vignette",             &Settings::Vignette,   NULL, 0, 0, 0, "" },
	{ "Smooth picture",       &Settings::Smooth,     NULL, 0, 0, 0, "" },
	{ "Resume where I left off", &Settings::Resume,  NULL, 0, 0, 0, "" },
	{ "Sideways RAM",         NULL, &Settings::SidewaysRAM, 0, 64, 16, " kB" },
	{ "Volume",               NULL, &Settings::Volume, 0, 100, 10, "" },
	{ "Window size",          NULL, &Settings::Scale,  1, 6,   1,  "x" }
};

const int NUM_MENU_ITEMS = (int)(sizeof(MenuItems) / sizeof(*MenuItems));

/* the lines above the first setting */
const int MENU_HEADER_LINES = 2;

std::vector<std::string> MenuLines(const Settings &Current)
{
	std::vector<std::string> Lines;
	Lines.push_back(" SETTINGS");
	Lines.push_back("");

	for(int c = 0; c < NUM_MENU_ITEMS; c++)
	{
		const MenuItem &Item = MenuItems[c];

		char Value[16];
		if(Item.Switch)
			snprintf(Value, sizeof(Value), "%s", Current.*Item.Switch ? "on" : "off");
		else
			snprintf(Value, sizeof(Value), "%d%s", Current.*Item.Number, Item.Unit);

		char Line[64];
		snprintf(Line, sizeof(Line), " %-24s%6s", Item.Label, Value);
		Lines.push_back(Line);
	}

	Lines.push_back("");
	Lines.push_back(" Up, Down     choose");
	Lines.push_back(" Left, Right  change");
	Lines.push_back(" Escape       close");
	return Lines;
}

/* the picture tube effects as the settings have them */
Display::Effects EffectsOf(const Settings &Current)
{
	Display::Effects Result;
	Result.Scanlines = Current.Scanlines;
	Result.VerticalLines = Current.VerticalLines;
	Result.Vignette = Current.Vignette;
	Result.Smooth = Current.Smooth;
	return Result;
}

/* Changes the setting that is picked out: a switch flips, a number goes a step up
or down. True if that made a difference. */
bool ChangeSetting(Settings &Current, int Index, int Direction)
{
	const MenuItem &Item = MenuItems[Index];

	if(Item.Switch)
	{
		Current.*Item.Switch = !(Current.*Item.Switch);
		return true;
	}

	int Value = Current.*Item.Number + Direction * Item.Step;
	if(Value < Item.Minimum) Value = Item.Minimum;
	if(Value > Item.Maximum) Value = Item.Maximum;

	const bool Changed = Value != Current.*Item.Number;
	Current.*Item.Number = Value;
	return Changed;
}

/* Keeps what the machine has saved to tape in a file of its own. A file is only
ever overwritten if it was written earlier in this same session, as happens when a
program is saved again under the same name; any other file of that name is left
alone and a number is added to the new one. Returns the name used, or nothing if
the file could not be written. */
std::string SaveRecording(const std::string &Directory, const std::string &Name, const std::vector<Uint8> &Image, std::set<std::string> &Written)
{
	/* the name is the machine's, which may be no good as the name of a file */
	const size_t Dot = Name.find_last_of('.');
	std::string Base = Name.substr(0, Dot), Extension = (Dot == std::string::npos) ? "" : Name.substr(Dot);
	for(size_t c = 0; c < Base.size(); c++)
	{
		const char Character = Base[c];
		const bool Safe = SDL_isalnum(Character) || Character == ' ' || Character == '-' || Character == '_';
		if(!Safe) Base[c] = '_';
	}
	if(Base.empty()) Base = "saved";

	std::string Path;
	for(int Number = 1; Number < 1000; Number++)
	{
		char Suffix[16] = "";
		if(Number > 1) snprintf(Suffix, sizeof(Suffix), " %d", Number);
		Path = (Directory.empty() ? "" : Directory + "/") + Base + Suffix + Extension;

		if(Written.count(Path)) break;

		FILE *const Existing = fopen(Path.c_str(), "rb");
		if(!Existing) break;
		fclose(Existing);
		Path.clear();
	}
	if(Path.empty()) return Path;

	FILE *const File = fopen(Path.c_str(), "wb");
	if(!File) return std::string();

	const bool Complete = fwrite(Image.data(), 1, Image.size(), File) == Image.size();
	if(fclose(File) || !Complete) return std::string();

	Written.insert(Path);
	return Path;
}

/* A snapshot file is the state of a machine with, in front of it, something to
tell it by, the name of the emulator it belongs to and the version of that
emulator's snapshots. */
const char SnapshotSignature[] = "EMUSNAP";

/* the extension of the name of a snapshot file */
#define SNAPSHOT_EXTENSION	"state"

void SnapshotHeader(StateStream &Stream, const HostedMachine &Machine)
{
	Stream.Expect(std::string(SnapshotSignature));
	Stream.Expect(std::string(Machine.Describe().ProgramName));
	Stream.Expect(Machine.Describe().SnapshotVersion);
}

bool SaveSnapshot(HostedMachine &Machine, const std::string &FileName)
{
	StateStream Stream;
	SnapshotHeader(Stream, Machine);
	Machine.TransferState(Stream);

	FILE *const File = fopen(FileName.c_str(), "wb");
	if(!File) return false;

	const std::vector<uint8_t> &Contents = Stream.Contents();
	const bool Complete = fwrite(Contents.data(), 1, Contents.size(), File) == Contents.size();
	return !fclose(File) && Complete;
}

/* True if a file is a snapshot, of whatever emulator or version. */
bool IsSnapshot(const std::string &FileName)
{
	FILE *const File = fopen(FileName.c_str(), "rb");
	if(!File) return false;

	/* the signature is a piece of text in the stream, which has its length in
	front of it */
	uint8_t Start[sizeof(uint64_t) + sizeof(SnapshotSignature) - 1];
	const bool Complete = fread(Start, 1, sizeof(Start), File) == sizeof(Start);
	fclose(File);

	return Complete && !memcmp(Start + sizeof(uint64_t), SnapshotSignature, sizeof(SnapshotSignature) - 1);
}

enum SnapshotResult { SNAPSHOT_LOADED, SNAPSHOT_MISSING, SNAPSHOT_UNFIT, SNAPSHOT_BROKEN };

SnapshotResult LoadSnapshot(HostedMachine &Machine, const std::string &FileName)
{
	FILE *const File = fopen(FileName.c_str(), "rb");
	if(!File) return SNAPSHOT_MISSING;

	std::vector<uint8_t> Contents;
	uint8_t Buffer[4096];
	size_t Length;
	while((Length = fread(Buffer, 1, sizeof(Buffer), File)) > 0)
		Contents.insert(Contents.end(), Buffer, Buffer + Length);
	fclose(File);

	/* as long as it is only the header that is wrong, the machine is untouched */
	StateStream Stream(Contents);
	SnapshotHeader(Stream, Machine);
	if(Stream.Failed()) return SNAPSHOT_UNFIT;

	Machine.TransferState(Stream);
	return Stream.Failed() ? SNAPSHOT_BROKEN : SNAPSHOT_LOADED;
}

/* Puts the machine in the state a snapshot file holds and returns what there is
to tell the user about it. */
/* A line of a script, without its end; false at the end of the script. */
bool ReadLine(FILE *File, std::string &Line)
{
	Line.clear();

	char Buffer[256];
	while(fgets(Buffer, sizeof(Buffer), File))
	{
		Line += Buffer;
		if(Line[Line.size() - 1] == '\n') break;
	}
	if(Line.empty()) return false;

	while(!Line.empty() && (Line[Line.size() - 1] == '\n' || Line[Line.size() - 1] == '\r'))
		Line.erase(Line.size() - 1);
	return true;
}

void WriteLines(const std::vector<std::string> &Lines)
{
	for(size_t c = 0; c < Lines.size(); c++) printf("%s\n", Lines[c].c_str());
	fflush(stdout);
}

std::string RestoreSnapshot(HostedMachine &Machine, const std::string &FileName, const char *Success)
{
	switch(LoadSnapshot(Machine, FileName))
	{
		case SNAPSHOT_LOADED: return Success;
		case SNAPSHOT_MISSING: return "no state has been kept yet";
		case SNAPSHOT_UNFIT: return "that state is of another emulator or another version";

		default:
			/* the machine is neither what it was nor what it should be */
			Machine.PowerOn();
			return "that state is damaged; the machine was restarted";
	}
}

/* Text made fit for typing: whatever the line endings were, they become single
newlines, and tabs become spaces. */
std::string TypableText(const char *Text)
{
	std::string Result;
	for(const char *Next = Text; *Next; Next++)
	{
		switch(*Next)
		{
			case '\r':
				Result += '\n';
				if(Next[1] == '\n') Next++;
			break;

			case '\t': Result += ' '; break;
			default: Result += *Next; break;
		}
	}

	/* a last line without an end would be left sitting on the input line */
	if(!Result.empty() && Result[Result.size() - 1] != '\n') Result += '\n';
	return Result;
}

/* the text on the clipboard, fit for typing */
std::string ClipboardText()
{
	char *const Clipboard = SDL_GetClipboardText();
	if(!Clipboard) return std::string();

	const std::string Result = TypableText(Clipboard);
	SDL_free(Clipboard);
	return Result;
}

/* A program as text is a file the emulator types in, as it does what is on the
clipboard, and what it writes when asked to save the program in memory. Such a
file is told by the extension of its name */
#define PROGRAM_EXTENSION	"bas"
#define TEXT_EXTENSIONS		PROGRAM_EXTENSION ";txt"

bool HasExtension(const std::string &FileName, const char *Extension)
{
	const size_t Dot = FileName.find_last_of('.');
	return Dot != std::string::npos && !SDL_strcasecmp(FileName.c_str() + Dot + 1, Extension);
}

bool IsTextFile(const std::string &FileName)
{
	return HasExtension(FileName, PROGRAM_EXTENSION) || HasExtension(FileName, "txt");
}

/* Reads a file of text, fit for typing; nothing if it cannot be read or is not
text after all. */
std::string TextFromFile(const std::string &FileName)
{
	FILE *const File = fopen(FileName.c_str(), "rb");
	if(!File) return std::string();

	std::string Contents;
	char Buffer[4096];
	size_t Length;
	while((Length = fread(Buffer, 1, sizeof(Buffer), File)) > 0) Contents.append(Buffer, Length);
	fclose(File);

	if(Contents.find('\0') != std::string::npos) return std::string();
	return TypableText(Contents.c_str());
}

bool SaveText(const std::string &FileName, const std::string &Text)
{
	FILE *const File = fopen(FileName.c_str(), "wb");
	if(!File) return false;

	const bool Complete = fwrite(Text.data(), 1, Text.size(), File) == Text.size();
	return !fclose(File) && Complete;
}

/* adds an extension to the name of a file if it is not there yet */
std::string WithExtension(const std::string &FileName, const char *Extension)
{
	return HasExtension(FileName, Extension) ? FileName : FileName + "." + Extension;
}

/* A name for a picture of the screen: that of the machine and the date and time,
in the user's folder for pictures if there is one. */
std::string ScreenshotName(const HostedMachine &Machine)
{
	char Stamp[32] = "";

	SDL_Time Now;
	SDL_DateTime Time;
	if(SDL_GetCurrentTime(&Now) && SDL_TimeToDateTime(Now, &Time, true))
		snprintf(Stamp, sizeof(Stamp), " %04d-%02d-%02d %02d.%02d.%02d", Time.year, Time.month, Time.day, Time.hour, Time.minute, Time.second);

	const char *Folder = SDL_GetUserFolder(SDL_FOLDER_PICTURES);
	return std::string(Folder ? Folder : "") + Machine.Describe().Name + Stamp + ".bmp";
}

/* What the status bar says on the left: what there is to tell the user, or else
which tape is in the recorder. */
std::string StatusLeft(const HostedMachine &Machine, const std::string &Message)
{
	if(!Message.empty()) return Message;

	if(!Machine.TapeInserted()) return "no tape";

	/* the name last, so that it is the name that goes if there is no room */
	char Position[32];
	snprintf(Position, sizeof(Position), "%s %d%% ", Machine.TapeRunning() ? "playing" : "tape", Machine.TapePosition());
	return Position + Machine.TapeName();
}

/* And on the right: the lights of the machine and the state of the emulator. */
std::string StatusRight(const HostedMachine &Machine, bool Warp, bool Paused)
{
	std::string Result = Machine.Indicators();

	const char *const State = Paused ? "PAUSED" : (Warp ? "FULL SPEED" : "");
	if(*State)
	{
		if(!Result.empty()) Result += "  ";
		Result += State;
	}
	return Result;
}

/* The title of the window, which has to say it all if there is no status bar. */
std::string Title(const HostedMachine &Machine, bool Warp, bool Paused, const std::string &Message, bool StatusBar)
{
	std::string Result = Machine.Describe().Name;
	if(StatusBar) return Result;

	if(!Message.empty()) return Result + " - " + Message;
	if(Paused) return Result + " - paused";

	if(Machine.TapeInserted())
	{
		char Position[16];
		snprintf(Position, sizeof(Position), " %d%%", Machine.TapePosition());
		Result += " - " + Machine.TapeName() + Position;
		if(Machine.TapeRunning()) Result += " (playing)";
	}

	if(Warp) Result += " - full speed";
	return Result;
}

}

int RunEmulator(HostedMachine &Machine, int argc, char *argv[])
{
	Options Opts;
	if(!ParseOptions(argc, argv, Machine, Opts))
	{
		PrintUsage(Machine);
		return 1;
	}

	/* not before the machine has seen its options, which may change what it is */
	const HostedMachine::Description &Description = Machine.Describe();

	if(!SDL_Init(Opts.Headless ? 0 : (SDL_INIT_VIDEO | SDL_INIT_AUDIO)))
	{
		fprintf(stderr, "could not start SDL: %s\n", SDL_GetError());
		return 1;
	}

	const char *const Base = SDL_GetBasePath();

	/* An application on a Mac is not started from a directory of the user's
	choice, so what it saves goes with the user's documents */
	if(Opts.Saves.empty() && Base && strstr(Base, ".app/Contents/"))
	{
		const char *const Documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
		if(Documents) Opts.Saves = std::string(Documents, strlen(Documents) - (*Documents ? 1 : 0));
	}

	/* The settings as they are kept, and as they are in force: the same, but for
	what the command line overrules. A change on the settings screen is a change
	to both */
	std::string SettingsFile;
	if(char *const Folder = SDL_GetPrefPath("Emulators", Description.ProgramName))
	{
		SettingsFile = std::string(Folder) + "settings.cfg";
		SDL_free(Folder);
	}

	/* next to the settings: the snapshot of F3 and F4, and the state the machine
	was in when the emulator was last quit */
	const std::string SettingsFolder = SettingsFile.empty() ? "" : SettingsFile.substr(0, SettingsFile.size() - strlen("settings.cfg"));
	const std::string SnapshotFile = SettingsFolder.empty() ? "" : SettingsFolder + "snapshot." SNAPSHOT_EXTENSION;
	const std::string ResumeFile = SettingsFolder.empty() ? "" : SettingsFolder + "resume." SNAPSHOT_EXTENSION;

	Settings Kept;
	if(!SettingsFile.empty()) Kept.Load(SettingsFile);
	if(Kept.Scale < 1) Kept.Scale = Description.Scale;

	Settings Current = Kept;
	if(Opts.Scale >= 1) Current.Scale = Opts.Scale;
	if(Opts.Volume >= 0) Current.Volume = Opts.Volume;
	if(Opts.FastTape >= 0) Current.FastTape = Opts.FastTape != 0;
	if(Opts.AutoLoad >= 0) Current.AutoLoad = Opts.AutoLoad != 0;
	if(Opts.FullScreen >= 0) Current.FullScreen = Opts.FullScreen != 0;
	if(Opts.SidewaysRAM >= 0) Current.SidewaysRAM = Opts.SidewaysRAM;

	/* Where the ROMs are looked for: where the command line says, or else in the
	folder 'roms' next to the program (in a Mac application, among its
	resources), and then in the folder 'roms' next to the settings, which stays
	when a new version of the program takes the place of the old one */
	std::vector<std::string> ROMFolders;
	const std::string UserROMs = SettingsFolder.empty() ? "" : SettingsFolder + "roms";
	if(!Opts.ROMs.empty())
		ROMFolders.push_back(Opts.ROMs);
	else
	{
		ROMFolders.push_back(std::string(Base ? Base : "") + "roms");
		if(!UserROMs.empty()) ROMFolders.push_back(UserROMs);
	}

	std::string Problem;
	if(!Machine.LoadROMs(ROMFolders, Problem))
	{
		/* the folder next to the settings is made, so that it is there to be
		found and the ROMs can be put in it */
		if(Opts.ROMs.empty() && !UserROMs.empty()) SDL_CreateDirectory(UserROMs.c_str());

		fprintf(stderr, "%s\n", Problem.c_str());
		if(!Opts.Headless)
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, Description.Name, Problem.c_str(), NULL);
		SDL_Quit();
		return 1;
	}
	if(Machine.SetSidewaysRAM(Current.SidewaysRAM) < Current.SidewaysRAM)
		fprintf(stderr, "not enough empty slots for %d kB of sideways RAM\n", Current.SidewaysRAM);
	Machine.PowerOn();

	/* the script of monitor commands, if there is one */
	FILE *Script = NULL;
	if(Opts.Script == "-")
		Script = stdin;
	else if(!Opts.Script.empty() && !(Script = fopen(Opts.Script.c_str(), "r")))
	{
		fprintf(stderr, "%s: could not be read\n", Opts.Script.c_str());
		return 1;
	}

	AutoType Typist(Machine);

	/* something to tell the user, shown in the title bar for a while */
	std::string Message;
	int MessageCountdown = 0;

	if(!Opts.Tape.empty())
	{
		/* what is named on the command line is a tape image, a snapshot or a
		program as text */
		if(IsTextFile(Opts.Tape))
		{
			if(Opts.Text.empty()) Opts.Text = TextFromFile(Opts.Tape);
			if(Opts.Text.empty()) fprintf(stderr, "%s: could not be read as text\n", Opts.Tape.c_str());
		}
		else if(IsSnapshot(Opts.Tape))
		{
			Message = RestoreSnapshot(Machine, Opts.Tape, "state put back");
			MessageCountdown = MESSAGE_FRAMES;
			if(Opts.Headless) printf("%s\n", Message.c_str());
		}
		else if(!Machine.InsertTape(Opts.Tape.c_str()))
			fprintf(stderr, "%s: could not be read as a tape image or a snapshot\n", Opts.Tape.c_str());
		else if(Current.AutoLoad && Opts.Text.empty())
			Opts.Text = Machine.LoadCommand();
	}
	else if(Current.Resume && !Opts.Headless && Opts.Text.empty() && IsSnapshot(ResumeFile))
	{
		/* carry on where the user left off, unless told to do something else */
		Message = RestoreSnapshot(Machine, ResumeFile, "carried on where you left off");
		MessageCountdown = MESSAGE_FRAMES;
	}
	if(!Opts.Text.empty()) Typist.Type(Opts.Text, Description.BootFrames);

	Display Screen;
	AudioOutput Sound;
	FrameLimiter Limiter;

	if(!Opts.Headless)
	{
		if(!Screen.Open(Description.Name, Description.Width, Description.Height, Description.PixelHeight, Description.BorderX, Description.BorderY, Current.Scale, Current.StatusBar))
		{
			fprintf(stderr, "could not open a window: %s\n", SDL_GetError());
			return 1;
		}

		Screen.SetFullScreen(Current.FullScreen);
		Screen.SetEffects(EffectsOf(Current));

		/* no sound is no reason not to run */
		if(!Sound.Open(Description.SampleRate))
			fprintf(stderr, "could not open a sound device: %s\n", SDL_GetError());
		Sound.SetVolume((float)Current.Volume / 100.0f);
	}

	/* the events by which the name of a file the user was asked for arrives: one
	for a file to open, one for a file to keep a snapshot in and one for a file
	to keep a program in */
	const Uint32 FileChosenEvent = SDL_RegisterEvents(3);
	const Uint32 SnapshotFileChosenEvent = FileChosenEvent ? FileChosenEvent + 1 : 0;
	const Uint32 ProgramFileChosenEvent = FileChosenEvent ? FileChosenEvent + 2 : 0;

	const std::string OpenExtensions = std::string(Description.TapeImageExtension) + ";" SNAPSHOT_EXTENSION ";" TEXT_EXTENSIONS;

	bool Quit = false, UserWarp = false, WasWarp = false, Paused = false;

	/* typing in a listing is done at full speed */
	bool Pasting = IsTextFile(Opts.Tape) && !Opts.Text.empty();

	/* the settings screen, and the setting that is picked out on it */
	bool MenuOpen = false;
	int MenuItem = 0;

	/* the monitor, which keeps its breakpoints while it is not on the screen */
	Monitor TheMonitor;
	bool MonitorOpen = false;
	if(!SettingsFolder.empty()) TheMonitor.KeepBreakpointsIn(SettingsFolder + "breakpoints.cfg");
	TheMonitor.FilesIn(Opts.Saves);

	/* A script has a monitor of its own, with breakpoints of its own that are not
	kept. The machine stands still for the script until it says to run */
	Monitor ScriptMonitor;
	bool ScriptWaiting = true;

	/* how long the machine runs before the monitor or the script has it stand
	still again: no limit if zero */
	long FramesToRun = 0, FramesRun = 0;
	bool RunToBreak = false, RunToTyped = false;

	long FrameNumber = 0;
	std::string CurrentTitle = Description.Name;

	/* the files this session has saved recordings in */
	std::set<std::string> Recordings;

	while(!Quit)
	{
		bool Paste = false, Screenshot = false, Eject = false, TogglePause = false, Open = false;
		bool OpenMenu = false, CloseMenu = false, SettingsChanged = false;
		bool OpenMonitor = false, CloseMonitor = false;
		bool KeepState = false, KeepStateAs = false, RestoreState = false, RestoreStateFrom = false;
		bool SaveProgram = false;

		/* a file to keep a snapshot in, and one to keep the program in */
		std::string NewSnapshot, NewProgram;

		/* a tape image to insert and load, from wherever it came */
		std::string NewTape;

		SDL_Event Event;
		while(SDL_PollEvent(&Event))
		{
			switch(Event.type)
			{
				case SDL_EVENT_QUIT:
					Quit = true;
				break;

				case SDL_EVENT_KEY_DOWN:
					/* in the monitor the keyboard is for the monitor */
					if(MonitorOpen)
					{
						if(Event.key.scancode == SDL_SCANCODE_F12)
							Screenshot = true;
						else if(TheMonitor.Key(Machine, Event.key.key) == Monitor::RESUME)
						{
							CloseMonitor = true;
							FramesToRun = TheMonitor.RunFrames();
							RunToBreak = TheMonitor.RunToBreak();
							RunToTyped = TheMonitor.RunToTyped();
							FramesRun = 0;

							const std::string Text = TheMonitor.TakeText();
							if(!Text.empty()) Typist.Type(Text);
						}
						break;
					}

					/* on the settings screen the keyboard is for the settings */
					if(MenuOpen)
					{
						int Direction = 0;

						switch(Event.key.scancode)
						{
							case SDL_SCANCODE_UP: MenuItem = (MenuItem + NUM_MENU_ITEMS - 1) % NUM_MENU_ITEMS; break;
							case SDL_SCANCODE_DOWN: MenuItem = (MenuItem + 1) % NUM_MENU_ITEMS; break;
							case SDL_SCANCODE_LEFT: Direction = -1; break;
							case SDL_SCANCODE_RIGHT:
							case SDL_SCANCODE_RETURN:
							case SDL_SCANCODE_SPACE: Direction = 1; break;
							case SDL_SCANCODE_F12: Screenshot = true; break;
							case SDL_SCANCODE_ESCAPE:
							case SDL_SCANCODE_F2: CloseMenu = true; break;
							default: break;
						}

						if(Direction && ChangeSetting(Current, MenuItem, Direction))
						{
							/* the one setting changes in what is kept as well */
							const struct MenuItem &Item = MenuItems[MenuItem];
							if(Item.Switch)
								Kept.*Item.Switch = Current.*Item.Switch;
							else
								Kept.*Item.Number = Current.*Item.Number;
							SettingsChanged = true;

							/* the machine gets it at once, without being restarted */
							if(Item.Number == &Settings::SidewaysRAM)
							{
								const int Got = Machine.SetSidewaysRAM(Current.SidewaysRAM);
								Message = Got < Current.SidewaysRAM ? "not enough empty slots for that much sideways RAM" : "sideways RAM: " + std::to_string(Got) + " kB";
								MessageCountdown = MESSAGE_FRAMES;
							}
						}
						break;
					}

					if(Event.key.repeat) break;

					/* keys pressed together with Cmd are for the emulator, not
					for the machine */
					if(Event.key.mod & SDL_KMOD_GUI)
					{
						switch(Event.key.scancode)
						{
							case SDL_SCANCODE_COMMA: OpenMenu = true; break;
							case SDL_SCANCODE_S:
								if(Event.key.mod & SDL_KMOD_SHIFT)
									KeepStateAs = true;
								else
									KeepState = true;
							break;
							case SDL_SCANCODE_L:
								if(Event.key.mod & SDL_KMOD_SHIFT)
									RestoreStateFrom = true;
								else
									RestoreState = true;
							break;
							case SDL_SCANCODE_O: Open = true; break;
							case SDL_SCANCODE_B: SaveProgram = true; break;
							case SDL_SCANCODE_V: Paste = true; break;
							case SDL_SCANCODE_P: TogglePause = true; break;
							case SDL_SCANCODE_D: OpenMonitor = true; break;
							case SDL_SCANCODE_G: Screenshot = true; break;
							case SDL_SCANCODE_E: Eject = true; break;
							default: break;
						}
						break;
					}

					switch(Event.key.scancode)
					{
						case SDL_SCANCODE_F1:
							if(Event.key.mod & SDL_KMOD_SHIFT)
								SaveProgram = true;
							else
								Open = true;
						break;
						case SDL_SCANCODE_F2: OpenMenu = true; break;
						case SDL_SCANCODE_F3:
							if(Event.key.mod & SDL_KMOD_SHIFT)
								KeepStateAs = true;
							else
								KeepState = true;
						break;
						case SDL_SCANCODE_F4:
							if(Event.key.mod & SDL_KMOD_SHIFT)
								RestoreStateFrom = true;
							else
								RestoreState = true;
						break;
						case SDL_SCANCODE_F5: Machine.PressReset(); break;
						case SDL_SCANCODE_F6: Machine.PowerOn(); break;
						case SDL_SCANCODE_F7:
							if(Event.key.mod & SDL_KMOD_SHIFT)
								Eject = true;
							else
								Machine.RewindTape();
						break;
						case SDL_SCANCODE_F8: UserWarp = !UserWarp; break;
						case SDL_SCANCODE_F9: Paste = true; break;
						case SDL_SCANCODE_F10:
							if(Event.key.mod & SDL_KMOD_SHIFT)
								OpenMonitor = true;
							else
								TogglePause = true;
						break;
						case SDL_SCANCODE_F11:
							/* for now, not for good: the setting as it is kept stays */
							Current.FullScreen = !Current.FullScreen;
							Screen.SetFullScreen(Current.FullScreen);
						break;
						case SDL_SCANCODE_F12: Screenshot = true; break;
						case SDL_SCANCODE_ESCAPE: Typist.Stop(); break;
						default: break;
					}
					/* fall through */
				case SDL_EVENT_KEY_UP:
					Machine.HostKey(Event.key.scancode, Event.type == SDL_EVENT_KEY_DOWN);
				break;

				case SDL_EVENT_DROP_FILE:
					NewTape = Event.drop.data;
				break;

				default:
					if(FileChosenEvent && Event.type == FileChosenEvent)
					{
						NewTape = (const char *)Event.user.data1;
						SDL_free(Event.user.data1);
					}
					else if(SnapshotFileChosenEvent && Event.type == SnapshotFileChosenEvent)
					{
						NewSnapshot = (const char *)Event.user.data1;
						SDL_free(Event.user.data1);
					}
					else if(ProgramFileChosenEvent && Event.type == ProgramFileChosenEvent)
					{
						NewProgram = (const char *)Event.user.data1;
						SDL_free(Event.user.data1);
					}
				break;
			}
		}

		if(KeepState)
		{
			Message = (!SnapshotFile.empty() && SaveSnapshot(Machine, SnapshotFile)) ? "state kept" : "the state could not be kept";
			MessageCountdown = MESSAGE_FRAMES;
		}

		if(KeepStateAs) Screen.AskForNewFile("Snapshots", SNAPSHOT_EXTENSION, SnapshotFileChosenEvent);

		if(!NewSnapshot.empty())
		{
			/* the name is the user's, the extension is not */
			NewSnapshot = WithExtension(NewSnapshot, SNAPSHOT_EXTENSION);

			Message = SaveSnapshot(Machine, NewSnapshot) ? "state kept in " + NewSnapshot : "the state could not be kept in " + NewSnapshot;
			MessageCountdown = MESSAGE_FRAMES;
		}

		/* the program is asked for now, to know whether there is one, and again
		when the user has chosen a file, which may be a while later */
		std::string Program;
		if(SaveProgram)
		{
			if(Machine.ProgramText(Program))
				Screen.AskForNewFile("BASIC programs as text", PROGRAM_EXTENSION, ProgramFileChosenEvent);
			else
			{
				Message = "there is no BASIC program to save as text";
				MessageCountdown = MESSAGE_FRAMES;
			}
		}

		if(!NewProgram.empty())
		{
			NewProgram = WithExtension(NewProgram, PROGRAM_EXTENSION);

			const bool Saved = Machine.ProgramText(Program) && SaveText(NewProgram, Program);
			Message = Saved ? "program saved as " + NewProgram : "the program could not be saved as " + NewProgram;
			MessageCountdown = MESSAGE_FRAMES;
		}

		/* a snapshot to put back: that of F3, or one that was opened like a tape */
		std::string OldSnapshot;
		if(RestoreState) OldSnapshot = SnapshotFile;
		if(!NewTape.empty() && IsSnapshot(NewTape))
		{
			OldSnapshot = NewTape;
			NewTape.clear();
		}

		if(RestoreState || !OldSnapshot.empty())
		{
			Message = RestoreSnapshot(Machine, OldSnapshot, "state put back");
			MessageCountdown = MESSAGE_FRAMES;

			/* nothing is left of whatever was going on */
			Typist.Stop();
			Pasting = false;
			Sound.Flush();
			Limiter.Restart();
		}

		/* a snapshot chosen this way arrives like any other file that is opened */
		if(RestoreStateFrom) Screen.AskForFile("Snapshots", SNAPSHOT_EXTENSION, FileChosenEvent);

		if(Open) Screen.AskForFile("Tape images, snapshots and programs as text", OpenExtensions.c_str(), FileChosenEvent);

		/* a program as text is typed in, like text from the clipboard */
		std::string NewText;
		if(!NewTape.empty() && IsTextFile(NewTape))
		{
			NewText = TextFromFile(NewTape);
			if(NewText.empty())
			{
				Message = NewTape + " could not be read as text";
				MessageCountdown = MESSAGE_FRAMES;
			}
			NewTape.clear();
		}

		if(!NewTape.empty())
		{
			if(Machine.InsertTape(NewTape.c_str()))
			{
				if(Current.AutoLoad) Typist.Type(Machine.LoadCommand());
			}
			else
			{
				Message = NewTape + " could not be read as a tape image or a snapshot";
				MessageCountdown = MESSAGE_FRAMES;
			}
		}

		if(Eject && Machine.TapeInserted())
		{
			Message = Machine.TapeName() + " taken out";
			MessageCountdown = MESSAGE_FRAMES;
			Machine.EjectTape();
		}

		if(SettingsChanged)
		{
			Screen.SetFullScreen(Current.FullScreen);
			Screen.SetStatusBar(Current.StatusBar);
			Screen.SetEffects(EffectsOf(Current));
			Screen.SetScale(Current.Scale);
			Sound.SetVolume((float)Current.Volume / 100.0f);

			if(SettingsFile.empty() || !Kept.Save(SettingsFile))
			{
				Message = "the settings could not be kept";
				MessageCountdown = MESSAGE_FRAMES;
			}
		}

		if(OpenMenu && !Opts.Headless)
		{
			MenuOpen = true;
			Sound.Flush();
		}
		if(CloseMenu)
		{
			MenuOpen = false;
			Limiter.Restart();
		}

		if(OpenMonitor && !Opts.Headless)
		{
			MonitorOpen = true;
			TheMonitor.Opened(Machine, "");
			FramesToRun = 0;
			Sound.Flush();
		}
		if(CloseMonitor)
		{
			MonitorOpen = false;
			Limiter.Restart();
		}

		if(Screenshot)
		{
			const std::string Name = ScreenshotName(Machine);
			const bool Saved =
				MonitorOpen ? Screen.SavePageBMP(Name.c_str(), Machine.Pixels(), TheMonitor.Lines(Machine), Monitor::COLUMNS) :
				MenuOpen ? Screen.SaveWindowBMP(Name.c_str(), Machine.Pixels(), MenuLines(Current), MENU_HEADER_LINES + MenuItem) :
				Screen.SaveWindowBMP(Name.c_str(), Machine.Pixels());
			Message = Saved ? "picture saved as " + Name : "could not save a picture as " + Name;
			MessageCountdown = MESSAGE_FRAMES;
		}

		if(TogglePause)
		{
			Paused = !Paused;
			if(Paused)
				Sound.Flush();
			else
				Limiter.Restart();
		}

		if(MessageCountdown && !--MessageCountdown) Message.clear();

		if(Paused || MenuOpen || MonitorOpen)
		{
			/* the machine stands still; only the window is kept up to date */
			if(!Opts.Headless)
			{
				Screen.SetStatus(StatusLeft(Machine, Message), StatusRight(Machine, false, Paused), !Message.empty());

				if(MonitorOpen)
					Screen.PresentPage(Machine.Pixels(), TheMonitor.Lines(Machine), Monitor::COLUMNS);
				else if(MenuOpen)
					Screen.Present(Machine.Pixels(), MenuLines(Current), MENU_HEADER_LINES + MenuItem);
				else
					Screen.Present(Machine.Pixels());

				const std::string NewTitle = Title(Machine, false, Paused, Message, Current.StatusBar);
				if(NewTitle != CurrentTitle)
				{
					CurrentTitle = NewTitle;
					Screen.SetTitle(CurrentTitle.c_str());
				}
			}

			SDL_Delay(PAUSE_POLL_TIME);
			continue;
		}

		/* the script has its say whenever the machine stands still for it */
		while(Script && ScriptWaiting && !Quit)
		{
			std::string Line;
			if(!ReadLine(Script, Line))
			{
				if(Script != stdin) fclose(Script);
				Script = NULL;

				/* without a window there is nothing left to do */
				if(Opts.Headless) Quit = true;
				break;
			}

			std::vector<std::string> Output;
			switch(ScriptMonitor.Script(Machine, Line, Output))
			{
				case Monitor::QUIT: Quit = true; break;

				case Monitor::RESUME:
				{
					ScriptWaiting = false;
					FramesToRun = ScriptMonitor.RunFrames();
					RunToBreak = ScriptMonitor.RunToBreak();
					RunToTyped = ScriptMonitor.RunToTyped();
					FramesRun = 0;

					const std::string Text = ScriptMonitor.TakeText();
					if(!Text.empty()) Typist.Type(Text);
				}
				break;

				default: break;
			}
			WriteLines(Output);
		}
		if(Quit) break;

		if(Paste) NewText = ClipboardText();
		if(!NewText.empty())
		{
			Typist.Type(NewText);
			Pasting = true;
		}
		if(!Typist.Busy()) Pasting = false;

		Typist.Frame();
		/* without a window there is no monitor to stop in, unless a script has one */
		const uint8_t *const Breakpoints = Script ? ScriptMonitor.Breakpoints() : (Opts.Headless ? NULL : TheMonitor.Breakpoints());

		/* a reason to stop and hand the machine to the monitor or the script */
		std::string Reason;
		bool Failure = false;
		char Text[80];

		if(Machine.RunFrame(Breakpoints))
		{
			/* a breakpoint: the rest of the frame is run when the machine runs on */
			Reason = "stopped at a breakpoint";

			uint16_t Addr;
			bool Written;
			if(Machine.AccessBreak(Addr, Written))
			{
				snprintf(Text, sizeof(Text), "%04X was %s", Addr, Written ? "written" : "read");
				Reason = Text;
			}
		}
		else
		{
			FrameNumber++;
			TheMonitor.Frame();
			ScriptMonitor.Frame();

			if(FramesToRun) FramesRun++;

			if(RunToTyped && !Typist.Busy())
			{
				snprintf(Text, sizeof(Text), "typed in %ld frames", FramesRun);
				Reason = Text;
			}
			else if(FramesToRun && FramesRun >= FramesToRun)
			{
				Failure = RunToBreak || RunToTyped;
				snprintf(Text, sizeof(Text),
					RunToBreak ? "no breakpoint was reached in %ld frames" : (RunToTyped ? "the text was not typed in %ld frames" : (FramesRun == 1 ? "ran %ld frame" : "ran %ld frames")), FramesRun);
				Reason = Text;
			}
		}

		if(!Reason.empty())
		{
			FramesToRun = 0;
			RunToTyped = false;

			if(Script)
			{
				snprintf(Text, sizeof(Text), "frame %ld: ", FrameNumber);
				std::vector<std::string> Output;
				ScriptMonitor.Halted(Machine, Text + Reason, Failure, Output);
				WriteLines(Output);
				ScriptWaiting = true;
			}
			else
			{
				MonitorOpen = true;
				TheMonitor.Opened(Machine, Reason);
				Sound.Flush();
			}
		}

		std::string RecordingName;
		std::vector<Uint8> Recording;
		if(Machine.TakeRecording(RecordingName, Recording))
		{
			const std::string Path = SaveRecording(Opts.Saves, RecordingName, Recording, Recordings);
			Message = Path.empty() ? "could not keep what was saved to tape" : "saved to tape: " + Path;
			MessageCountdown = MESSAGE_FRAMES;
			if(Opts.Headless) printf("%s\n", Message.c_str());
		}

		/* a loading tape is not much to look at or listen to, and neither is a
		listing being typed in at seven characters a second */
		const bool Warp = UserWarp || Pasting || (Current.FastTape && Machine.TapeRunning());

		if(!Opts.Headless)
		{
			Screen.SetStatus(StatusLeft(Machine, Message), StatusRight(Machine, Warp, false), !Message.empty());

			if(Warp)
			{
				if(!WasWarp) Sound.Flush();
				if(!(FrameNumber % WARP_FRAME_SKIP)) Screen.Present(Machine.Pixels());
			}
			else
			{
				if(WasWarp) Limiter.Restart();
				Sound.Queue(Machine.Samples().data(), (int)Machine.Samples().size());
				Screen.Present(Machine.Pixels());
				Limiter.Wait(Machine.FrameNanoseconds());
			}

			const std::string NewTitle = Title(Machine, Warp, false, Message, Current.StatusBar);
			if(NewTitle != CurrentTitle)
			{
				CurrentTitle = NewTitle;
				Screen.SetTitle(CurrentTitle.c_str());
			}
		}

		Machine.Samples().clear();
		WasWarp = Warp;

		if(Opts.Frames && FrameNumber >= Opts.Frames) Quit = true;
	}

	if(!Opts.Screenshot.empty() && !Display::SaveBMP(Opts.Screenshot.c_str(), Machine.Pixels(), Description.Width, Description.Height, Current.Scale, Current.Scale * Description.PixelHeight))
		fprintf(stderr, "%s: could not be written: %s\n", Opts.Screenshot.c_str(), SDL_GetError());

	/* keep the state to carry on from next time, or see to it that there is no
	old one left to carry on from */
	if(!Opts.Headless && !ResumeFile.empty())
	{
		if(Current.Resume)
			SaveSnapshot(Machine, ResumeFile);
		else
			remove(ResumeFile.c_str());
	}

	if(Script && Script != stdin) fclose(Script);

	Sound.Close();
	Screen.Close();
	SDL_Quit();
	return ScriptMonitor.Failed() ? 1 : 0;
}
