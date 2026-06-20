#include "spg290.h"
#include <iostream>
#include <stdint.h>
#include <string>
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

uint8_t spg290::ADDCX()
{
    // Destination and source registers
    uint32_t d, a, b;
    uint8_t d_reg, a_reg, b_reg;
    bool carry_in, carry_out;
    uint64_t sum;

    // Extract register locations from instruction word
	d_reg = (instr & 0x3E00000) >> 21;	// bits 25-21 (see s+core7 pg. 12)
	a_reg = (instr & 0x1F0000) >> 16;	// bits 20-16 (see s+core7 pg. 12)
	b_reg = (instr & 0x7C00) >> 10;		// bits 14-10 (see s+core7 pg. 12)

    // Read values from registers
    a = read(a_reg);
    b = read(b_reg);

    // Retrieve current carry flag
    carry_in = GetFlag(C);

    // Perform addition with carry
    sum = (uint64_t)a + (uint64_t)b + (carry_in ? 1 : 0);
    d = sum & 0xFFFFFFFF;          // Truncate to 32 bits
    carry_out = (sum >> 32) != 0;  // Determine carry out

    // Update flags if CU bit is set
    if (instr & CU_MASK)
    {
        SetFlag(Z, d == 0);              // Zero flag
        SetFlag(N, (d >> 31) == 0);      // Negative flag 
        SetFlag(C, carry_out);           // Carry flag
    }

    // Write result to destination register
    write(d_reg, d);

    return 1;  // Return cycle count or status
}

uint8_t spg290::ADDIX()
{
    uint8_t d_reg;
    uint32_t d_val;
    int32_t imm;

    // Extract destination register from bits 25-21
    d_reg = (instr & 0x3E00000) >> 21;

    // Extract 16-bit signed immediate and sign-extend to 32 bits
    imm = (int16_t)(instr & 0xFFFF);
    uint32_t sign_extended_imm = (uint32_t)imm;

    // Read the current value of the destination register
    d_val = read(d_reg);

    // Perform the addition
    d_val += sign_extended_imm;

    // Check if the CU bit is set to update condition flags
    if (instr & CU_MASK)
    {
        // Set Z flag if the result is zero
        SetFlag(Z, d_val == 0);
        // Set N flag if the result is non-negative (based on MSB)
        SetFlag(N, (d_val >> 31) == 0);
    }

    // Write the result back to the destination register
    write(d_reg, d_val);

    return 1;
}

uint8_t spg290::ADDISX()
{
    uint32_t d_val;
    uint8_t d_reg;
    int32_t imm16;

    // Extract destination register rD from bits 25-21
    d_reg = (instr & 0x3E00000) >> 21;

    // Extract the 16-bit signed immediate value (SImm16) from bits 15-0 and sign-extend
    imm16 = (int16_t)(instr & 0xFFFF);

    // Read the current value of register rD
    d_val = read(d_reg);

    // Calculate the 16-bit left-shifted immediate value
    uint32_t shifted_imm = (uint32_t)imm16 << 16;

    // Perform the addition
    d_val += shifted_imm;

    // Check if the CU bit is set to update condition flags
    if (instr & CU_MASK)
    {
        // Set Z flag if the result is zero
        SetFlag(Z, d_val == 0);
        // Set N flag if the result is negative (highest bit is 1)
        SetFlag(N, (d_val >> 31) != 0);
    }

    // Write the result back to register rD
    write(d_reg, d_val);

    return 1;
}

