#include "BASIC.h"

#include <cstdio>
#include <cstring>

/* A program is a row of lines, from the address BASIC calls PAGE. A line is a
carriage return, its number with the high byte first, its length and its text, in
which keywords are single bytes of 0x80 and up: tokens. A line number of 0x8000
and up ends the program.

The keywords are in a table in the ROM: the name, the token and a byte of flags
for each of them. */

namespace
{

const size_t RAM_SIZE = 0x8000, ROM_SIZE = 0x4000;

/* where BASIC keeps the high byte of PAGE; the low byte is always zero */
const size_t PAGE_HIGH = 0x18;

const uint8_t LINE_START = 0x0d;
const size_t LINE_HEADER_LENGTH = 4;

/* the first keyword in the table with its token, by which the table is found */
const char FirstKeyword[] = "AND\x80";

/* A line number in the text of a line, as after GOTO, is this token and three
bytes that hold the number in such a way that none of them looks like anything
else */
const uint8_t TOKEN_LINE_NUMBER = 0x8d;

/* The five keywords that can be assigned to, such as TIME, have a second token
for when they are: the first one plus this */
const uint8_t ASSIGNED_TOKEN_OFFSET = 0x40;

/* Finds the keyword for a token in the table. */
bool Keyword(const uint8_t *ROM, uint8_t Token, std::string &Name)
{
	const uint8_t *const End = ROM + ROM_SIZE;
	const size_t FirstLength = sizeof(FirstKeyword) - 1;

	const uint8_t *Entry = ROM;
	while(Entry + FirstLength <= End && memcmp(Entry, FirstKeyword, FirstLength)) Entry++;

	while(Entry < End && *Entry >= 'A' && *Entry <= 'Z')
	{
		const uint8_t *Next = Entry;
		while(Next < End && *Next < 0x80) Next++;
		if(Next >= End) break;

		if(*Next == Token)
		{
			Name.assign((const char *)Entry, (size_t)(Next - Entry));
			return true;
		}

		/* past the token and the flags */
		Entry = Next + 2;
	}

	return false;
}

}

bool BASIC::List(const uint8_t *RAM, const uint8_t *ROM, std::string &Text)
{
	Text.clear();

	size_t Addr = (size_t)RAM[PAGE_HIGH] << 8;

	while(true)
	{
		if(Addr + LINE_HEADER_LENGTH > RAM_SIZE || RAM[Addr] != LINE_START) return false;

		const unsigned int Number = ((unsigned int)RAM[Addr + 1] << 8) | RAM[Addr + 2];
		if(Number & 0x8000) return true;

		const size_t Length = RAM[Addr + 3];
		if(Length < LINE_HEADER_LENGTH || Addr + Length > RAM_SIZE) return false;

		char NumberText[16];
		snprintf(NumberText, sizeof(NumberText), "%5u", Number);
		Text += NumberText;

		/* nothing between quotes is a token */
		bool Quoted = false;
		for(size_t c = Addr + LINE_HEADER_LENGTH; c < Addr + Length; c++)
		{
			const uint8_t Byte = RAM[c];
			std::string Name;

			if(Byte == '"') Quoted = !Quoted;

			if(Quoted || Byte < 0x80)
				Text += (char)Byte;
			else if(Byte == TOKEN_LINE_NUMBER && c + 3 < Addr + Length)
			{
				const unsigned int Top = RAM[c + 1] ^ 0x54;
				const unsigned int Target =
					(RAM[c + 2] & 0x3f) | ((Top & 0x30) << 2) |
					((RAM[c + 3] & 0x3fu) << 8) | ((Top & 0x0c) << 12);

				snprintf(NumberText, sizeof(NumberText), "%u", Target);
				Text += NumberText;
				c += 3;
			}
			else if(Keyword(ROM, Byte, Name) || Keyword(ROM, (uint8_t)(Byte - ASSIGNED_TOKEN_OFFSET), Name))
				Text += Name;
			else
				Text += (char)Byte;
		}

		Text += '\n';
		Addr += Length;
	}
}
