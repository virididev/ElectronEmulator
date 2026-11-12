#include "ULA.h"

#include <cstdio>

#include "Sound.h"
#include "Tape.h"
#include "TapeRecorder.h"
#include "Video.h"

namespace
{

enum
{
	REG_INTERRUPTS = 0x0,		/* read: status, write: enable */
	REG_SCREEN_LOW = 0x2,
	REG_SCREEN_HIGH = 0x3,
	REG_CASSETTE = 0x4,
	REG_PAGING = 0x5,			/* and the clearing of interrupts */
	REG_COUNTER = 0x6,
	REG_CONTROL = 0x7,
	REG_PALETTE = 0x8			/* up to 0xf */
};

/* bits of the paging register */
enum { CLEAR_DISPLAY_END = 0x10, CLEAR_RTC = 0x20, CLEAR_HIGH_TONE = 0x40, PAGE_MASK = 0x0f };

/* bits of the control register */
enum { CONTROL_MODE_SHIFT = 1, CONTROL_DISPLAY_SHIFT = 3, CONTROL_MOTOR = 0x40, CONTROL_CAPS_LOCK = 0x80 };

/* the interrupts that can be enabled */
const uint8_t ENABLE_MASK = 0x7c;

/* The two interrupts that tick fifty times a second each: one a hundred lines into
the picture, and one as the last line of the picture has been drawn. In most modes
that last line is where the frame is taken to end here; in the two modes for text
the picture is six lines shorter and the interrupt that much earlier */
const int RTC_POSITION = (Video::FIRST_LINE + 99) * ULA::TICKS_PER_LINE;
const int TEXT_END_POSITION = (Video::FIRST_LINE + Video::TEXT_HEIGHT) * ULA::TICKS_PER_LINE;

/* the cassette interface works at 1200 bits per second */
const int TICKS_PER_BIT = ULA::CLOCK_RATE / 1200;

/* A short pulse off the tape does not last a whole number of ticks, so the time
to the next one is counted down in parts of a tick: a short pulse is as many of
those as there are ticks to a second, a tick as many as short pulses to a second */
const int PULSE_TIME = ULA::CLOCK_RATE, TICK_TIME = Tape::PULSE_RATE;

/* a byte on tape: a start bit, eight data bits and a stop bit */
const int BITS_PER_BYTE = 10;

/* The receiver keeps the last eleven bits, the newest at the top. A byte is in
when those are the stop bit of the byte before, a start bit, eight data bits and a
stop bit; eleven ones are carrier tone */
const unsigned int SHIFTER_TOP = 0x400;
const unsigned int SHIFTER_ALL = 0x7ff;
const unsigned int SHIFTER_FRAMING = 0x403, SHIFTER_BYTE_IN = 0x401;

}

ULA::ULA(Video &V, Sound &S, Tape &T, TapeRecorder &R) : TheVideo(V), TheSound(S), TheTape(T), TheRecorder(R)
{
	PowerOn();
}

void ULA::PowerOn()
{
	Position = 0;
	Ticks = 0;
	Status = IRQ_POWER_ON;
	Enabled = 0;
	Paged = 0;
	Mode = MODE_INPUT;
	CapsLock = false;

	Shifter = 0;
	Received = 0;
	SkipBits = 0;
	PulseKind = Tape::PULSE_SILENCE;
	PulseCount = 0;
	PulseCountdown = 0;
	SendCountdown = 0;

	TheVideo.Reset();
	TheSound.Reset();
	TheTape.SetMotor(false);
	TheRecorder.Reset();
}

