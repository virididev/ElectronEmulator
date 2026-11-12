#ifndef ELECTRON_SOUND_H
#define ELECTRON_SOUND_H

#include <cstdint>
#include <string>
#include <vector>

#include "Snapshot/StateStream.h"

/* The sound half of the Electron's ULA: a single square wave of fixed volume, made
   by a counter that divides down the clock. */
class Sound
{
	public:
		/* a sample is the average of the speaker's level over this many ticks of
		the machine's 2 MHz clock */
		enum { TICKS_PER_SAMPLE = 32, SAMPLE_RATE = 2000000 / TICKS_PER_SAMPLE };

		Sound();
		void Reset();

		/* the value of the ULA's counter register; the frequency that results is
		1 MHz / (32 * (Divider + 1)) */
		void SetDivider(uint8_t Divider);

		/* the counter drives the speaker only in one of the ULA's four modes */
		void SetEnabled(bool Enabled);

		/* advances by one tick */
		void Tick();

		/* adds lines of text that say what state the chip is in, for a monitor */
		void Describe(std::vector<std::string> &Lines) const;

		/* saves the state, not including the sound waiting to be played, to a stream, or loads it from one */
		void TransferState(StateStream &);

		/* sound produced so far, in the range -1 to 1; for the caller to play
		and clear */
		std::vector<float> Samples;

	private:
		bool Enabled, Level;
		int HalfPeriod, Countdown;
		int SampleTicks, SampleSum;
		float DCLevel;
};

#endif
