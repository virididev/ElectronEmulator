#include "AutoType.h"

#include "HostedMachine.h"

/* how long a key is held and how long the pause until the next one is, in frames;
machines of this kind look at their keyboard fifty or a hundred times a second and
want to see a key several times in a row before they believe it */
#define FRAMES_DOWN	4
#define FRAMES_UP	3

/* after RETURN the machine is given time to do as it was told, since it does not
look at the keyboard while it is busy */
#define FRAMES_AFTER_RETURN	30

AutoType::AutoType(HostedMachine &M) : Machine(M), Countdown(0), Fast(false), Held(0), Stage(NOTHING)
{
}

void AutoType::Type(const std::string &NewText, int DelayFrames, bool NewFast)
{
	if(!Busy())
	{
		Countdown = DelayFrames;
		Fast = NewFast;
	}
	Text += NewText;
}

void AutoType::Stop()
{
	if(Stage == KEY_DOWN) Machine.TypeCharacter(Held, false, false);
	if(Stage != NOTHING) Machine.TypeCharacter(Held, false, true);
	Stage = NOTHING;
	Text.clear();
	Countdown = 0;
}

void AutoType::Frame()
{
	if(!Busy() || --Countdown > 0) return;

	switch(Stage)
	{
		case MODIFIERS_DOWN:
			Machine.TypeCharacter(Held, true, false);
			Stage = KEY_DOWN;
			Countdown = FRAMES_DOWN;
		return;

		case KEY_DOWN:
			Machine.TypeCharacter(Held, false, false);
			Stage = KEY_UP;
			Countdown = 1;
		return;

		case KEY_UP:
			Machine.TypeCharacter(Held, false, true);
			Stage = NOTHING;
			Countdown = (Held == '\n') ? FRAMES_AFTER_RETURN : FRAMES_UP;
		return;

		default: break;
	}

	/* as much as there is room for in the buffer, every frame */
	if(Fast)
	{
		const int Taken = Machine.BufferText(Text);
		if(Taken >= 0)
		{
			Text.erase(0, (size_t)Taken);
			Countdown = 1;
			return;
		}
	}

	/* characters the machine has no key for are skipped */
	while(!Text.empty())
	{
		Held = Text[0];
		Text.erase(0, 1);

		if(Machine.TypeCharacter(Held, true, true))
		{
			Stage = MODIFIERS_DOWN;
			Countdown = 1;
			return;
		}
	}
}