void ULA::Describe(std::vector<std::string> &Lines) const
{
	static const char *const ModeNames[] = { "cassette input", "sound", "cassette output", "off" };

	char Text[128];
	snprintf(Text, sizeof(Text), "ULA   interrupts %02X enabled %02X IRQ %s  ROM %d", Status, Enabled, IRQ() ? "yes" : "no", Paged);
	Lines.push_back(Text);

	snprintf(Text, sizeof(Text), "      %s, motor %s, caps lock %s", ModeNames[Mode], TheTape.MotorOn() ? "on" : "off", CapsLock ? "on" : "off");
	Lines.push_back(Text);

	snprintf(Text, sizeof(Text), "      line %d of %d, tick %d  shifter %03X received %02X",
		Position / TICKS_PER_LINE, (int)LINES_PER_FRAME, Position % TICKS_PER_LINE, Shifter, Received);
	Lines.push_back(Text);
}

void ULA::TransferState(StateStream &Stream)
{
	Stream.Value(Position);
	Stream.Value(Ticks);
	Stream.Value(Status);
	Stream.Value(Enabled);
	Stream.Value(Paged);
	Stream.Value(Mode);
	Stream.Value(CapsLock);

	Stream.Value(Shifter);
	Stream.Value(Received);
	Stream.Value(SkipBits);
	Stream.Value(PulseKind);
	Stream.Value(PulseCount);
	Stream.Value(PulseCountdown);
	Stream.Value(SendCountdown);
}

bool ULA::RAMBusy() const
{
	/* in the four modes with 80 bytes to the line, the ULA needs every memory
	cycle there is while it draws a line of the picture; the blank lines of
	mode 3 are not drawn from memory, which makes it the fastest of the four */
	return TheVideo.Mode() <= 3 && TheVideo.Fetching(Position);
}

uint8_t ULA::Read(uint8_t Register)
{
	switch(Register & 0x0f)
	{
		case REG_INTERRUPTS:
		{
			uint8_t Result = Status | 0x80;
			if(IRQ()) Result |= IRQ_MASTER;

			/* having been read once, the power on bit is gone; it is how the
			operating system tells power on from BREAK */
			Status &= ~IRQ_POWER_ON;
			return Result;
		}

		case REG_CASSETTE:
			Status &= ~IRQ_RECEIVE;
			return Received;

		/* the other registers can only be written */
		default: return 0xff;
	}
}

void ULA::Write(uint8_t Register, uint8_t Data)
{
	Register &= 0x0f;

	switch(Register)
	{
		case REG_INTERRUPTS:
			Enabled = Data & ENABLE_MASK;
		break;

		case REG_SCREEN_LOW:
			TheVideo.RunTo(Position);
			TheVideo.SetStartLow(Data);
		break;

		case REG_SCREEN_HIGH:
			TheVideo.RunTo(Position);
			TheVideo.SetStartHigh(Data);
		break;

		case REG_CASSETTE:
			/* a byte to send, which takes its time getting out */
			if(Mode == MODE_OUTPUT)
			{
				if(TheTape.MotorOn()) TheRecorder.Byte(Data, Ticks);
				Status &= ~IRQ_SEND;
				SendCountdown = BITS_PER_BYTE * TICKS_PER_BIT;
			}
		break;

		case REG_PAGING:
			if(Data & CLEAR_DISPLAY_END) Status &= ~IRQ_DISPLAY_END;
			if(Data & CLEAR_RTC) Status &= ~IRQ_RTC;
			if(Data & CLEAR_HIGH_TONE) Status &= ~IRQ_HIGH_TONE;

			/* ROMs 8 to 15 can be paged in at any time, but while one of 8 to
			11 - the keyboard and BASIC - is paged in, a write of 0 to 7 is
			ignored; to get to those, one of 12 to 15 has to be paged in first */
			if((Paged & 0x0c) != 0x08 || (Data & 0x08)) Paged = Data & PAGE_MASK;
		break;

		case REG_COUNTER:
			TheSound.SetDivider(Data);
		break;

		case REG_CONTROL:
		{
			const uint8_t NewMode = (Data >> CONTROL_MODE_SHIFT) & 3;
			const bool Motor = (Data & CONTROL_MOTOR) != 0;

			/* a recording lasts for as long as the ULA sends and the motor runs */
			if(Mode == MODE_OUTPUT && (NewMode != MODE_OUTPUT || !Motor)) TheRecorder.Stopped();

			if(NewMode != Mode)
			{
				Mode = NewMode;
				TheSound.SetEnabled(Mode == MODE_SOUND);

				/* an idle transmitter is ready for a byte; the receiver starts
				from nothing */
				Status &= ~(IRQ_RECEIVE | IRQ_SEND);
				if(Mode == MODE_OUTPUT) Status |= IRQ_SEND;
				Shifter = 0;
				SkipBits = 0;
				PulseCount = 0;
				SendCountdown = 0;
			}

			TheVideo.RunTo(Position);
			TheVideo.SetMode((Data >> CONTROL_DISPLAY_SHIFT) & 7);

			TheTape.SetMotor(Motor);
			CapsLock = (Data & CONTROL_CAPS_LOCK) != 0;
		}
		break;

		default:
			if(Register >= REG_PALETTE)
			{
				TheVideo.RunTo(Position);
				TheVideo.SetPalette(Register - REG_PALETTE, Data);
			}
		break;
	}
}