uint8_t spg290::ADDRIX()
{
    uint32_t d, a;
    int32_t imm14;
    uint8_t d_reg, a_reg;

    // Extract destination and source registers from instruction word
    d_reg = (instr & 0x3E00000) >> 21;  // Bits 25-21 for rD
    a_reg = (instr & 0x1F0000) >> 16;  // Bits 20-16 for rA

    // Extract 14-bit signed immediate (bits 13-0)
    imm14 = instr & 0x3FFF;
    // Sign-extend the 14-bit value to 32 bits
    imm14 = (imm14 << 18) >> 18;

    // Read value from source register rA
    a = read(a_reg);

    // Perform addition: d = a + sign-extended imm14
    d = a + imm14;

    // Update condition flags if the CU bit is set
    if (instr & CU_MASK)
    {
        SetFlag(Z, d == 0);               // Set Z flag if result is zero
        SetFlag(N, (d >> 31) == 0);       // Set N flag based on sign bit (as per ANDX example)
    }

    // Write the result to destination register rD
    write(d_reg, d);

    return 1;  // Execution completed successfully
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

uint8_t spg290::ANDISX()
{
    uint32_t d, a, imm_shifted;
    uint8_t d_reg;
    uint16_t imm16;
    
    // Extract destination register (rD) from bits 25-21
    d_reg = (instr & 0x3E00000) >> 21;

    // Extract Imm16 from bits 15-0
    imm16 = instr & 0xFFFF;

    // Shift Imm16 left by 16 bits to form the 32-bit value
    imm_shifted = (uint32_t)imm16 << 16;

    // Read current value of rD (source operand)
    a = read(d_reg);

    // Perform the AND operation
    d = a & imm_shifted;

    // Check if condition flags need to be updated
    if (instr & CU_MASK)
    {
        // If result is 0, set Z flag
        SetFlag(Z, d == 0);
        // If result's MSB is 0, set N flag (following ORX's convention)
        SetFlag(N, (d >> 31) == 0);
    }

    // Write the result back to rD
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

uint8_t spg290::BITTSTC()
{
    // a: source reg
    uint8_t a_reg, bn;
    uint32_t a;
    
    // Extract register and bit number from instruction word
    a_reg = (instr & 0x1F0000) >> 16; // bits 20-16 (rA)
    bn = (instr & 0x7C00) >> 10;      // bits 14-10 (BN)
    
    // Read the value from register rA
    a = read(a_reg);
    
    // Compute the bit mask and test the bit
    uint32_t bit_mask = 1 << bn;
    uint32_t bit_value = a & bit_mask;
    
    // Update flags if CU bit is set (always true for .c suffix)
    if (instr & CU_MASK)
    {
        // Z is set if the tested bit is 0
        SetFlag(Z, bit_value == 0);
        // N is set if the tested bit is the sign bit (bit 31) and it's set
        SetFlag(N, (bit_value >> 31) & 1);
    }
    
    return 1; // Execution cycle count
}

uint8_t spg290::CEINST() {
    //THIS METHOD IS ALL WRONG BUT MAY BE ABLE TO USE OUTLINE
    uint32_t a = 0, b = 0, result = 0;
    uint8_t func5, usd1, rA_reg, rB_reg, usd2, dest_reg;

    // Extract fields from the instruction word
    func5 = (instr & 0x3E000000) >> 25; // func5: bits 29-25 (operation code)
    usd1   = (instr & 0x1F00000)  >> 20; // USD1:  bits 24-20
    rA_reg = (instr & 0xF8000)    >> 15; // rA:    bits 19-15
    rB_reg = (instr & 0x7C00)     >> 10; // rB:    bits 14-10
    usd2   = (instr & 0x3E0)      >> 5;  // USD2:  bits 9-5

    // Check if custom engine supports this func5 operation
    //if (!IsCustomOpSupported(func5)) {
    //    throw RI_Exception(); // Reserved instruction exception
    //    return 0;
    //}

    // Fetch source operands if required by the operation
    //if (OperationUsesSourceA(func5))
    //    a = read(rA_reg);
    //if (OperationUsesSourceB(func5))
    //    b = read(rB_reg);

    // Execute custom operation based on func5
    //switch (func5) {
        // Example operation: ADD with USD1 as destination
        //case CE_ADD:
        //    result = a + b;
        //    dest_reg = usd1;
        //    break;
        // Example operation: AND with immediate USD2 and store in USD1
        //case CE_ANDI:
        //    result = a & usd2;
        //    dest_reg = usd1;
        //    break;
        // Handle other custom operations...
        //default:
        //    throw CeE_Exception(); // Custom engine execution exception
        //    return 0;
    //}

    // Write result to destination (could be USD1, USD2, or another reg)
    write(dest_reg, result);

    // Conditionally update flags if CU_MASK is set (optional)
    if (instr & CU_MASK) {
        SetFlag(Z, result == 0);
        SetFlag(N, result >> 31);
    }

    return 1; // Execution successful
}


uint8_t spg290::ORX()
{
    // d: destination reg
    // a: source reg
    // b: source reg
    // operation: d = a | b
    uint32_t d, a, b;
    uint8_t d_reg, a_reg, b_reg;
    
    // Extract register locations from instruction word (same as ANDX)
    d_reg = (instr & 0x3E00000) >> 21; // bits 25-21
    a_reg = (instr & 0x1F0000) >> 16;  // bits 20-16
    b_reg = (instr & 0x7C00) >> 10;    // bits 14-10

    // Read values from registers
    a = read(a_reg);
    b = read(b_reg);

    // Perform OR operation
    d = a | b;

    // Update flags if CU_MASK is set
    if (instr & CU_MASK)
    {
        SetFlag(Z, d == 0);              // Set Z flag if result is zero
        SetFlag(N, (d >> 31) == 0);      // Set N flag based on sign bit (matches ANDX logic)
    }

    // Write result to destination register
    write(d_reg, d);

    return 1;
}