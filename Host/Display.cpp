#include "Display.h"

#include <vector>

/* Text is drawn in SDL's built in font of eight by eight pixels, as large as lets
a line of this many characters fit across the window: fewer and larger ones on a
menu than on the status bar. */
#define TEXT_COLUMNS	32
#define STATUS_COLUMNS	44
#define CHARACTER_SIZE	SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE
#define LINE_HEIGHT		(CHARACTER_SIZE + 4)

/* the least number of coordinates across the window for text to be drawn in */
#define MINIMUM_WIDTH	700

/* How dark the lines of the two line effects are and how dark the corners get
with vignetting, where 255 is black */
#define SCANLINE_DARKNESS		96
#define VERTICAL_LINE_DARKNESS	56
#define VIGNETTE_DARKNESS		150

/* the vignette is a small picture that is stretched over the whole of the screen */
#define VIGNETTE_SIZE			128

/* a machine with more pixels across than this gets a vertical line for every two
of them, or the lines would be too fine to see */
#define MOST_VERTICAL_LINES		400

Display::Display() :
	Window(NULL), Renderer(NULL), Texture(NULL),
	ScanlineTexture(NULL), VerticalLineTexture(NULL), VignetteTexture(NULL),
	Width(0), Height(0), PixelHeight(1), BorderX(0), BorderY(0), Scale(1), Unit(1),
	FullScreen(false), StatusBar(false), StatusLeftEndsInName(false)
{
}

int Display::MenuTextScale() const
{
	const int Result = FullWidth() / (TEXT_COLUMNS * CHARACTER_SIZE);
	return Result < 1 ? 1 : Result;
}

int Display::StatusTextScale() const
{
	const int Result = FullWidth() / (STATUS_COLUMNS * CHARACTER_SIZE);
	return Result < 1 ? 1 : Result;
}

int Display::BarHeight() const
{
	return LINE_HEIGHT * StatusTextScale();
}

/* To be called when the size of what is drawn has changed. */
void Display::SizeChanged(bool ResizeWindow)
{
	SDL_SetRenderLogicalPresentation(Renderer, FullWidth(), FullHeight(), SDL_LOGICAL_PRESENTATION_LETTERBOX);
	SDL_SetWindowMinimumSize(Window, FullWidth(), FullHeight());
	if(ResizeWindow && !FullScreen) SDL_SetWindowSize(Window, FullWidth() * Scale / Unit, FullHeight() * Scale / Unit);
}

Display::~Display()
{
	Close();
}

bool Display::Open(const char *Title, int W, int H, int P, int BX, int BY, int S, bool Bar)
{
	Close();

	Width = W;
	Height = H;
	PixelHeight = P;
	BorderX = BX;
	BorderY = BY;
	Scale = S < 1 ? 1 : S;
	StatusBar = Bar;

	Unit = 1;
	while(FullWidth() < MINIMUM_WIDTH) Unit++;

	/* the window uses every pixel the screen has, which on some screens is more
	than the sizes of windows are counted in */
	if(!SDL_CreateWindowAndRenderer(Title, FullWidth()*Scale/Unit, FullHeight()*Scale/Unit, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &Window, &Renderer))
		return false;

	Texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Width, Height);
	if(!Texture)
	{
		Close();
		return false;
	}

	if(!CreateOverlays())
	{
		Close();
		return false;
	}

	/* the picture is drawn in the coordinates of the emulated machine and SDL
	fits that into the window; pixels stay hard edged unless asked otherwise */
	SetEffects(CurrentEffects);
	SizeChanged(false);

	/* the emulation keeps its own time, which is rarely that of the monitor */
	SDL_SetRenderVSync(Renderer, 0);

	return true;
}

/* An overlay is a picture in black of which only the transparency varies; it is
stretched to fit, smoothly. */
SDL_Texture *Display::CreateOverlay(int W, int H, const std::vector<Uint32> &Pixels)
{
	SDL_Texture *const Overlay = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, W, H);
	if(!Overlay) return NULL;

	SDL_UpdateTexture(Overlay, NULL, Pixels.data(), W * (int)sizeof(Uint32));
	SDL_SetTextureBlendMode(Overlay, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(Overlay, SDL_SCALEMODE_LINEAR);
	return Overlay;
}

