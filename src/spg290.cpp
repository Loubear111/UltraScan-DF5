#include "spg290.h"
#include <iostream>
#include "Bus.h"

spg290::spg290()
{

}

spg290::~spg290()
{
}

uint32_t spg290::read(uint32_t a)
{
	return bus->read(a, false);
}

void spg290::write(uint32_t a, uint8_t d)
{
	bus->write(a, d);
}

void spg290::clock()
{
	if (cycles == 0)
	{
		instr = read(pc);
		std::cout << "instr: " << instr << std::endl;
		pc++;

		// Extract opcode from instruction word
		// opcode is bits 30-26 so we must shift it over after masking
		uint8_t opcode = (instr & OPCODE_MASK) >> 26;
		std::cout << "opcode: " << std::hex << opcode << std::endl;
		
		// check what the opcode is
		if (opcode == 0)
		{
			// check what the secondary function bits are
			if ((instr & func6_MASK) >> 1 == 0x10)
			{
				// We have an ANDX instruction
				ANDX();
			}
			else if((instr & func6_MASK) >> 1 == 0x9)
            {
			    // ORX instruction
			    ORX();
            }
		}

		// Just setting cycle per instruction to 1 for all instructions for now
		// This will need to be changed later to by cycle accurate
		cycles = 1;
	}

	cycles--;
}

uint8_t spg290::ANDX()
{
	// d: destination reg
	// a: source reg
	// b: source reg
	// operation: d = a & b
	uint8_t d, a, b;
	uint8_t d_reg, a_reg, b_reg;
	
	// Extract register locations from instruction word
	d_reg = (instr & 0x3E00000) >> 21;	// bits 25-21 (see s+core7 pg. 12)
	a_reg = (instr & 0x1F0000) >> 16;	// bits 20-16 (see s+core7 pg. 12)
	b_reg = (instr & 0x7C00) >> 10;		// bits 14-10 (see s+core7 pg. 12)

	// get the values stored in registers
	a = read(a_reg);
	b = read(b_reg);

	// perform operation
	d = a & b;

	// write back the result of the operation
	write(d_reg, d);

	return 1;
}

uint8_t spg290::ORX()
{
    // d: destination reg
    // a: source reg
    // b: source reg
    // operation: d = a | b
    uint8_t d, a, b;
    uint8_t d_reg, a_reg, b_reg;

    // Extract register locations from instruction word
    d_reg = (instr & 0x3E00000) >> 21;	// bits 25-21 (see s+core7 pg. 92 and 12)
    a_reg = (instr & 0x1F0000) >> 16;	// bits 20-16 (see s+core7 pg. 92 and 12)
    b_reg = (instr & 0x7C00) >> 10;		// bits 14-10 (see s+core7 pg. 92 and 12)

    // get the values stored in registers
    a = read(a_reg);
    b = read(b_reg);

    // perform operation
    d = a | b;

    // write back the result of the operation
    write(d_reg, d);

    return 1;
}