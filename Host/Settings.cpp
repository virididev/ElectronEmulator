#include "Settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

/* the settings by name; each is either a switch or a number */
const struct
{
	const char *Name;
	bool Settings::*Switch;
	int Settings::*Number;
} Entries[] =
{
	{ "FastTape",   &Settings::FastTape,   NULL },
	{ "AutoLoad",   &Settings::AutoLoad,   NULL },
	{ "FullScreen", &Settings::FullScreen, NULL },
	{ "StatusBar",  &Settings::StatusBar,  NULL },
	{ "Scanlines",  &Settings::Scanlines,  NULL },
	{ "VerticalLines", &Settings::VerticalLines, NULL },
	{ "Vignette",   &Settings::Vignette,   NULL },
	{ "Smooth",     &Settings::Smooth,     NULL },
	{ "Resume",     &Settings::Resume,     NULL },
	{ "Volume",     NULL,                  &Settings::Volume },
	{ "SidewaysRAM", NULL,                 &Settings::SidewaysRAM },
	{ "Scale",      NULL,                  &Settings::Scale }
};

const size_t NUM_ENTRIES = sizeof(Entries) / sizeof(*Entries);

}

Settings::Settings() : FastTape(true), AutoLoad(true), FullScreen(false), StatusBar(true), Scanlines(false), VerticalLines(false), Vignette(false), Smooth(false), Resume(false), Volume(100), SidewaysRAM(0), Scale(0)
{
}

void Settings::Load(const std::string &FileName)
{
	FILE *const File = fopen(FileName.c_str(), "r");
	if(!File) return;

	char Line[256];
	while(fgets(Line, sizeof(Line), File))
	{
		char Name[64], Value[64];
		if(sscanf(Line, " %63[A-Za-z] = %63s", Name, Value) != 2) continue;

		for(size_t c = 0; c < NUM_ENTRIES; c++)
		{
			if(strcmp(Name, Entries[c].Name)) continue;

			if(Entries[c].Switch)
				this->*Entries[c].Switch = !strcmp(Value, "true");
			else
				this->*Entries[c].Number = atoi(Value);
		}
	}
	fclose(File);

	if(Volume < 0) Volume = 0;
	if(Volume > 100) Volume = 100;
	if(Scale < 0) Scale = 0;
	SidewaysRAM = SidewaysRAM < 0 ? 0 : SidewaysRAM > 64 ? 64 : SidewaysRAM / 16 * 16;
}

bool Settings::Save(const std::string &FileName) const
{
	FILE *const File = fopen(FileName.c_str(), "w");
	if(!File) return false;

	for(size_t c = 0; c < NUM_ENTRIES; c++)
	{
		if(Entries[c].Switch)
			fprintf(File, "%s = %s\n", Entries[c].Name, this->*Entries[c].Switch ? "true" : "false");
		else
			fprintf(File, "%s = %d\n", Entries[c].Name, this->*Entries[c].Number);
	}

	return !fclose(File);
}
