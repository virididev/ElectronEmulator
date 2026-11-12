#ifndef ELECTRON_BASIC_H
#define ELECTRON_BASIC_H

#include <cstdint>
#include <string>

/* Not hardware: what the emulator knows about the BASIC that is in the Electron's
   ROM, which is how it keeps a program in memory. */
namespace BASIC
{

/* The program in memory as the text LIST would show, a line of text to a line of
program. RAM is the 32 kB of it and ROM the BASIC ROM, which has the keywords in
it. False if what is in memory is not a program */
bool List(const uint8_t *RAM, const uint8_t *ROM, std::string &Text);

}

#endif
