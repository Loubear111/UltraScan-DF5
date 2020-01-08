#include "spg290.h"
#include <iostream>
#include "Bus.h"

spg290::spg290()
{

}

spg290::~spg290()
{
}

uint32_t spg290::read(uint16_t a)
{
	return bus->read(a, false);
}

void spg290::write(uint16_t a, uint32_t d)
{
	bus->write(a, d);
}

// Returns the value of a specific bit of the status register
uint8_t spg290::GetFlag(FLAGS290 f)
{
	return ((status & f) > 0) ? 1 : 0;
}

// Sets or clears a specific bit of the status register
void spg290::SetFlag(FLAGS290 f, bool v)
{
	if (v)
		status |= f;
	else
		status &= ~f;
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
		}
		else if (opcode == 1)
		{
			if ((instr & func3_MASK) >> 18 == 0x4)
			{
				// We have an ANDIX instruction
				ANDIX();
			}
		}
		else if (opcode == 0b01100)
		{
			// We have an ANDRIX instruction
			ANDRIX();
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
	uint32_t d, a, b;
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

	if (instr & CU_MASK)
	{
		// If result is 0, set Z flag bit
		SetFlag(Z, d == 0);
		// If result is negative number, set N flag bit
		SetFlag(N, (d >> 31) == 0);
	}

	// write back the result of the operation
	write(d_reg, d);

	return 1;
}

uint8_t spg290::ANDIX()
{
	// d: destination reg
	// operation: d = d & imm
	uint32_t d;
	uint16_t imm;
	uint8_t d_reg;

	// Extract register locations from instruction word
	d_reg = (instr & 0x3E00000) >> 21;	// bits 25-21 (see s+core7 pg. 12)

	// get the values stored in registers
	d = read(d_reg);

	// Extract the immediate value from the instruction word
	// we have to do the shifting/masking shenanigans because bit 15
	// of the instruction word falls in the middle of the 16-bit immediate
	imm = ((instr & 0x7FFE) >> 1) | ((instr & 0x30000) >> 2);

	// perform operation
	d = d & imm;

	if (instr & CU_MASK)
	{
		// If result is 0, set Z flag bit
		SetFlag(Z, d == 0);
		// If result is negative number, set N flag bit
		SetFlag(N, (d >> 31) == 0);
	}
	// write back the result of the operation
	write(d_reg, d);

	return 1;
}

uint8_t spg290::ANDRIX()
{
	// d: destination reg
	// operation: d = d & imm
	uint32_t d, a;
	uint16_t imm;
	uint8_t d_reg, a_reg;

	// Extract register locations from instruction word
	d_reg = (instr & 0x3E00000) >> 21;	// bits 25-21 (see s+core7 pg. 12)
	a_reg = (instr & 0x1F0000) >> 16;	// bits 20-16 (see s+core7 pg. 12)
	
	// get the values stored in registers
	a = read(a_reg);

	// Extract the immediate value from the instruction word
	// here the immediate is 14-bits
	imm = ((instr & 0x7FFE) >> 1);

	// perform operation
	d = a & imm;

	if (instr & CU_MASK)
	{
		// If result is 0, set Z flag bit
		SetFlag(Z, d == 0);
		// If result is negative number, set N flag bit
		SetFlag(N, (d >> 31) == 0);
	}
	// write back the result of the operation
	write(d_reg, d);

	return 1;
}