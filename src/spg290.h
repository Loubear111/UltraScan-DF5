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

	// I don't think SPG290 has an accumulator, so we probs don't need this
	uint8_t acc = 0x00; // Accumulator register (this is where we store temp numbers for arithmetic)
	uint8_t x = 0x00; // X register
	uint8_t y = 0x00; // Y register
	uint8_t stkp = 0x00; // stack pointer (points to location on bus)

	// program counter, 32 bits
	uint32_t pc = 0x00000000;

	// status register
	uint8_t status = 0x00;

	uint8_t GetFlag(FLAGS290 f);

	void clock();

private:

	Bus		*bus = nullptr;
	uint32_t read(uint32_t a);
	void	write(uint32_t a, uint8_t d);

	void	SetFlag(FLAGS290 f, bool v);

	uint32_t  instr = 0x00;    // Is the instruction byte
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
	// masks out bits 30-26 of instr which are the opcode
	uint32_t	OPCODE_MASK	= 0x7C000000;
	
	// masks out bits 6-1 of instr which are func6 (for "Special-Form" Instructions) 
	// see page 12 of S+Core7 programmers manual
	uint8_t		func6_MASK	= 0x7E;

	/* OPCODES */
	uint8_t ANDX();		// Logical AND	
	uint8_t UNDEF();	// catches undefined opcodes
};