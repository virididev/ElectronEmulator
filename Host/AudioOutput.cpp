#include "AudioOutput.h"

/* how much sound to keep queued, and beyond what amount the emulation has
evidently run away from the sound card and it is better to start over; seconds */
#define TARGET_LATENCY	0.06f
#define MAXIMUM_LATENCY	0.25f

AudioOutput::AudioOutput() : Stream(NULL), SampleRate(0)
{
}

AudioOutput::~AudioOutput()
{
	Close();
}

bool AudioOutput::Open(int Rate)
{
	Close();
	SampleRate = Rate;

	SDL_AudioSpec Spec;
	Spec.format = SDL_AUDIO_F32;
	Spec.channels = 1;
	Spec.freq = SampleRate;

	Stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &Spec, NULL, NULL);
	if(!Stream) return false;

	SDL_ResumeAudioStreamDevice(Stream);
	return true;
}

void AudioOutput::Close()
{
	if(Stream) SDL_DestroyAudioStream(Stream);
	Stream = NULL;
}

void AudioOutput::Queue(const float *Samples, int Count)
{
	if(!Stream || Count <= 0) return;

	const float Latency = (float)SDL_GetAudioStreamQueued(Stream) / (float)(sizeof(float) * SampleRate);

	if(Latency > MAXIMUM_LATENCY)
		SDL_ClearAudioStream(Stream);
	else
	{
		float Ratio = 1.0f + (Latency - TARGET_LATENCY) * 0.1f;
		if(Ratio < 0.995f) Ratio = 0.995f;
		if(Ratio > 1.005f) Ratio = 1.005f;
		SDL_SetAudioStreamFrequencyRatio(Stream, Ratio);
	}

	SDL_PutAudioStreamData(Stream, Samples, Count * (int)sizeof(float));
}

void AudioOutput::SetVolume(float Volume)
{
	if(Stream) SDL_SetAudioStreamGain(Stream, Volume);
}

void AudioOutput::Flush()
{
	if(Stream) SDL_ClearAudioStream(Stream);
}
