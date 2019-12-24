#include "spg290.h"
#include "Bus.h"

spg290::spg290()
{
	using cpu = spg290;
	lookup =
	{
		{ "andx", &cpu::ANDX, 32, 1 }, { "???", &cpu::UNDEF, 32, 1 },
	};
}

spg290::~spg290()
{
}

uint8_t spg290::read(uint16_t a)
{
	return bus->read(a, false);
}

void spg290::write(uint16_t a, uint8_t d)
{
	bus->write(a, d);
}

void spg290::clock()
{
	if (cycles == 0)
	{
		opcode = read(pc);
		pc++;

		// set required cycles to run instruction
		cycles = lookup[opcode].cycles;

		// Execute the function for given instruction
		(this->*lookup[opcode].operate)();
	}

	cycles--;
}

uint8_t spg290::ANDX()
{

}