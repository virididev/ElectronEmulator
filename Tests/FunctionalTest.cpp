/* Runs Klaus Dormann's 6502 functional test against the 6502 core.

   The test binary is not part of this repository; it is 6502_functional_test.bin
   from https://github.com/Klaus2m5/6502_65C02_functional_tests, a 64 kB memory
   image that starts at 0x0400 and ends in an endless loop at 0x3469 on success.
   Anywhere else it loops, a test failed. */

#include <cstdio>
#include <cstring>

#include "CPU/MOS6502.h"

class TestBus : public Bus
{
	public:
		uint8_t Memory[65536];

		uint8_t Read(uint16_t Addr) { return Memory[Addr]; }
		void Write(uint16_t Addr, uint8_t Data) { Memory[Addr] = Data; }
};

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		fprintf(stderr, "usage: %s 6502_functional_test.bin\n", argv[0]);
		return 2;
	}

	static TestBus TheBus;
	memset(TheBus.Memory, 0, sizeof(TheBus.Memory));

	FILE *File = fopen(argv[1], "rb");
	if(!File || fread(TheBus.Memory, 1, 65536, File) != 65536)
	{
		fprintf(stderr, "%s: not a 64 kB memory image\n", argv[1]);
		return 2;
	}
	fclose(File);

	MOS6502 CPU(TheBus);
	CPU.PC = 0x0400;

	while(true)
	{
		const uint16_t Before = CPU.PC;
		CPU.Step();
		if(CPU.PC == Before) break;
	}

	const bool Passed = CPU.PC == 0x3469;
	printf("%s: stopped at %04x after %llu cycles\n", Passed ? "passed" : "FAILED", CPU.PC, (unsigned long long)CPU.Cycles);
	return Passed ? 0 : 1;
}
