#ifndef HOST_SHELL_H
#define HOST_SHELL_H

class HostedMachine;

/* Everything an emulator is besides the machine it emulates: the command line,
   the window, the sound, the keyboard, the keys that control the emulator itself,
   and the loop that keeps the machine running at the right speed.

   The keys of the shell:

	F1			open a tape image or a snapshot
	F2			the settings screen
	F3			keep the state of the machine: a snapshot
	Shift+F3	keep the state of the machine in a file of your choice
	F4			put the machine back in the state that F3 kept
	Shift+F4	put it back in the state from a file of your choice
	F5			the machine's reset button or key
	F6			power off and on
	F7			rewind the tape
	Shift+F7	take the tape out
	F8			full speed on and off
	F9			type the text that is on the clipboard
	F10			pause, and carry on again
	Shift+F10	the monitor: see Monitor.h
	F11			full screen on and off
	F12			save a picture of the screen in the Pictures folder

   On a Mac, Cmd+O, Cmd+comma, Cmd+S, Shift+Cmd+S, Cmd+L, Shift+Cmd+L, Cmd+V,
   Cmd+P, Cmd+D, Cmd+G and Cmd+E do the same as F1, F2, F3, Shift+F3, F4, Shift+F4,
   F9, F10, Shift+F10, F12 and Shift+F7. A tape image dropped on the window is inserted and loaded. Escape
   stops text that is being typed.

   The snapshot of F3 and F4 is kept in a file, so it is still there in a later
   session. Snapshots kept in files of your choice
   are opened with Shift+F4, or like tape images: with F1, by dropping them on
   the window or by naming them on the command line. If the user wants it so, the state is also
   kept on quitting and carried on from at the next start.

   Below the picture is a status bar, unless the user has switched it off: the
   tape and how far it has played, the machine's own lights, and whether the
   emulator is paused or running at full speed. What the emulator has to tell the
   user appears there for a few seconds. Without a status bar all of this goes in
   the title bar.

   Settings are kept from one session to the next; options on the command line
   overrule them for the session they are given in.

   What the machine saves to tape is kept as a tape image in a file of its own,
   named after what was saved.

   Returns the exit code of the program. */
int RunEmulator(HostedMachine &Machine, int argc, char *argv[]);

#endif
