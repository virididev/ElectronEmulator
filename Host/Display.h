#ifndef HOST_DISPLAY_H
#define HOST_DISPLAY_H

#include <SDL3/SDL.h>

#include <string>
#include <vector>

/* A window that shows the picture of an emulated machine.

   The machine hands over complete frames as rows of 0xAARRGGBB pixels; they are
   scaled to whatever size the window has, in whole pixels where that is possible,
   with a border around them. Below the picture there can be a status bar: one
   line of text, some of it on the left and some on the right.

   The picture can be made to look more like that of the picture tube these
   machines were connected to, by any of four effects that are laid over it. */
class Display
{
	public:
		Display();
		~Display();

		/* Width and Height are those of the frames that will be shown. PixelHeight
		is how many times taller than wide a pixel of the machine is, BorderX and
		BorderY the margins kept left and right of the picture and above and below
		it, measured in pixel widths, and Scale the initial size of the window as
		a multiple of the picture's size */
		bool Open(const char *Title, int Width, int Height, int PixelHeight, int BorderX, int BorderY, int Scale, bool StatusBar);
		void Close();

		void Present(const Uint32 *Pixels);

		/* shows a frame, dimmed, with lines of text over it of which one is
		picked out; for menus. A line can be 32 characters long */
		void Present(const Uint32 *Pixels, const std::vector<std::string> &Lines, int Highlighted);

		/* Shows a frame, all but blacked out, with a page of text over it; for the
		monitor. The text is as large as lets all the lines fit, and lines of the
		given number of characters, in whole pixels of the screen rather than of
		the machine, so that it is as sharp as the screen allows */
		void PresentPage(const Uint32 *Pixels, const std::vector<std::string> &Lines, int Columns);

		void SetTitle(const char *Title);

		/* The picture tube effects: dark lines between the lines of the picture,
		dark lines between its columns, the picture growing darker towards its
		corners, and pixels that run into each other instead of having hard
		edges */
		struct Effects
		{
			bool Scanlines, VerticalLines, Vignette, Smooth;
			Effects() : Scanlines(false), VerticalLines(false), Vignette(false), Smooth(false) {}
		};
		void SetEffects(const Effects &);

		/* Shows or hides the status bar, the window growing or shrinking by its
		height, and sets what it says. If the text on the left does not fit it is
		shortened: at its end, or in its middle if it ends in the name of a file,
		which is then the part worth keeping */
		void SetStatusBar(bool Shown);
		void SetStatus(const std::string &Left, const std::string &Right, bool LeftEndsInName);

		void SetFullScreen(bool FullScreen);
		bool IsFullScreen() const { return FullScreen; }

		/* resizes the window to a multiple of the picture's size */
		void SetScale(int Scale);

		/* Asks the user for a file to open, or for a new file to save in. The files
		on offer are those with one of the given extensions, which are separated
		by semicolons. The answer comes later, as an event of the given type with
		the name of the file as its first data pointer, to be released with
		SDL_free; no event if the user declines */
		void AskForFile(const char *Description, const char *Extensions, Uint32 EventType);
		void AskForNewFile(const char *Description, const char *Extensions, Uint32 EventType);

		/* writes a frame to a BMP file exactly as the window shows it, border
		included and at the size the window has */
		bool SaveWindowBMP(const char *Name, const Uint32 *Pixels);
		bool SaveWindowBMP(const char *Name, const Uint32 *Pixels, const std::vector<std::string> &Lines, int Highlighted);
		bool SavePageBMP(const char *Name, const Uint32 *Pixels, const std::vector<std::string> &Lines, int Columns);

		/* writes a frame to a BMP file without the help of a window: every pixel
		ScaleX times across and ScaleY times down */
		static bool SaveBMP(const char *Name, const Uint32 *Pixels, int Width, int Height, int ScaleX, int ScaleY);

	private:
		SDL_Window *Window;
		SDL_Renderer *Renderer;
		SDL_Texture *Texture;

		/* what is laid over the picture for the effects that are on */
		SDL_Texture *ScanlineTexture, *VerticalLineTexture, *VignetteTexture;
		Effects CurrentEffects;
		SDL_Texture *CreateOverlay(int Width, int Height, const std::vector<Uint32> &Pixels);
		bool CreateOverlays();
		int Width, Height, PixelHeight, BorderX, BorderY, Scale;

		/* Things are drawn in coordinates that are a whole number of times finer
		than the pixels of the machine, so that text can be smaller than those
		pixels on a machine that has few of them */
		int Unit;
		bool FullScreen, StatusBar;
		std::string StatusLeft, StatusRight;
		bool StatusLeftEndsInName;

		/* sizes in the coordinates things are drawn in */
		int FullWidth() const { return (Width + 2*BorderX) * Unit; }
		int PictureHeight() const { return (Height*PixelHeight + 2*BorderY) * Unit; }
		int FullHeight() const { return PictureHeight() + (StatusBar ? BarHeight() : 0); }
		int MenuTextScale() const;
		int StatusTextScale() const;
		int BarHeight() const;
		void SizeChanged(bool ResizeWindow);

		void Draw(const Uint32 *Pixels);
		void DrawStatusBar();
		void DrawText(const std::vector<std::string> &Lines, int Highlighted);
		void DrawPage(const std::vector<std::string> &Lines, int Columns);
		bool SaveDrawn(const char *Name);
};

#endif
