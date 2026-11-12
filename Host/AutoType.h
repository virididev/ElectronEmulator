#ifndef HOST_AUTOTYPE_H
#define HOST_AUTOTYPE_H

#include <string>

class HostedMachine;

/* Types text on the keyboard of a machine, one key at a time, the way a person
   would but faster. */
class AutoType
{
	public:
		explicit AutoType(HostedMachine &);

		/* queues text; a newline is the RETURN key. Typing starts after the given
		number of frames. Fast puts the text straight in the machine's keyboard
		buffer when it can, rather than pressing its keys; it holds for all that
		is queued until the typing is done */
		void Type(const std::string &Text, int DelayFrames = 0, bool Fast = false);

		bool Busy() const { return !Text.empty() || Stage != NOTHING; }

		/* lets go of the keyboard and forgets what was still to be typed */
		void Stop();

		/* to be called once for every frame the machine runs */
		void Frame();

	private:
		HostedMachine &Machine;
		std::string Text;
		int Countdown;
		bool Fast;

		/* the character being typed, and how far that has come: its modifiers
		down, then its key, then its key up again, its modifiers still down */
		char Held;
		enum { NOTHING, MODIFIERS_DOWN, KEY_DOWN, KEY_UP } Stage;
};

#endif