/* Makes bits of the pulses that come off the tape: four short ones in a row are
a 1 and two long ones a 0. Pulses that do not add up to a bit before the other kind
comes along are lost, which is what puts the receiver in step with the tape again
after anything out of the ordinary. */
void ULA::ReceivePulse(int Pulse)
{
	if(Pulse != PulseKind)
	{
		PulseKind = Pulse;
		PulseCount = 0;
	}

	if(Pulse != Tape::PULSE_SHORT && Pulse != Tape::PULSE_LONG)
	{
		Shifter = 0;
		SkipBits = 0;
		return;
	}

	const bool Short = Pulse == Tape::PULSE_SHORT;
	if(++PulseCount < (Short ? (int)Tape::SHORT_PULSES_PER_BIT : (int)Tape::LONG_PULSES_PER_BIT)) return;

	PulseCount = 0;
	ReceiveBit(Short ? 1 : 0);
}

void ULA::ReceiveBit(int Bit)
{
	Shifter = (Shifter >> 1) | (Bit ? SHIFTER_TOP : 0);

	/* having found a byte, the next one cannot be in before all of its bits are */
	if(SkipBits)
	{
		SkipBits--;
		return;
	}

	if((Shifter & SHIFTER_FRAMING) == SHIFTER_BYTE_IN)
	{
		Received = (uint8_t)(Shifter >> 2);
		Status = (Status & ~IRQ_HIGH_TONE) | IRQ_RECEIVE;
		SkipBits = BITS_PER_BYTE - 1;
	}
	else if(Shifter == SHIFTER_ALL)
		Status |= IRQ_HIGH_TONE;
}

bool ULA::Tick()
{
	if(Mode == MODE_INPUT)
	{
		if(TheTape.Running() && (PulseCountdown -= TICK_TIME) <= 0)
		{
			/* a pulse is taken in as it starts; the next one is due when
			it is over */
			const int Pulse = TheTape.NextPulse();
			PulseCountdown += Tape::Length(Pulse) * PULSE_TIME;
			ReceivePulse(Pulse);
		}
	}
	else if(SendCountdown && !--SendCountdown)
		Status |= IRQ_SEND;

	TheSound.Tick();

	Ticks++;
	Position++;
	if(Position == RTC_POSITION) Status |= IRQ_RTC;
	if(Position == TEXT_END_POSITION && TheVideo.TextMode()) Status |= IRQ_DISPLAY_END;
	if(Position < TICKS_PER_FRAME) return false;

	if(!TheVideo.TextMode()) Status |= IRQ_DISPLAY_END;
	TheVideo.RunTo(TICKS_PER_FRAME);
	TheVideo.EndFrame();
	Position = 0;
	return true;
}