bool Display::CreateOverlays()
{
	/* scanlines: every line of the picture is light at the top and dark at the
	bottom, as if the beam did not quite fill it */
	std::vector<Uint32> Pixels((size_t)Height * 2);
	for(int y = 0; y < Height * 2; y++) Pixels[(size_t)y] = (y & 1) ? (Uint32)SCANLINE_DARKNESS << 24 : 0;
	ScanlineTexture = CreateOverlay(1, Height * 2, Pixels);

	/* vertical lines: the same from left to right, for the stripes of phosphor */
	const int Columns = (Width > MOST_VERTICAL_LINES) ? Width / 2 : Width;
	Pixels.assign((size_t)Columns * 2, 0);
	for(int x = 0; x < Columns * 2; x++) Pixels[(size_t)x] = (x & 1) ? (Uint32)VERTICAL_LINE_DARKNESS << 24 : 0;
	VerticalLineTexture = CreateOverlay(Columns * 2, 1, Pixels);

	/* vignette: darker with the square of the distance from the middle */
	Pixels.assign((size_t)VIGNETTE_SIZE * VIGNETTE_SIZE, 0);
	for(int y = 0; y < VIGNETTE_SIZE; y++)
	{
		for(int x = 0; x < VIGNETTE_SIZE; x++)
		{
			const float DX = ((float)x + 0.5f) / (float)(VIGNETTE_SIZE / 2) - 1.0f;
			const float DY = ((float)y + 0.5f) / (float)(VIGNETTE_SIZE / 2) - 1.0f;
			const float Distance = (DX*DX + DY*DY) / 2.0f;
			Pixels[(size_t)(y * VIGNETTE_SIZE + x)] = (Uint32)((float)VIGNETTE_DARKNESS * Distance) << 24;
		}
	}
	VignetteTexture = CreateOverlay(VIGNETTE_SIZE, VIGNETTE_SIZE, Pixels);

	return ScanlineTexture && VerticalLineTexture && VignetteTexture;
}

void Display::SetEffects(const Effects &NewEffects)
{
	CurrentEffects = NewEffects;
	if(Texture) SDL_SetTextureScaleMode(Texture, CurrentEffects.Smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
}

void Display::Close()
{
	if(ScanlineTexture) SDL_DestroyTexture(ScanlineTexture);
	if(VerticalLineTexture) SDL_DestroyTexture(VerticalLineTexture);
	if(VignetteTexture) SDL_DestroyTexture(VignetteTexture);
	ScanlineTexture = VerticalLineTexture = VignetteTexture = NULL;

	if(Texture) SDL_DestroyTexture(Texture);
	if(Renderer) SDL_DestroyRenderer(Renderer);
	if(Window) SDL_DestroyWindow(Window);
	Texture = NULL;
	Renderer = NULL;
	Window = NULL;
}

void Display::Draw(const Uint32 *Pixels)
{
	SDL_UpdateTexture(Texture, NULL, Pixels, Width * (int)sizeof(Uint32));

	const SDL_FRect Target = { (float)(BorderX*Unit), (float)(BorderY*Unit), (float)(Width*Unit), (float)(Height*PixelHeight*Unit) };
	SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
	SDL_RenderClear(Renderer);
	SDL_RenderTexture(Renderer, Texture, NULL, &Target);

	/* the lines belong to the picture, the vignette to the whole of the tube,
	border included */
	if(CurrentEffects.Scanlines) SDL_RenderTexture(Renderer, ScanlineTexture, NULL, &Target);
	if(CurrentEffects.VerticalLines) SDL_RenderTexture(Renderer, VerticalLineTexture, NULL, &Target);
	if(CurrentEffects.Vignette)
	{
		const SDL_FRect Tube = { 0.0f, 0.0f, (float)FullWidth(), (float)PictureHeight() };
		SDL_RenderTexture(Renderer, VignetteTexture, NULL, &Tube);
	}

	if(StatusBar) DrawStatusBar();
}

void Display::DrawStatusBar()
{
	const int TextSize = StatusTextScale();
	const float Top = (float)PictureHeight() / (float)TextSize;
	const int Columns = FullWidth() / (TextSize * CHARACTER_SIZE);

	SDL_SetRenderScale(Renderer, (float)TextSize, (float)TextSize);

	const SDL_FRect Bar = { 0.0f, Top, (float)FullWidth() / (float)TextSize, (float)LINE_HEIGHT };
	SDL_SetRenderDrawColor(Renderer, 48, 48, 48, 255);
	SDL_RenderFillRect(Renderer, &Bar);

	/* what is on the right goes before what is on the left, which is shortened
	if the two do not fit next to each other */
	const std::string Right = StatusRight.substr(0, (size_t)Columns - 2);
	const size_t Room = (size_t)Columns - 2 - Right.size() - (Right.empty() ? 0 : 1);

	std::string Left = StatusLeft;
	if(Left.size() > Room && Room > 3)
	{
		const size_t Tail = StatusLeftEndsInName ? (Room - 3) * 2 / 3 : 0, Head = Room - 3 - Tail;
		Left = Left.substr(0, Head) + "..." + Left.substr(Left.size() - Tail);
	}

	SDL_SetRenderDrawColor(Renderer, 224, 224, 224, 255);
	SDL_RenderDebugText(Renderer, (float)CHARACTER_SIZE, Top + 2.0f, Left.c_str());
	SDL_RenderDebugText(Renderer, (float)((Columns - 1 - (int)Right.size()) * CHARACTER_SIZE), Top + 2.0f, Right.c_str());

	SDL_SetRenderScale(Renderer, 1.0f, 1.0f);
}

void Display::SetStatusBar(bool Shown)
{
	if(!Window || Shown == StatusBar) return;
	StatusBar = Shown;
	SizeChanged(true);
}

void Display::SetStatus(const std::string &Left, const std::string &Right, bool LeftEndsInName)
{
	StatusLeft = Left;
	StatusRight = Right;
	StatusLeftEndsInName = LeftEndsInName;
}

void Display::DrawText(const std::vector<std::string> &Lines, int Highlighted)
{
	const int FullWidth = this->FullWidth(), FullHeight = this->FullHeight();

	/* dim the picture */
	SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 208);
	SDL_RenderFillRect(Renderer, NULL);

	const int Scale = MenuTextScale();
	SDL_SetRenderScale(Renderer, (float)Scale, (float)Scale);

	const float Left = (float)(FullWidth/Scale - TEXT_COLUMNS*CHARACTER_SIZE) / 2.0f;
	float Top = (float)(FullHeight/Scale - (int)Lines.size()*LINE_HEIGHT) / 2.0f;

	for(size_t c = 0; c < Lines.size(); c++, Top += LINE_HEIGHT)
	{
		if((int)c == Highlighted)
		{
			const SDL_FRect Bar = { Left, Top - 2.0f, (float)(TEXT_COLUMNS*CHARACTER_SIZE), (float)LINE_HEIGHT };
			SDL_SetRenderDrawColor(Renderer, 255, 255, 255, 255);
			SDL_RenderFillRect(Renderer, &Bar);
			SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
		}
		else
			SDL_SetRenderDrawColor(Renderer, 255, 255, 255, 255);

		SDL_RenderDebugText(Renderer, Left, Top, Lines[c].c_str());
	}

	SDL_SetRenderScale(Renderer, 1.0f, 1.0f);
	SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_NONE);
}

