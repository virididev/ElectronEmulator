#include "Sound.h"

#include <cstdio>

namespace
{

const float Volume = 0.25f;

/* the cutoff of the filter that removes the constant part of the output: about
10 Hz */
const float DCRate = 2.0f * 3.14159265f * 10.0f / (float)Sound::SAMPLE_RATE;

}

Sound::Sound()
{
	Reset();
}

void Sound::Reset()
{
	Enabled = Level = false;
	HalfPeriod = Countdown = 32;
	SampleTicks = SampleSum = 0;
	DCLevel = 0.0f;
	Samples.clear();
}

void Sound::Describe(std::vector<std::string> &Lines) const
{
	char Text[128];
	snprintf(Text, sizeof(Text), "SOUND %s  counter %02X  %d Hz", Enabled ? "on" : "off", HalfPeriod / 32 - 1, 1000000 / HalfPeriod);
	Lines.push_back(Text);
}

void Sound::TransferState(StateStream &Stream)
{
	Stream.Value(Enabled);
	Stream.Value(Level);
	Stream.Value(HalfPeriod);
	Stream.Value(Countdown);
	Stream.Value(SampleTicks);
	Stream.Value(SampleSum);
	Stream.Value(DCLevel);
}

void Sound::SetDivider(uint8_t Divider)
{
	/* half a period of the square wave, in ticks of half a microsecond */
	HalfPeriod = 32 * (Divider + 1);
}

void Sound::SetEnabled(bool E)
{
	Enabled = E;
	if(!Enabled) Level = false;
}

void Sound::Tick()
{
	if(Enabled && --Countdown <= 0)
	{
		Countdown = HalfPeriod;
		Level = !Level;
	}

	/* averaging keeps frequencies beyond what the sample rate can carry from
	turning into audible ones; programs set the highest possible frequency as
	a way of being silent */
	if(Level) SampleSum++;
	if(++SampleTicks < TICKS_PER_SAMPLE) return;

	const float Output = Volume * (float)SampleSum / (float)TICKS_PER_SAMPLE;
	SampleTicks = SampleSum = 0;

	DCLevel += (Output - DCLevel) * DCRate;
	Samples.push_back(Output - DCLevel);
}
