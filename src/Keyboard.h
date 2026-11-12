#ifndef ELECTRON_KEYBOARD_H
#define ELECTRON_KEYBOARD_H

#include <cstdint>
#include <SDL3/SDL_scancode.h>

/* The Electron keyboard: a matrix of fourteen columns by four rows. It has no
   chip of its own: with the keyboard paged in instead of a ROM, the low fourteen
   address lines each select a column when they are low and the four rows appear
   on the data bus.

   A key is identified by column * 4 + row. BREAK is not part of the matrix, it is
   wired to the processor's reset line. */
class Keyboard
{
	public:
		enum
		{
			KEY_NONE = -1,
			KEY_RETURN = 1*4 + 2,
			KEY_SHIFT = 13*4 + 3,
			KEY_CTRL = 13*4 + 2,
			NUM_COLUMNS = 14
		};

		Keyboard();
		void Reset();

		void SetKey(int Key, bool Down);

		/* what the processor reads at an address with the keyboard paged in */
		uint8_t Read(uint16_t Addr) const;

		/* the Electron key at the same position as a key of the host's keyboard */
		static int KeyForScancode(SDL_Scancode);

		/* the Electron key, with or without SHIFT or CTRL, that types a character */
		static bool KeyForCharacter(char Character, int &Key, bool &Shift, bool &Control);

	private:
		uint8_t Columns[NUM_COLUMNS];
};

#endif
