#include "Keyboard.h"

namespace
{

const int NUM_KEYS = Keyboard::NUM_COLUMNS * 4;

/* what sits on each crossing of the matrix, as the host keys in the same spot
where there is one; COPY is on END and the colon on the key right of the
semicolon */
const SDL_Scancode Layout[NUM_KEYS] =
{
	SDL_SCANCODE_RIGHT,  SDL_SCANCODE_END,      SDL_SCANCODE_UNKNOWN,    SDL_SCANCODE_SPACE,
	SDL_SCANCODE_LEFT,   SDL_SCANCODE_DOWN,     SDL_SCANCODE_RETURN,     SDL_SCANCODE_BACKSPACE,
	SDL_SCANCODE_MINUS,  SDL_SCANCODE_UP,       SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_0,      SDL_SCANCODE_P,        SDL_SCANCODE_SEMICOLON,  SDL_SCANCODE_SLASH,
	SDL_SCANCODE_9,      SDL_SCANCODE_O,        SDL_SCANCODE_L,          SDL_SCANCODE_PERIOD,
	SDL_SCANCODE_8,      SDL_SCANCODE_I,        SDL_SCANCODE_K,          SDL_SCANCODE_COMMA,
	SDL_SCANCODE_7,      SDL_SCANCODE_U,        SDL_SCANCODE_J,          SDL_SCANCODE_M,
	SDL_SCANCODE_6,      SDL_SCANCODE_Y,        SDL_SCANCODE_H,          SDL_SCANCODE_N,
	SDL_SCANCODE_5,      SDL_SCANCODE_T,        SDL_SCANCODE_G,          SDL_SCANCODE_B,
	SDL_SCANCODE_4,      SDL_SCANCODE_R,        SDL_SCANCODE_F,          SDL_SCANCODE_V,
	SDL_SCANCODE_3,      SDL_SCANCODE_E,        SDL_SCANCODE_D,          SDL_SCANCODE_C,
	SDL_SCANCODE_2,      SDL_SCANCODE_W,        SDL_SCANCODE_S,          SDL_SCANCODE_X,
	SDL_SCANCODE_1,      SDL_SCANCODE_Q,        SDL_SCANCODE_A,          SDL_SCANCODE_Z,
	SDL_SCANCODE_ESCAPE, SDL_SCANCODE_CAPSLOCK, SDL_SCANCODE_LCTRL,      SDL_SCANCODE_LSHIFT
};

/* more host keys for the same Electron keys; the Electron has one of each of
SHIFT and CTRL, and CAPS LK doubles as FUNC */
const struct { SDL_Scancode Scancode; int Key; } Extras[] =
{
	{ SDL_SCANCODE_RSHIFT,       13*4 + 3 },
	{ SDL_SCANCODE_RCTRL,        13*4 + 2 },
	{ SDL_SCANCODE_LALT,         13*4 + 1 },
	{ SDL_SCANCODE_RALT,         13*4 + 1 },
	{ SDL_SCANCODE_DELETE,       1*4 + 3 },
	{ SDL_SCANCODE_KP_ENTER,     1*4 + 2 },
	{ SDL_SCANCODE_RIGHTBRACKET, 0*4 + 1 }
};

/* the characters the keys produce, without and with shift, and with CTRL for the
five keys that have a third character on them; letters are left out of the second
list because an Electron starts up with caps lock on */
const char Plain[NUM_KEYS] =
{
	0,   0,   0,    ' ',
	0,   0,   '\n', 0,
	'-', 0,   ':',  0,
	'0', 'P', ';',  '/',
	'9', 'O', 'L',  '.',
	'8', 'I', 'K',  ',',
	'7', 'U', 'J',  'M',
	'6', 'Y', 'H',  'N',
	'5', 'T', 'G',  'B',
	'4', 'R', 'F',  'V',
	'3', 'E', 'D',  'C',
	'2', 'W', 'S',  'X',
	'1', 'Q', 'A',  'Z',
	0,   0,   0,    0
};

const char Shifted[NUM_KEYS] =
{
	'|',  '[', 0,   0,
	'^',  '_', 0,   0,
	'=',  0,   '*', 0,
	'@',  0,   '+', '?',
	')',  0,   0,   '>',
	'(',  0,   0,   '<',
	'\'', 0,   0,   0,
	'&',  0,   0,   0,
	'%',  0,   0,   0,
	'$',  0,   0,   0,
	'#',  0,   0,   0,
	'"',  0,   0,   0,
	'!',  0,   0,   0,
	0,    0,   0,   0
};

const struct { char Character; int Key; } Controlled[] =
{
	{ '\\', 0*4 + 0 }, { ']', 0*4 + 1 }, { '~', 1*4 + 0 }, { '}', 1*4 + 1 }, { '{', 2*4 + 1 }
};

}

Keyboard::Keyboard()
{
	Reset();
}

void Keyboard::Reset()
{
	for(int c = 0; c < NUM_COLUMNS; c++) Columns[c] = 0;
}

void Keyboard::SetKey(int Key, bool Down)
{
	if(Key < 0 || Key >= NUM_KEYS) return;

	if(Down)
		Columns[Key >> 2] |= 1 << (Key & 3);
	else
		Columns[Key >> 2] &= ~(1 << (Key & 3));
}

uint8_t Keyboard::Read(uint16_t Addr) const
{
	uint8_t Rows = 0;
	for(int c = 0; c < NUM_COLUMNS; c++)
		if(!(Addr & (1 << c))) Rows |= Columns[c];

	/* the top four bits of the data bus are not driven */
	return 0xf0 | Rows;
}

int Keyboard::KeyForScancode(SDL_Scancode Scancode)
{
	if(Scancode == SDL_SCANCODE_UNKNOWN) return KEY_NONE;

	for(int c = 0; c < NUM_KEYS; c++)
		if(Layout[c] == Scancode) return c;

	for(unsigned int c = 0; c < sizeof(Extras) / sizeof(*Extras); c++)
		if(Extras[c].Scancode == Scancode) return Extras[c].Key;

	return KEY_NONE;
}

bool Keyboard::KeyForCharacter(char Character, int &Key, bool &Shift, bool &Control)
{
	Shift = Control = false;
	if(!Character) return false;
	if(Character >= 'a' && Character <= 'z') Character -= 'a' - 'A';

	for(unsigned int c = 0; c < sizeof(Controlled) / sizeof(*Controlled); c++)
	{
		if(Controlled[c].Character == Character)
		{
			Key = Controlled[c].Key;
			Control = true;
			return true;
		}
	}

	for(int c = 0; c < NUM_KEYS; c++)
	{
		if(Plain[c] == Character || Shifted[c] == Character)
		{
			Key = c;
			Shift = Shifted[c] == Character;
			return true;
		}
	}

	return false;
}
