#pragma once

#include <vector>

class Bus;

class spg290
{
public:
	spg290();
	~spg290();

public:
	// Link the CPU to the communication Bus
	void ConnectBus(Bus *n) { bus = n; }

	enum FLAGS290
	{
		// TODO: Find out what flags there are for the spg290
		/*
		Here's an example of flags for the NES's 6502 chip
		The SPG290 should be somewhat similar...
		C = (1 << 0),	// Carry Bit
		Z = (1 << 1),	// Zero
		I = (1 << 2),	// Disable Interrupts
		D = (1 << 3),	// Decimal Mode (unused in this implementation)
		B = (1 << 4),	// Break
		U = (1 << 5),	// Unused
		V = (1 << 6),	// Overflow
		N = (1 << 7),	// Negative
		*/

		// k, can't find documentation on spg290 flag bits so... 
		// going to use the NES for now since its fairly similar I hope
		C = (1 << 0),	// Carry Bit
		Z = (1 << 1),	// Zero
		I = (1 << 2),	// Disable Interrupts
		D = (1 << 3),	// Decimal Mode (unused in this implementation)
		B = (1 << 4),	// Break
		U = (1 << 5),	// Unused
		V = (1 << 6),	// Overflow
		N = (1 << 7),	// Negative
	};

	uint8_t a = 0x00; // Accumulator register (this is where we store temp numbers for arithmetic)
	uint8_t x = 0x00; // X register
	uint8_t y = 0x00; // Y register
	uint8_t stkp = 0x00; // stack pointer (points to location on bus)

	// program counter... maybe 16-bits??
	uint16_t pc = 0x0000;

	// status register
	uint8_t status = 0x00;

	void clock();

	uint8_t fetch();
	uint8_t fetched = 0x00;

private:

	Bus		*bus = nullptr;
	uint8_t read(uint16_t a);
	void	write(uint16_t a, uint8_t d);

	uint8_t GetFlag(FLAGS290 f);
	void	SetFlag(FLAGS290 f, bool v);

	uint8_t  opcode = 0x00;    // Is the instruction byte
	uint8_t  cycles = 0;	   // Counts how many cycles the instruction has remaining

	struct INSTRUCTION
	{
		std::string name; // OPCODE
		uint8_t(spg290::*operate)(void) = nullptr;  // pointer to instr function
		uint8_t		instrLength = 0; // 32-bit or 16-bit instr
		uint8_t     cycles = 0; // Cycles required to execute
	};
	std::vector<INSTRUCTION> lookup;

private:
	/* OPCODES */
	uint8_t ANDX();		// Logical AND	
	uint8_t UNDEF();	// catches undefined opcodes
};