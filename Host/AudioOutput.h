#ifndef HOST_AUDIOOUTPUT_H
#define HOST_AUDIOOUTPUT_H

#include <SDL3/SDL.h>

/* Plays the sound of an emulated machine.

   A machine produces mono samples at whatever rate suits its sound hardware and
   queues them here once a frame; SDL converts them to what the sound card wants.
   The emulation and the sound card each run on a clock of their own, so the amount
   of sound waiting to be played is kept steady by playing it a fraction of a
   percent faster or slower. */
class AudioOutput
{
	public:
		AudioOutput();
		~AudioOutput();

		bool Open(int SampleRate);
		void Close();

		/* samples are in the range -1 to 1 */
		void Queue(const float *Samples, int Count);

		/* 0 is silent, 1 is the sound as the machine makes it */
		void SetVolume(float Volume);

		/* throws away whatever has not been played yet */
		void Flush();

	private:
		SDL_AudioStream *Stream;
		int SampleRate;
};

#endif
