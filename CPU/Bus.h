#ifndef CPU_BUS_H
#define CPU_BUS_H

#include <cstdint>

/* The address and data bus as the 6502 sees it.

   The 6502 performs exactly one bus access in every clock cycle, so each call to
   Read or Write stands for one cycle. A machine implements this interface, decodes
   the address, and advances the rest of its hardware by one cycle per call; that is
   all it takes to keep every chip in step with the processor. */
class Bus
{
	public:
		virtual ~Bus() {}

		virtual uint8_t Read(uint16_t Addr) = 0;
		virtual void Write(uint16_t Addr, uint8_t Data) = 0;
};

#endif
