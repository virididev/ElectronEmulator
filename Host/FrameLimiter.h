#ifndef HOST_FRAMELIMITER_H
#define HOST_FRAMELIMITER_H

#include <SDL3/SDL.h>

/* Holds the emulation to the speed of the real machine: after every frame, Wait
   sleeps until the moment that frame would have ended on real hardware. */
class FrameLimiter
{
	public:
		FrameLimiter();

		/* forgets all history; to be called after anything that kept the emulation
		from running at its normal speed */
		void Restart();

		void Wait(Uint64 FrameNanoseconds);

	private:
		Uint64 Deadline;
};

#endif
