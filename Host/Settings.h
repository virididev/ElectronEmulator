#ifndef HOST_SETTINGS_H
#define HOST_SETTINGS_H

#include <string>

/* What the user can set about an emulator, kept from one session to the next in a
   small text file of 'name = value' lines. */
struct Settings
{
	/* run at full speed while a tape is loading */
	bool FastTape;

	/* load and run a tape as soon as it is inserted */
	bool AutoLoad;

	bool FullScreen;

	/* show a line of information below the picture */
	bool StatusBar;

	/* the picture tube effects: see Display */
	bool Scanlines, VerticalLines, Vignette, Smooth;

	/* keep the state of the machine on quitting and carry on from it at the
	next start */
	bool Resume;

	/* loudness of the sound, 0 to 100 */
	int Volume;

	/* sideways RAM in the machine's empty ROM slots, in kB: 0 to 64, in banks
	of 16 */
	int SidewaysRAM;

	/* size of the window as a multiple of the machine's picture; 0 for the size
	the machine suggests */
	int Scale;

	Settings();

	/* Load leaves alone what the file does not mention, and so everything if
	there is no file */
	void Load(const std::string &FileName);
	bool Save(const std::string &FileName) const;
};

#endif
