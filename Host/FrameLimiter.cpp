#include "FrameLimiter.h"

FrameLimiter::FrameLimiter()
{
	Restart();
}

void FrameLimiter::Restart()
{
	Deadline = SDL_GetTicksNS();
}

void FrameLimiter::Wait(Uint64 FrameNanoseconds)
{
	Deadline += FrameNanoseconds;

	const Uint64 Now = SDL_GetTicksNS();
	if(Now < Deadline)
		SDL_DelayPrecise(Deadline - Now);
	else if(Now - Deadline > 4*FrameNanoseconds)
	{
		/* too far behind to ever catch up, the host was busy elsewhere */
		Deadline = Now;
	}
}