/* A page of text is not drawn in the coordinates everything else is drawn in,
which are those of the machine, but in the pixels of the screen. That makes the
text as large as will fit and as sharp as the screen is. */
void Display::DrawPage(const std::vector<std::string> &Lines, int Columns)
{
	/* where the picture is in the window, in pixels: as large as fits, in the
	middle, with the status bar below it */
	int OutputWidth, OutputHeight;
	SDL_SetRenderLogicalPresentation(Renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
	SDL_GetCurrentRenderOutputSize(Renderer, &OutputWidth, &OutputHeight);

	float Zoom = (float)OutputWidth / (float)FullWidth();
	if((float)OutputHeight / (float)FullHeight() < Zoom) Zoom = (float)OutputHeight / (float)FullHeight();

	const SDL_FRect Area =
	{
		((float)OutputWidth - (float)FullWidth() * Zoom) / 2.0f, ((float)OutputHeight - (float)FullHeight() * Zoom) / 2.0f,
		(float)FullWidth() * Zoom, (float)PictureHeight() * Zoom
	};

	SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 236);
	SDL_RenderFillRect(Renderer, &Area);
	SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_NONE);

	/* the largest whole number of pixels to a pixel of the font */
	const int LineHeight = CHARACTER_SIZE + 2, Rows = (int)Lines.size() + 1;
	int TextSize = (int)Area.w / ((Columns + 2) * CHARACTER_SIZE);
	if((int)Area.h / (Rows * LineHeight) < TextSize) TextSize = (int)Area.h / (Rows * LineHeight);
	if(TextSize < 1) TextSize = 1;

	SDL_SetRenderScale(Renderer, (float)TextSize, (float)TextSize);
	SDL_SetRenderDrawColor(Renderer, 224, 224, 224, 255);

	const float Left = Area.x / (float)TextSize + (float)CHARACTER_SIZE;
	float Top = Area.y / (float)TextSize + (float)(CHARACTER_SIZE / 2);
	for(size_t c = 0; c < Lines.size(); c++, Top += (float)LineHeight)
		SDL_RenderDebugText(Renderer, Left, Top, Lines[c].c_str());

	SDL_SetRenderScale(Renderer, 1.0f, 1.0f);
	SDL_SetRenderLogicalPresentation(Renderer, FullWidth(), FullHeight(), SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

void Display::PresentPage(const Uint32 *Pixels, const std::vector<std::string> &Lines, int Columns)
{
	if(!Renderer) return;

	Draw(Pixels);
	DrawPage(Lines, Columns);
	SDL_RenderPresent(Renderer);
}

bool Display::SavePageBMP(const char *Name, const Uint32 *Pixels, const std::vector<std::string> &Lines, int Columns)
{
	if(!Renderer) return false;

	Draw(Pixels);
	DrawPage(Lines, Columns);
	return SaveDrawn(Name);
}

void Display::Present(const Uint32 *Pixels)
{
	if(!Renderer) return;

	Draw(Pixels);
	SDL_RenderPresent(Renderer);
}

void Display::Present(const Uint32 *Pixels, const std::vector<std::string> &Lines, int Highlighted)
{
	if(!Renderer) return;

	Draw(Pixels);
	DrawText(Lines, Highlighted);
	SDL_RenderPresent(Renderer);
}

namespace
{

/* called by SDL once the user has chosen a file, or has not */
void SDLCALL FileChosen(void *UserData, const char *const *Files, int)
{
	if(!Files || !Files[0]) return;

	SDL_Event Event;
	SDL_zero(Event);
	Event.type = (Uint32)(uintptr_t)UserData;
	Event.user.data1 = SDL_strdup(Files[0]);
	if(!SDL_PushEvent(&Event)) SDL_free(Event.user.data1);
}

}

void Display::AskForFile(const char *Description, const char *Extensions, Uint32 EventType)
{
	if(!Window) return;

	/* SDL wants the filter to still be there when the user answers */
	static SDL_DialogFileFilter Filter;
	Filter.name = Description;
	Filter.pattern = Extensions;

	SDL_ShowOpenFileDialog(FileChosen, (void *)(uintptr_t)EventType, Window, &Filter, 1, NULL, false);
}

void Display::AskForNewFile(const char *Description, const char *Extensions, Uint32 EventType)
{
	if(!Window) return;

	static SDL_DialogFileFilter Filter;
	Filter.name = Description;
	Filter.pattern = Extensions;

	SDL_ShowSaveFileDialog(FileChosen, (void *)(uintptr_t)EventType, Window, &Filter, 1, NULL);
}

bool Display::SaveWindowBMP(const char *Name, const Uint32 *Pixels)
{
	if(!Renderer) return false;

	Draw(Pixels);
	return SaveDrawn(Name);
}

bool Display::SaveWindowBMP(const char *Name, const Uint32 *Pixels, const std::vector<std::string> &Lines, int Highlighted)
{
	if(!Renderer) return false;

	Draw(Pixels);
	DrawText(Lines, Highlighted);
	return SaveDrawn(Name);
}

bool Display::SaveDrawn(const char *Name)
{
	/* what is read back is what has been drawn but not yet shown */
	SDL_Surface *Surface = SDL_RenderReadPixels(Renderer, NULL);
	if(!Surface) return false;

	const bool Saved = SDL_SaveBMP(Surface, Name);
	SDL_DestroySurface(Surface);
	return Saved;
}

void Display::SetTitle(const char *Title)
{
	if(Window) SDL_SetWindowTitle(Window, Title);
}

void Display::SetFullScreen(bool Full)
{
	if(!Window || Full == FullScreen) return;
	FullScreen = Full;
	SDL_SetWindowFullscreen(Window, FullScreen);
}

void Display::SetScale(int S)
{
	if(!Window || S < 1) return;
	Scale = S;
	if(!FullScreen) SDL_SetWindowSize(Window, FullWidth() * Scale / Unit, FullHeight() * Scale / Unit);
}

bool Display::SaveBMP(const char *Name, const Uint32 *Source, int SourceWidth, int SourceHeight, int ScaleX, int ScaleY)
{
	if(ScaleX < 1) ScaleX = 1;
	if(ScaleY < 1) ScaleY = 1;
	const int W = SourceWidth * ScaleX, H = SourceHeight * ScaleY;

	std::vector<Uint32> Pixels((size_t)W * (size_t)H);
	for(int y = 0; y < H; y++)
		for(int x = 0; x < W; x++)
			Pixels[(size_t)y * (size_t)W + (size_t)x] = Source[(y / ScaleY) * SourceWidth + (x / ScaleX)];

	SDL_Surface *Surface = SDL_CreateSurfaceFrom(W, H, SDL_PIXELFORMAT_ARGB8888, Pixels.data(), W * (int)sizeof(Uint32));
	if(!Surface) return false;

	const bool Saved = SDL_SaveBMP(Surface, Name);
	SDL_DestroySurface(Surface);
	return Saved;
}
