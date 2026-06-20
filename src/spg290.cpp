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

void spg290::writeReg(uint8_t r, uint32_t val)
{
	if (r != 0)
		regs[r] = val;
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
		pc++;

		// Bit 31 is the 32-bit instruction indicator. When it is clear,
		// the word holds a 16-bit ("!") instruction in its low 16 bits.
		if ((instr & 0x80000000u) == 0)
		{
			decode16();
			cycles = 1;
			cycles--;
			return;
		}

		// Extract opcode from instruction word
		// opcode is bits 30-26 so we must shift it over after masking
		uint8_t opcode = (instr & OPCODE_MASK) >> 26;

		switch (opcode)
		{
		case OP_RTYPE:
		{
			// R-type: secondary function selected by func6 (bits 6:1)
			uint8_t func6 = (instr & func6_MASK) >> 1;
			switch (func6)
			{
			case F6_NOP:     NOP();     break;
			case F6_ADDX:    ADDX();    break;
			case F6_ADDCX:   ADDCX();   break;
			case F6_SUBX:    SUBX();    break;
			case F6_SUBCX:   SUBCX();   break;
			case F6_NEGX:    NEGX();    break;
			case F6_CMP:     CMP();     break;
			case F6_CMPZ:    CMPZ();    break;
			case F6_MVCOND:  MVCOND();  break;
			case F6_MTCEH:   MTCEH();   break;
			case F6_REM:     REM();     break;
			case F6_REMU:    REMU();    break;
			case F6_ANDX:    ANDX();    break;
			case F6_ORX:     ORX();     break;
			case F6_XORX:    XORX();    break;
			case F6_NOTX:    NOTX();    break;
			case F6_BITTSTC: BITTSTC(); break;
			case F6_ROLC:    ROLC();    break;
			case F6_ROLIC:   ROLIC();   break;
			case F6_RORC:    RORC();    break;
			case F6_RORIC:   RORIC();   break;
			case F6_ADDS:    ADDS();    break;
			case F6_SUBS:    SUBS();    break;
			case F6_ABSS:    ABSS();    break;
			case F6_SLLS:    SLLS();    break;
			case F6_MSBF:    MSBF();    break;
			case F6_SLLX:    SLLX();    break;
			case F6_SRLX:    SRLX();    break;
			case F6_SRAX:    SRAX();    break;
			case F6_ROLX:    ROLX();    break;
			case F6_RORX:    RORX();    break;
			case F6_SLLIX:   SLLIX();   break;
			case F6_SRLIX:   SRLIX();   break;
			case F6_SRAIX:   SRAIX();   break;
			case F6_ROLIX:   ROLIX();   break;
			case F6_RORIX:   RORIX();   break;
			case F6_EXTSBX:  EXTSBX();  break;
			case F6_EXTSHX:  EXTSHX();  break;
			case F6_EXTZBX:  EXTZBX();  break;
			case F6_EXTZHX:  EXTZHX();  break;
			case F6_CLZ:     CLZ();     break;
			case F6_ABS:     ABS();     break;
			case F6_MIN:     MIN();     break;
			case F6_MAX:     MAX();     break;
			case F6_BITREV:  BITREV();  break;
			case F6_MUL:     MUL();     break;
			case F6_MULU:    MULU();    break;
			case F6_DIV:     DIV();     break;
			case F6_DIVU:    DIVU();    break;
			case F6_MFCEL:   MFCEL();   break;
			case F6_MFCEH:   MFCEH();   break;
			case F6_MTCEL:   MTCEL();   break;
			case F6_MAD:     MAD();     break;
			case F6_MADU:    MADU();    break;
			case F6_MSB:     MSB();     break;
			case F6_MSBU:    MSBU();    break;
			case F6_MULF:    MULF();    break;
			case F6_MADF:    MADF();    break;
			default: break;
			}
			break;
		}
		case OP_ITYPE1:
		{
			uint8_t func3 = (instr & func3_MASK) >> 18;
			switch (func3)
			{
			case F3_ADDIX:  ADDIX();  break;
			case F3_ORIX:   ORIX();   break;
			case F3_CMPI:   CMPI();   break;
			case F3_LDI:    LDI();    break;
			case F3_ANDIX:  ANDIX();  break;
			case F3_ANDISX: ANDISX(); break;
			case F3_ADDISX: ADDISX(); break;
			case F3_ORISX:  ORISX();  break;
			default: break;
			}
			break;
		}
		case OP_ITYPE2:
		{
			uint8_t func3 = (instr & func3_MASK) >> 18;
			switch (func3)
			{
			case F3_LDIS:   LDIS();   break;
			case F3_SUBIX:  SUBIX();  break;
			case F3_SUBISX: SUBISX(); break;
			default: break;
			}
			break;
		}
		case OP_ANDRIX: ANDRIX(); break;
		case OP_ADDRIX: ADDRIX(); break;
		case OP_ORRIX:  ORRIX();  break;
		case OP_SUBRIX: SUBRIX(); break;
		case OP_LW:     LW();     break;
		case OP_SW:     SW();     break;
		case OP_LBU:    LBU();    break;
		case OP_LB:     LB();     break;
		case OP_LHU:    LHU();    break;
		case OP_LH:     LH();     break;
		case OP_SB:     SB();     break;
		case OP_SH:     SH();     break;
		case OP_BCOND:  BCOND();  break;
		case OP_JX:     JX();     break;
		case OP_BR:     BR();     break;
		case OP_BCONDL: BCONDL(); break;
		case OP_BRL:    BRL();    break;
		case OP_LSU_UPD:LSU_UPD();break;
		case OP_HWMAC:  HWMAC();  break;
		case OP_SYS:    SYS();    break;
		default: break;
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
    a = regs[a_reg];
    b = regs[b_reg];

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
    writeReg(d_reg, d);

    return 1;  // Return cycle count or status
}

uint8_t spg290::ADDIX()
{
    uint8_t d_reg;
    uint32_t d_val;
    int32_t imm;

    // Extract destination register from bits 25-21
    d_reg = (instr & 0x3E00000) >> 21;

    // Extract 16-bit signed immediate (bits 16:1) and sign-extend to 32 bits
    imm = (int16_t)((instr >> 1) & 0xFFFF);
    uint32_t sign_extended_imm = (uint32_t)imm;

    // Read the current value of the destination register
    d_val = regs[d_reg];

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
    writeReg(d_reg, d_val);

    return 1;
}

uint8_t spg290::ADDISX()
{
    uint32_t d_val;
    uint8_t d_reg;
    int32_t imm16;

    // Extract destination register rD from bits 25-21
    d_reg = (instr & 0x3E00000) >> 21;

    // Extract the 16-bit signed immediate value (SImm16) from bits 16-1 and sign-extend
    imm16 = (int16_t)((instr >> 1) & 0xFFFF);

    // Read the current value of register rD
    d_val = regs[d_reg];

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
    writeReg(d_reg, d_val);

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

    // Extract 14-bit signed immediate (bits 14-1)
    imm14 = (instr >> 1) & 0x3FFF;
    // Sign-extend the 14-bit value to 32 bits
    imm14 = (imm14 << 18) >> 18;

    // Read value from source register rA
    a = regs[a_reg];

    // Perform addition: d = a + sign-extended imm14
    d = a + imm14;

    // Update condition flags if the CU bit is set
    if (instr & CU_MASK)
    {
        SetFlag(Z, d == 0);               // Set Z flag if result is zero
        SetFlag(N, (d >> 31) == 0);       // Set N flag based on sign bit (as per ANDX example)
    }

    // Write the result to destination register rD
    writeReg(d_reg, d);

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
	a = regs[a_reg];
	b = regs[b_reg];

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
	writeReg(d_reg, d);

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
	d = regs[d_reg];

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
	writeReg(d_reg, d);

	return 1;
}

uint8_t spg290::ANDISX()
{
    uint32_t d, a, imm_shifted;
    uint8_t d_reg;
    uint16_t imm16;
    
    // Extract destination register (rD) from bits 25-21
    d_reg = (instr & 0x3E00000) >> 21;

    // Extract Imm16 from bits 16-1
    imm16 = (instr >> 1) & 0xFFFF;

    // Shift Imm16 left by 16 bits to form the 32-bit value
    imm_shifted = (uint32_t)imm16 << 16;

    // Read current value of rD (source operand)
    a = regs[d_reg];

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
    writeReg(d_reg, d);

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
	a = regs[a_reg];

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
	writeReg(d_reg, d);

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
    a = regs[a_reg];
    
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
    writeReg(dest_reg, result);

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
    a = regs[a_reg];
    b = regs[b_reg];

    // Perform OR operation
    d = a | b;

    // Update flags if CU_MASK is set
    if (instr & CU_MASK)
    {
        SetFlag(Z, d == 0);              // Set Z flag if result is zero
        SetFlag(N, (d >> 31) == 0);      // Set N flag based on sign bit (matches ANDX logic)
    }

    // Write result to destination register
    writeReg(d_reg, d);

    return 1;
}

// ====================================================================
// Field-extraction helpers follow the encoding documented in spg290.h:
//   rD = bits 25:21, rA = bits 20:16, rB / SA = bits 14:10,
//   imm16 = bits 16:1, imm14 = bits 14:1, CU = bit 0.
// ====================================================================

// Evaluate a 4-bit execution condition (EC) against the current flags.
// Encoding per S+core7 Table 2-8 (mv{cond}).
bool spg290::evalCondition(uint8_t ec)
{
    bool c = GetFlag(C);
    bool z = GetFlag(Z);
    bool n = GetFlag(N);
    bool v = GetFlag(V);

    switch (ec & 0xF)
    {
    case 0:  return c;              // CS  : carry set (>= unsigned)
    case 1:  return !c;             // CC  : carry clear (< unsigned)
    case 2:  return c && !z;        // GTU : > unsigned
    case 3:  return !c || z;        // LEU : <= unsigned
    case 4:  return z;              // EQ  : ==
    case 5:  return !z;             // NE  : !=
    case 6:  return !z && (n == v); // GT  : > signed
    case 7:  return z || (n != v);  // LE  : <= signed
    case 8:  return n == v;         // GE  : >= signed
    case 9:  return n != v;         // LT  : < signed
    case 10: return n;              // MI  : negative
    case 11: return !n;             // PL  : positive / zero
    case 12: return v;              // VS  : overflow
    case 13: return !v;             // VC  : no overflow
    case 14: return false;          // CNZ : no operation
    case 15: return true;           // AL  : always
    default: return false;
    }
}

uint8_t spg290::ADDX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;

    uint32_t a = regs[a_reg];
    uint32_t b = regs[b_reg];
    uint64_t full = (uint64_t)a + (uint64_t)b;
    uint32_t r = (uint32_t)full;

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, (full >> 32) & 1);
        SetFlag(V, ((~(a ^ b) & (a ^ r)) >> 31) & 1);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::SUBX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;

    uint32_t a = regs[a_reg];
    uint32_t b = regs[b_reg];
    uint32_t r = a - b;

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, a >= b);                              // C = ~borrow
        SetFlag(V, (((a ^ b) & (a ^ r)) >> 31) & 1);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::SUBCX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;

    uint32_t a = regs[a_reg];
    uint32_t b = regs[b_reg];
    uint8_t  cin = GetFlag(C);
    // GPRrD = GPRrA - GPRrB - (~C)  where (~C) is the 1-bit complement of C
    uint64_t sub = (uint64_t)b + (uint64_t)(1 - cin);
    uint32_t r = (uint32_t)((uint64_t)a - sub);

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, (uint64_t)a >= sub);
        SetFlag(V, (((a ^ b) & (a ^ r)) >> 31) & 1);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::NEGX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t b_reg = (instr & 0x7C00) >> 10;   // neg rD, rB

    uint32_t b = regs[b_reg];
    uint32_t r = 0u - b;

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, b == 0);                  // ~borrow(0 - b)
        SetFlag(V, b == 0x80000000u);        // overflow only for INT_MIN
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::NOTX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;

    uint32_t r = ~regs[a_reg];

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::NOP()
{
    return 1;
}

uint8_t spg290::XORX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;

    uint32_t r = regs[a_reg] ^ regs[b_reg];

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
    }

    writeReg(d_reg, r);
    return 1;
}

// Shared subtract-and-flag for cmp / cmpz: computes a - b, updates N/Z/C/V,
// and updates T based on the 2-bit TCS field (00 -> T=Z, 01 -> T=N, else keep).
uint8_t spg290::CMP()
{
    uint8_t tcs   = (instr >> 21) & 0x3;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;

    uint32_t a = regs[a_reg];
    uint32_t b = regs[b_reg];
    uint32_t r = a - b;

    SetFlag(N, (r >> 31) & 1);
    SetFlag(Z, r == 0);
    SetFlag(C, a >= b);
    SetFlag(V, (((a ^ b) & (a ^ r)) >> 31) & 1);

    if (tcs == 0)      SetFlag(T, GetFlag(Z));
    else if (tcs == 1) SetFlag(T, GetFlag(N));

    return 1;
}

uint8_t spg290::CMPZ()
{
    uint8_t tcs   = (instr >> 21) & 0x3;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;

    uint32_t a = regs[a_reg];

    SetFlag(N, (a >> 31) & 1);
    SetFlag(Z, a == 0);
    SetFlag(C, true);   // ~borrow(a - 0) is always true
    SetFlag(V, false);

    if (tcs == 0)      SetFlag(T, GetFlag(Z));
    else if (tcs == 1) SetFlag(T, GetFlag(N));

    return 1;
}

uint8_t spg290::MVCOND()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t ec    = (instr >> 10) & 0xF;   // EC occupies low 4 bits of the rB field

    if (evalCondition(ec))
        writeReg(d_reg, regs[a_reg]);

    return 1;
}

uint8_t spg290::ORIX()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    uint32_t imm16 = (instr >> 1) & 0xFFFF;        // zero-extended
    uint32_t r = regs[d_reg] | imm16;

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::ORISX()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    uint32_t imm16 = (instr >> 1) & 0xFFFF;
    uint32_t r = regs[d_reg] | (imm16 << 16);

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::ORRIX()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    uint8_t  a_reg = (instr & 0x1F0000) >> 16;
    uint32_t imm14 = (instr >> 1) & 0x3FFF;        // zero-extended
    uint32_t r = regs[a_reg] | imm14;

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::SUBIX()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    int32_t  imm   = (int16_t)((instr >> 1) & 0xFFFF);   // sign-extended
    uint32_t a = regs[d_reg];
    uint32_t b = (uint32_t)imm;
    uint32_t r = a - b;

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, a >= b);
        SetFlag(V, (((a ^ b) & (a ^ r)) >> 31) & 1);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::SUBISX()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    int32_t  imm   = (int16_t)((instr >> 1) & 0xFFFF);
    uint32_t a = regs[d_reg];
    uint32_t b = ((uint32_t)imm) << 16;
    uint32_t r = a - b;

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, a >= b);
        SetFlag(V, (((a ^ b) & (a ^ r)) >> 31) & 1);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::SUBRIX()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    uint8_t  a_reg = (instr & 0x1F0000) >> 16;
    int32_t  imm14 = (instr >> 1) & 0x3FFF;
    imm14 = (imm14 << 18) >> 18;                 // sign-extend 14-bit
    uint32_t a = regs[a_reg];
    uint32_t b = (uint32_t)imm14;
    uint32_t r = a - b;

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, a >= b);
        SetFlag(V, (((a ^ b) & (a ^ r)) >> 31) & 1);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::CMPI()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    int32_t  imm   = (int16_t)((instr >> 1) & 0xFFFF);
    uint32_t a = regs[d_reg];
    uint32_t b = (uint32_t)imm;
    uint32_t r = a - b;

    SetFlag(N, (r >> 31) & 1);
    SetFlag(Z, r == 0);
    SetFlag(C, a >= b);
    SetFlag(V, (((a ^ b) & (a ^ r)) >> 31) & 1);
    SetFlag(T, GetFlag(Z));

    return 1;
}

uint8_t spg290::LDI()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    int32_t imm   = (int16_t)((instr >> 1) & 0xFFFF);  // sign-extended
    writeReg(d_reg, (uint32_t)imm);
    return 1;
}

uint8_t spg290::LDIS()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    uint32_t imm16 = (instr >> 1) & 0xFFFF;
    writeReg(d_reg, imm16 << 16);
    return 1;
}

// ---- Shifts / rotates ----------------------------------------------

uint8_t spg290::SLLX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint8_t  sa = regs[b_reg] & 0x1F;
    uint32_t r = (sa == 0) ? a : (a << sa);

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, sa == 0 ? false : ((a >> (32 - sa)) & 1));
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::SRLX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint8_t  sa = regs[b_reg] & 0x1F;
    uint32_t r = (sa == 0) ? a : (a >> sa);

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, sa == 0 ? false : ((a >> (sa - 1)) & 1));
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::SRAX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint8_t  sa = regs[b_reg] & 0x1F;
    uint32_t r = (sa == 0) ? a : (uint32_t)((int32_t)a >> sa);

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, sa == 0 ? false : ((a >> (sa - 1)) & 1));
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::ROLX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint8_t  sa = regs[b_reg] & 0x1F;
    uint32_t r = (sa == 0) ? a : ((a << sa) | (a >> (32 - sa)));

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        if (sa != 0) SetFlag(C, (a >> (32 - sa)) & 1);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::RORX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint8_t  sa = regs[b_reg] & 0x1F;
    uint32_t r = (sa == 0) ? a : ((a >> sa) | (a << (32 - sa)));

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        if (sa != 0) SetFlag(C, (a >> (sa - 1)) & 1);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::SLLIX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t sa    = (instr & 0x7C00) >> 10;   // 5-bit shift amount
    uint32_t a = regs[a_reg];
    uint32_t r = (sa == 0) ? a : (a << sa);

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, sa == 0 ? false : ((a >> (32 - sa)) & 1));
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::SRLIX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t sa    = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint32_t r = (sa == 0) ? a : (a >> sa);

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, sa == 0 ? false : ((a >> (sa - 1)) & 1));
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::SRAIX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t sa    = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint32_t r = (sa == 0) ? a : (uint32_t)((int32_t)a >> sa);

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
        SetFlag(C, sa == 0 ? false : ((a >> (sa - 1)) & 1));
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::ROLIX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t sa    = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint32_t r = (sa == 0) ? a : ((a << sa) | (a >> (32 - sa)));

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        if (sa != 0) SetFlag(C, (a >> (32 - sa)) & 1);
    }

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::RORIX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t sa    = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint32_t r = (sa == 0) ? a : ((a >> sa) | (a << (32 - sa)));

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        if (sa != 0) SetFlag(C, (a >> (sa - 1)) & 1);
    }

    writeReg(d_reg, r);
    return 1;
}

// ---- Bit / extend / misc -------------------------------------------

uint8_t spg290::EXTSBX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint32_t r = (uint32_t)(int32_t)(int8_t)(regs[a_reg] & 0xFF);

    if (instr & CU_MASK)
        SetFlag(N, (r >> 31) & 1);   // Z is undefined per spec

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::EXTSHX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint32_t r = (uint32_t)(int32_t)(int16_t)(regs[a_reg] & 0xFFFF);

    if (instr & CU_MASK)
        SetFlag(N, (r >> 31) & 1);

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::EXTZBX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint32_t r = regs[a_reg] & 0xFF;

    if (instr & CU_MASK)
        SetFlag(N, (r >> 31) & 1);

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::EXTZHX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint32_t r = regs[a_reg] & 0xFFFF;

    if (instr & CU_MASK)
        SetFlag(N, (r >> 31) & 1);

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::CLZ()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint32_t a = regs[a_reg];
    uint32_t r = (a == 0) ? 32u : (uint32_t)__builtin_clz(a);
    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::ABS()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint32_t av = regs[a_reg];
    // Use unsigned negation to avoid signed overflow UB on INT_MIN.
    uint32_t r = ((int32_t)av >= 0) ? av : (0u - av);
    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::MIN()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int32_t a = (int32_t)regs[a_reg];
    int32_t b = (int32_t)regs[b_reg];
    writeReg(d_reg, (uint32_t)(a <= b ? a : b));
    return 1;
}

uint8_t spg290::MAX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int32_t a = (int32_t)regs[a_reg];
    int32_t b = (int32_t)regs[b_reg];
    writeReg(d_reg, (uint32_t)(a >= b ? a : b));
    return 1;
}

uint8_t spg290::BITREV()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;

    uint32_t a = regs[a_reg];
    // Shift amount: 0 if rB is r0, else low 5 bits of rB.
    uint8_t sa = (b_reg == 0) ? 0 : (regs[b_reg] & 0x1F);

    // Reverse all 32 bits of rA.
    uint32_t rev = 0;
    for (int i = 0; i < 32; ++i)
        rev |= ((a >> i) & 1u) << (31 - i);

    uint32_t r = (sa == 0) ? rev : (rev >> sa);
    writeReg(d_reg, r);
    return 1;
}

// ---- Custom engine: multiply / divide / moves ----------------------

uint8_t spg290::MUL()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int64_t prod = (int64_t)(int32_t)regs[a_reg] * (int64_t)(int32_t)regs[b_reg];
    cel = (uint32_t)(prod & 0xFFFFFFFF);
    ceh = (uint32_t)((uint64_t)prod >> 32);
    return 1;
}

uint8_t spg290::MULU()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint64_t prod = (uint64_t)regs[a_reg] * (uint64_t)regs[b_reg];
    cel = (uint32_t)(prod & 0xFFFFFFFF);
    ceh = (uint32_t)(prod >> 32);
    return 1;
}

uint8_t spg290::DIV()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int32_t a = (int32_t)regs[a_reg];
    int32_t b = (int32_t)regs[b_reg];
    if (b != 0)
    {
        cel = (uint32_t)(a / b);   // quotient
        ceh = (uint32_t)(a % b);   // remainder
    }
    return 1;
}

uint8_t spg290::DIVU()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint32_t b = regs[b_reg];
    if (b != 0)
    {
        cel = a / b;
        ceh = a % b;
    }
    return 1;
}

uint8_t spg290::REM()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int32_t a = (int32_t)regs[a_reg];
    int32_t b = (int32_t)regs[b_reg];
    if (b != 0)
    {
        cel = (uint32_t)(a / b);
        ceh = (uint32_t)(a % b);
    }
    writeReg(d_reg, ceh);   // rem == div then mfceh
    return 1;
}

uint8_t spg290::REMU()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint32_t b = regs[b_reg];
    if (b != 0)
    {
        cel = a / b;
        ceh = a % b;
    }
    writeReg(d_reg, ceh);
    return 1;
}

uint8_t spg290::MFCEL()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;   // destination GPR
    writeReg(d_reg, cel);
    return 1;
}

uint8_t spg290::MFCEH()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    writeReg(d_reg, ceh);
    return 1;
}

uint8_t spg290::MTCEL()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;    // source GPR
    cel = regs[a_reg];
    return 1;
}

uint8_t spg290::MTCEH()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    ceh = regs[a_reg];
    return 1;
}

// ---- Custom engine: multiply-accumulate ----------------------------

// Helper: current {CEH,CEL} as a single 64-bit accumulator.
static inline uint64_t packCE(uint32_t ceh, uint32_t cel)
{
    return ((uint64_t)ceh << 32) | (uint64_t)cel;
}

uint8_t spg290::MAD()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int64_t prod = (int64_t)(int32_t)regs[a_reg] * (int64_t)(int32_t)regs[b_reg];
    uint64_t acc = packCE(ceh, cel) + (uint64_t)prod;
    cel = (uint32_t)(acc & 0xFFFFFFFF);
    ceh = (uint32_t)(acc >> 32);
    return 1;
}

uint8_t spg290::MADU()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint64_t prod = (uint64_t)regs[a_reg] * (uint64_t)regs[b_reg];
    uint64_t acc = packCE(ceh, cel) + prod;
    cel = (uint32_t)(acc & 0xFFFFFFFF);
    ceh = (uint32_t)(acc >> 32);
    return 1;
}

uint8_t spg290::MSB()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int64_t prod = (int64_t)(int32_t)regs[a_reg] * (int64_t)(int32_t)regs[b_reg];
    uint64_t acc = packCE(ceh, cel) - (uint64_t)prod;
    cel = (uint32_t)(acc & 0xFFFFFFFF);
    ceh = (uint32_t)(acc >> 32);
    return 1;
}

uint8_t spg290::MSBU()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint64_t prod = (uint64_t)regs[a_reg] * (uint64_t)regs[b_reg];
    uint64_t acc = packCE(ceh, cel) - prod;
    cel = (uint32_t)(acc & 0xFFFFFFFF);
    ceh = (uint32_t)(acc >> 32);
    return 1;
}

uint8_t spg290::MULF()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    // Signed-fractional: the Q31*Q31 product is left-shifted by 1.
    int64_t prod = ((int64_t)(int32_t)regs[a_reg] * (int64_t)(int32_t)regs[b_reg]) << 1;
    cel = (uint32_t)((uint64_t)prod & 0xFFFFFFFF);
    ceh = (uint32_t)((uint64_t)prod >> 32);
    return 1;
}

uint8_t spg290::MADF()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int64_t prod = ((int64_t)(int32_t)regs[a_reg] * (int64_t)(int32_t)regs[b_reg]) << 1;
    uint64_t acc = packCE(ceh, cel) + (uint64_t)prod;
    cel = (uint32_t)(acc & 0xFFFFFFFF);
    ceh = (uint32_t)(acc >> 32);
    return 1;
}

// ---- Loads / stores ------------------------------------------------
// The effective address is computed as GPRrA + sext(Imm15) and used as a
// word index into the bus (this emulator models memory as a word array,
// so byte/half accesses operate on the addressed word).

uint8_t spg290::LW()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = (instr >> 1) & 0x7FFF;
    imm = (imm << 17) >> 17;                       // sign-extend 15-bit
    uint16_t addr = (uint16_t)(regs[a_reg] + (uint32_t)imm);
    writeReg(d_reg, read(addr));
    return 1;
}

uint8_t spg290::SW()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = (instr >> 1) & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint16_t addr = (uint16_t)(regs[a_reg] + (uint32_t)imm);
    write(addr, regs[d_reg]);
    return 1;
}

uint8_t spg290::LBU()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = (instr >> 1) & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint16_t addr = (uint16_t)(regs[a_reg] + (uint32_t)imm);
    writeReg(d_reg, read(addr) & 0xFF);
    return 1;
}

uint8_t spg290::LB()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = (instr >> 1) & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint16_t addr = (uint16_t)(regs[a_reg] + (uint32_t)imm);
    writeReg(d_reg, (uint32_t)(int32_t)(int8_t)(read(addr) & 0xFF));
    return 1;
}

uint8_t spg290::LHU()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = (instr >> 1) & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint16_t addr = (uint16_t)(regs[a_reg] + (uint32_t)imm);
    writeReg(d_reg, read(addr) & 0xFFFF);
    return 1;
}

uint8_t spg290::LH()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = (instr >> 1) & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint16_t addr = (uint16_t)(regs[a_reg] + (uint32_t)imm);
    writeReg(d_reg, (uint32_t)(int32_t)(int16_t)(read(addr) & 0xFFFF));
    return 1;
}

uint8_t spg290::SB()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = (instr >> 1) & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint16_t addr = (uint16_t)(regs[a_reg] + (uint32_t)imm);
    uint32_t word = (read(addr) & ~0xFFu) | (regs[d_reg] & 0xFFu);
    write(addr, word);
    return 1;
}

uint8_t spg290::SH()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = (instr >> 1) & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint16_t addr = (uint16_t)(regs[a_reg] + (uint32_t)imm);
    uint32_t word = (read(addr) & ~0xFFFFu) | (regs[d_reg] & 0xFFFFu);
    write(addr, word);
    return 1;
}

// ---- Control flow --------------------------------------------------
// pc is a word index; it has already been advanced past the current
// instruction by the time these execute, so PCcurrent == (pc - 1).

uint8_t spg290::BCOND()
{
    uint8_t bc   = (instr >> 22) & 0xF;
    int32_t disp = (instr >> 1) & 0x1FFFFF;        // 21-bit signed displacement
    disp = (disp << 11) >> 11;                     // sign-extend

    if (evalCondition(bc))
        pc = (pc - 1) + (uint32_t)disp;

    return 1;
}

uint8_t spg290::JX()
{
    uint32_t disp = (instr >> 1) & 0xFFFFFF;        // 24-bit absolute word index
    uint8_t  lk   = instr & 0x1;

    if (lk)
        writeReg(3, pc);     // link register r3 = address of following instruction

    pc = disp;
    return 1;
}

uint8_t spg290::BR()
{
    uint8_t bc    = (instr >> 22) & 0xF;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;

    if (evalCondition(bc))
        pc = regs[a_reg];

    return 1;
}

uint8_t spg290::BCONDL()
{
    uint8_t bc   = (instr >> 22) & 0xF;
    int32_t disp = (instr >> 1) & 0x1FFFFF;        // 21-bit signed displacement
    disp = (disp << 11) >> 11;                     // sign-extend

    // Link register r3 = address of the following instruction (pc already
    // advanced). The link is always written, regardless of the condition.
    writeReg(3, pc);

    if (evalCondition(bc))
        pc = (pc - 1) + (uint32_t)disp;

    return 1;
}

uint8_t spg290::BRL()
{
    uint8_t bc    = (instr >> 22) & 0xF;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;

    writeReg(3, pc);   // link unconditionally

    if (evalCondition(bc))
        pc = regs[a_reg];

    return 1;
}

// ---- Rotate through carry ------------------------------------------
// The 33-bit rotate chain is modelled as W, where W[32:1] = GPRrA and
// W[0] = C. Rotating that chain by N and splitting it back out gives the
// new rD (W[32:1]) and the new carry (W[0]), matching the S+core7 spec.

// Shared core: rotate-left-through-carry by sa (0..31). Updates rD/N (and C
// when sa != 0).
static inline void rotcRun(uint32_t a, uint8_t sa, bool carryIn,
                           bool rotateLeft, uint32_t &outR, bool &outC,
                           bool &cChanged)
{
    if (sa == 0)
    {
        outR = a;
        cChanged = false;
        return;
    }

    uint64_t w = ((uint64_t)a << 1) | (carryIn ? 1u : 0u);   // 33-bit chain
    uint32_t n = sa % 33;
    uint64_t rotated;
    if (rotateLeft)
        rotated = ((w << n) | (w >> (33 - n)));
    else
        rotated = ((w >> n) | (w << (33 - n)));
    rotated &= 0x1FFFFFFFFULL;                                // keep 33 bits

    outR = (uint32_t)(rotated >> 1);
    outC = (rotated & 1u) != 0;
    cChanged = true;
}

uint8_t spg290::ROLC()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint8_t  sa = regs[b_reg] & 0x1F;

    uint32_t r; bool c; bool cChanged;
    rotcRun(a, sa, GetFlag(C), true, r, c, cChanged);

    SetFlag(N, (r >> 31) & 1);
    if (cChanged) SetFlag(C, c);

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::ROLIC()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t sa    = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];

    uint32_t r; bool c; bool cChanged;
    rotcRun(a, sa, GetFlag(C), true, r, c, cChanged);

    SetFlag(N, (r >> 31) & 1);
    if (cChanged) SetFlag(C, c);

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::RORC()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];
    uint8_t  sa = regs[b_reg] & 0x1F;

    uint32_t r; bool c; bool cChanged;
    rotcRun(a, sa, GetFlag(C), false, r, c, cChanged);

    SetFlag(N, (r >> 31) & 1);
    if (cChanged) SetFlag(C, c);

    writeReg(d_reg, r);
    return 1;
}

uint8_t spg290::RORIC()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t sa    = (instr & 0x7C00) >> 10;
    uint32_t a = regs[a_reg];

    uint32_t r; bool c; bool cChanged;
    rotcRun(a, sa, GetFlag(C), false, r, c, cChanged);

    SetFlag(N, (r >> 31) & 1);
    if (cChanged) SetFlag(C, c);

    writeReg(d_reg, r);
    return 1;
}

// ---- Saturating arithmetic -----------------------------------------
// All saturating ops clamp the true (wider) result to the signed 32-bit
// range [INT32_MIN, INT32_MAX].

static inline uint32_t satFromI64(int64_t v)
{
    if (v > 0x7FFFFFFFLL)            return 0x7FFFFFFFu;
    if (v < -0x80000000LL)          return 0x80000000u;
    return (uint32_t)(int32_t)v;
}

uint8_t spg290::ADDS()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int64_t sum = (int64_t)(int32_t)regs[a_reg] + (int64_t)(int32_t)regs[b_reg];
    writeReg(d_reg, satFromI64(sum));
    return 1;
}

uint8_t spg290::SUBS()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int64_t diff = (int64_t)(int32_t)regs[a_reg] - (int64_t)(int32_t)regs[b_reg];
    writeReg(d_reg, satFromI64(diff));
    return 1;
}

uint8_t spg290::ABSS()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int64_t a = (int64_t)(int32_t)regs[a_reg];
    int64_t r = (a >= 0) ? a : -a;       // -INT32_MIN clamps to INT32_MAX below
    writeReg(d_reg, satFromI64(r));
    return 1;
}

uint8_t spg290::SLLS()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int64_t a = (int64_t)(int32_t)regs[a_reg];
    uint8_t sa = regs[b_reg] & 0x1F;
    int64_t shifted = a << sa;            // widen first, then clamp
    writeReg(d_reg, satFromI64(shifted));
    return 1;
}

// ---- Custom engine: 32-bit fractional multiply-subtract ------------

uint8_t spg290::MSBF()
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;
    int64_t prod = ((int64_t)(int32_t)regs[a_reg] * (int64_t)(int32_t)regs[b_reg]) << 1;
    uint64_t acc = packCE(ceh, cel) - (uint64_t)prod;
    cel = (uint32_t)(acc & 0xFFFFFFFF);
    ceh = (uint32_t)(acc >> 32);
    return 1;
}

// ---- Half-word multiply-accumulate family --------------------------
// Control field HZFSU lives in bits [25:21]:
//   H = high half  (use bits [31:16] and target CEH; else low half -> CEL)
//   Z = zero start (accumulator base is 0; else current CE register)
//   F = fractional (product is left-shifted by 1)
//   S = subtract   (base - product; else base + product)
//   U = unsigned   (16-bit operands are zero-extended; else sign-extended)
// Saturation (.fs forms) applies when F && !Z.

uint8_t spg290::HWMAC()
{
    uint8_t sel   = (instr & 0x3E00000) >> 21;   // HZFSU
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t b_reg = (instr & 0x7C00) >> 10;

    bool H = sel & 0x10;
    bool Z = sel & 0x08;
    bool F = sel & 0x04;
    bool S = sel & 0x02;
    bool U = sel & 0x01;

    uint32_t va = regs[a_reg];
    uint32_t vb = regs[b_reg];
    uint32_t halfA = H ? (va >> 16) : (va & 0xFFFF);
    uint32_t halfB = H ? (vb >> 16) : (vb & 0xFFFF);

    int64_t product;
    if (U)
        product = (int64_t)(uint32_t)(halfA & 0xFFFF) * (int64_t)(uint32_t)(halfB & 0xFFFF);
    else
        product = (int64_t)(int16_t)(halfA & 0xFFFF) * (int64_t)(int16_t)(halfB & 0xFFFF);

    if (F) product <<= 1;

    uint32_t cur  = H ? ceh : cel;
    int64_t  base = Z ? 0 : (int64_t)(int32_t)cur;
    int64_t  acc  = S ? (base - product) : (base + product);

    uint32_t result;
    if (F && !Z)
        result = satFromI64(acc);          // .fs saturating forms
    else
        result = (uint32_t)(acc & 0xFFFFFFFF);

    if (H) ceh = result; else cel = result;
    return 1;
}

// ---- Pre/post-index load/store family ------------------------------
// Effective address handling mirrors the base load/store ops, but the base
// register rA is always updated by sext(Imm12) afterwards. Pre-index uses
// (rA + imm) as the access address; post-index uses rA, then updates rA.

uint8_t spg290::LSU_UPD()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t sub   = (instr >> 13) & 0x7;          // which load/store
    int32_t imm   = (instr >> 1) & 0xFFF;         // 12-bit immediate
    imm = (imm << 20) >> 20;                      // sign-extend
    bool    pre   = instr & 0x1;

    uint32_t base = regs[a_reg];
    uint16_t addr = (uint16_t)(pre ? (base + (uint32_t)imm) : base);

    switch (sub)
    {
    case 0: // lw
        writeReg(d_reg, read(addr));
        break;
    case 1: // sw
        write(addr, regs[d_reg]);
        break;
    case 2: // lbu
        writeReg(d_reg, read(addr) & 0xFF);
        break;
    case 3: // lb
        writeReg(d_reg, (uint32_t)(int32_t)(int8_t)(read(addr) & 0xFF));
        break;
    case 4: // lhu
        writeReg(d_reg, read(addr) & 0xFFFF);
        break;
    case 5: // lh
        writeReg(d_reg, (uint32_t)(int32_t)(int16_t)(read(addr) & 0xFFFF));
        break;
    case 6: // sb
        write(addr, (read(addr) & ~0xFFu) | (regs[d_reg] & 0xFFu));
        break;
    case 7: // sh
        write(addr, (read(addr) & ~0xFFFFu) | (regs[d_reg] & 0xFFFFu));
        break;
    }

    // Base register writeback happens last so that, when rD == rA on a load,
    // the writeback value wins (matching the spec's sequential semantics).
    writeReg(a_reg, base + (uint32_t)imm);
    return 1;
}

// ====================================================================
// 16-bit instruction decoder. See the layout documented in spg290.h.
// GPR fields address r0..r15; the high bank (r16..r31) is reached via
// the H bit (pop/push) or the cross-bank moves (mlfh/mhfl).
//
// Flag notes from the spec: the "!" ALU ops (add/addc/sub/and/or/xor/
// neg/not) and shifts leave N/Z/C/V undefined, so this model leaves
// them untouched. cmp! updates N/Z/C/V, bittst! updates N/Z, and
// t{cond}! sets T from the condition.
// ====================================================================
uint8_t spg290::decode16()
{
    uint16_t w   = (uint16_t)(instr & 0xFFFF);
    uint8_t  op  = (w >> 12) & 0xF;

    switch (op)
    {
    case 0x0: // RR-ALU : func4[11:8] rD[7:4] rA[3:0]
    {
        uint8_t func = (w >> 8) & 0xF;
        uint8_t d    = (w >> 4) & 0xF;
        uint8_t a    = w & 0xF;
        uint32_t vd = regs[d];
        uint32_t va = regs[a];
        switch (func)
        {
        case 0:  writeReg(d, vd + va);                 break; // add!
        case 1:  writeReg(d, vd + va + GetFlag(C));    break; // addc!
        case 2:  writeReg(d, vd - va);                 break; // sub!
        case 3:  writeReg(d, vd & va);                 break; // and!
        case 4:  writeReg(d, vd | va);                 break; // or!
        case 5:  writeReg(d, vd ^ va);                 break; // xor!
        case 6:  writeReg(d, va);                      break; // mv!
        case 7:  writeReg(d, 0u - va);                 break; // neg!
        case 8:  writeReg(d, ~va);                     break; // not!
        case 9: // cmp! : updates N/Z/C/V
        {
            uint32_t r = vd - va;
            SetFlag(N, (r >> 31) & 1);
            SetFlag(Z, r == 0);
            SetFlag(C, vd >= va);
            SetFlag(V, (((vd ^ va) & (vd ^ r)) >> 31) & 1);
            break;
        }
        case 10: { uint8_t s = va & 0x1F; writeReg(d, s ? (vd << s) : vd); break; }                 // sll!
        case 11: { uint8_t s = va & 0x1F; writeReg(d, s ? (vd >> s) : vd); break; }                 // srl!
        case 12: { uint8_t s = va & 0x1F; writeReg(d, s ? (uint32_t)((int32_t)vd >> s) : vd); break; } // sra!
        default: break;
        }
        break;
    }
    case 0x1: // ldiu! : rD[11:8] Imm8[7:0]
    {
        uint8_t d = (w >> 8) & 0xF;
        writeReg(d, (uint32_t)(w & 0xFF));
        break;
    }
    case 0x2: // R-Imm : func2[11:10] rD[9:6] Imm[5:0]
    {
        uint8_t func = (w >> 10) & 0x3;
        uint8_t d    = (w >> 6) & 0xF;
        uint8_t imm5 = w & 0x1F;
        uint8_t imm4 = w & 0xF;
        uint32_t vd  = regs[d];
        switch (func)
        {
        case 0: writeReg(d, imm5 ? (vd << imm5) : vd);                 break; // slli!
        case 1: writeReg(d, imm5 ? (vd >> imm5) : vd);                 break; // srli!
        case 2: writeReg(d, vd + (1u << imm4));                        break; // addei!
        case 3: writeReg(d, vd - (1u << imm4));                        break; // subei!
        default: break;
        }
        break;
    }
    case 0x3: // bittst! : rD[11:8] BN[7:3]  -> N=rD[31], Z=~rD[BN]
    {
        uint8_t d  = (w >> 8) & 0xF;
        uint8_t bn = (w >> 3) & 0x1F;
        uint32_t vd = regs[d];
        SetFlag(N, (vd >> 31) & 1);
        SetFlag(Z, ((vd >> bn) & 1) == 0);
        break;
    }
    case 0x4: // cross-bank moves : func1[11] rD[7:4] rA[3:0]
    {
        uint8_t func = (w >> 11) & 0x1;
        uint8_t d    = (w >> 4) & 0xF;
        uint8_t a    = w & 0xF;
        if (func == 0) writeReg(d, regs[(uint8_t)(a + 16)]);       // mlfh! rD(lo) = rA(hi)
        else           writeReg((uint8_t)(d + 16), regs[a]);       // mhfl! rD(hi) = rA(lo)
        break;
    }
    case 0x5: // mem simple : func3[11:9] rD[8:5] rA[4:1]
    {
        uint8_t func = (w >> 9) & 0x7;
        uint8_t d    = (w >> 5) & 0xF;
        uint8_t a    = (w >> 1) & 0xF;
        uint16_t addr = (uint16_t)regs[a];
        switch (func)
        {
        case 0: writeReg(d, read(addr));                                            break; // lw!
        case 1: writeReg(d, (uint32_t)(int32_t)(int16_t)(read(addr) & 0xFFFF));     break; // lh!
        case 2: writeReg(d, read(addr) & 0xFF);                                     break; // lbu!
        case 3: write(addr, regs[d]);                                            break; // sw!
        case 4: write(addr, (read(addr) & ~0xFFFFu) | (regs[d] & 0xFFFFu));      break; // sh!
        case 5: write(addr, (read(addr) & ~0xFFu) | (regs[d] & 0xFFu));          break; // sb!
        default: break;
        }
        break;
    }
    case 0x6: // mem BP-relative : func3[11:9] rD[8:5] Imm5[4:0]  (BP = r2)
    {
        uint8_t func = (w >> 9) & 0x7;
        uint8_t d    = (w >> 5) & 0xF;
        uint8_t imm5 = w & 0x1F;
        uint32_t bp  = regs[2];
        switch (func)
        {
        case 0: writeReg(d, read((uint16_t)(bp + (imm5 << 2))));                                          break; // lwp!
        case 1: writeReg(d, (uint32_t)(int32_t)(int16_t)(read((uint16_t)(bp + (imm5 << 1))) & 0xFFFF));   break; // lhp!
        case 2: writeReg(d, read((uint16_t)(bp + imm5)) & 0xFF);                                          break; // lbup!
        case 3: write((uint16_t)(bp + (imm5 << 2)), regs[d]);                                          break; // swp!
        case 4: { uint16_t ad = (uint16_t)(bp + (imm5 << 1)); write(ad, (read(ad) & ~0xFFFFu) | (regs[d] & 0xFFFFu)); break; } // shp!
        case 5: { uint16_t ad = (uint16_t)(bp + imm5);        write(ad, (read(ad) & ~0xFFu)   | (regs[d] & 0xFFu));   break; } // sbp!
        default: break;
        }
        break;
    }
    case 0x7: // stack : func1[11] H[10] rD[9:6] rA[5:2]
    {
        uint8_t func = (w >> 11) & 0x1;
        uint8_t hbit = (w >> 10) & 0x1;
        uint8_t d    = (w >> 6) & 0xF;
        uint8_t a    = (w >> 2) & 0xF;
        uint8_t dreg = hbit ? (uint8_t)(d + 16) : d;
        uint32_t base = regs[a];
        if (func == 0) // pop! : load then post-increment by 4
        {
            writeReg(dreg, read((uint16_t)base));
            writeReg(a, base + 4);
        }
        else           // push! : pre-decrement by 4 then store
        {
            write((uint16_t)(base - 4), regs[dreg]);
            writeReg(a, base - 4);
        }
        break;
    }
    case 0x8: // b{cond}! : cond[11:8] Disp8[7:0]
    {
        uint8_t cond = (w >> 8) & 0xF;
        int32_t disp = (int32_t)(int8_t)(w & 0xFF);    // sign-extended
        if (evalCondition(cond))
            pc = (pc - 1) + (uint32_t)disp;
        break;
    }
    case 0x9: // br{cond}! : cond[11:8] rA[7:4]
    {
        uint8_t cond = (w >> 8) & 0xF;
        uint8_t a    = (w >> 4) & 0xF;
        if (evalCondition(cond))
            pc = regs[a];
        break;
    }
    case 0xA: // jx! : LK[11] Disp11[10:0]
    {
        uint8_t  lk    = (w >> 11) & 0x1;
        uint32_t disp  = w & 0x7FF;
        if (lk)
            writeReg(3, pc);          // link register r3 = following instruction
        pc = disp;
        break;
    }
    case 0xB: // system : func2[11:10] cond[9:6]
    {
        uint8_t func = (w >> 10) & 0x3;
        if (func == 2)             // t{cond}! : T = condition
        {
            uint8_t cond = (w >> 6) & 0xF;
            SetFlag(T, evalCondition(cond));
        }
        // func 0 = nop!, func 1 = sdbbp! (debug breakpoint -> no-op here)
        break;
    }
    default: break;
    }

    return 1;
}

// ====================================================================
// System / coprocessor register moves (OP_SYS).
//   rD       = bits 25:21
//   regnum/rA= bits 20:16  (control/special/coprocessor register number,
//                           or the second GPR for the mfcex/mtcex pair)
//   sub-op   = bits 14:10  (SYS_SUBOP)
//   CP#/{HI,LO} = bits 9:8 (coprocessor number, or HI/LO for cex moves)
// These ops do not touch the condition flags.
// ====================================================================
uint8_t spg290::SYS()
{
    uint8_t sub = (instr >> 10) & 0x1F;
    switch (sub)
    {
    case SYS_MFCR:  return MFCR();
    case SYS_MTCR:  return MTCR();
    case SYS_MFSR:  return MFSR();
    case SYS_MTSR:  return MTSR();
    case SYS_MFCX:  return MFCX();
    case SYS_MTCX:  return MTCX();
    case SYS_MFCCX: return MFCCX();
    case SYS_MTCCX: return MTCCX();
    case SYS_MFCEX: return MFCEX();
    case SYS_MTCEX: return MTCEX();
    case SYS_COPX:  return COPX();
    case SYS_TCOND: return TCOND();
    case SYS_TRAP:  return TRAPCOND();
    case SYS_SYSCALL: return SYSCALL();
    case SYS_RTE:   return RTE();
    case SYS_DRTE:  return DRTE();
    case SYS_LDCX:  return LDCX();
    case SYS_STCX:  return STCX();
    case SYS_LCB:   return LCB();
    case SYS_LCW:   return LCW();
    case SYS_LCE:   return LCE();
    case SYS_SCB:   return SCB();
    case SYS_SCW:   return SCW();
    case SYS_SCE:   return SCE();
    // sleep / cache / pflush / sdbbp / ceinst have no architectural effect
    // in this emulator and execute as no-ops.
    case SYS_SLEEP:
    case SYS_CACHE:
    case SYS_PFLUSH:
    case SYS_SDBBP:
    case SYS_CEINST:
    default:        return 1;
    }
}

uint8_t spg290::MFCR()
{
    uint8_t d_reg  = (instr & 0x3E00000) >> 21;
    uint8_t regnum = (instr & 0x1F0000) >> 16;
    writeReg(d_reg, cr[regnum]);
    return 1;
}

uint8_t spg290::MTCR()
{
    uint8_t d_reg  = (instr & 0x3E00000) >> 21;
    uint8_t regnum = (instr & 0x1F0000) >> 16;
    cr[regnum] = regs[d_reg];
    return 1;
}

uint8_t spg290::MFSR()
{
    uint8_t d_reg  = (instr & 0x3E00000) >> 21;
    uint8_t regnum = (instr & 0x1F0000) >> 16;
    writeReg(d_reg, sr[regnum]);
    return 1;
}

uint8_t spg290::MTSR()
{
    uint8_t a_reg  = (instr & 0x3E00000) >> 21;   // mtsr names its source rA
    uint8_t regnum = (instr & 0x1F0000) >> 16;
    sr[regnum] = regs[a_reg];
    return 1;
}

uint8_t spg290::MFCX()
{
    uint8_t d_reg  = (instr & 0x3E00000) >> 21;
    uint8_t regnum = (instr & 0x1F0000) >> 16;
    uint8_t cp     = (instr >> 8) & 0x3;
    writeReg(d_reg, copData[cp][regnum]);
    return 1;
}

uint8_t spg290::MTCX()
{
    uint8_t d_reg  = (instr & 0x3E00000) >> 21;
    uint8_t regnum = (instr & 0x1F0000) >> 16;
    uint8_t cp     = (instr >> 8) & 0x3;
    copData[cp][regnum] = regs[d_reg];
    return 1;
}

uint8_t spg290::MFCCX()
{
    uint8_t d_reg  = (instr & 0x3E00000) >> 21;
    uint8_t regnum = (instr & 0x1F0000) >> 16;
    uint8_t cp     = (instr >> 8) & 0x3;
    writeReg(d_reg, copCtrl[cp][regnum]);
    return 1;
}

uint8_t spg290::MTCCX()
{
    uint8_t d_reg  = (instr & 0x3E00000) >> 21;
    uint8_t regnum = (instr & 0x1F0000) >> 16;
    uint8_t cp     = (instr >> 8) & 0x3;
    copCtrl[cp][regnum] = regs[d_reg];
    return 1;
}

// mfcex / mtcex: HI=bit9, LO=bit8 select CEH/CEL (and the combined
// {HI,LO}=11 forms mfcehl/mtcehl move both halves at once via rD/rA).
uint8_t spg290::MFCEX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t hilo  = (instr >> 8) & 0x3;
    switch (hilo)
    {
    case 0x1: writeReg(d_reg, cel); break;                       // mfcel
    case 0x2: writeReg(d_reg, ceh); break;                       // mfceh
    case 0x3: writeReg(d_reg, ceh); writeReg(a_reg, cel); break;    // mfcehl
    default: break;
    }
    return 1;
}

uint8_t spg290::MTCEX()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t hilo  = (instr >> 8) & 0x3;
    switch (hilo)
    {
    case 0x1: cel = regs[d_reg]; break;                       // mtcel
    case 0x2: ceh = regs[d_reg]; break;                       // mtceh
    case 0x3: ceh = regs[d_reg]; cel = regs[a_reg]; break;    // mtcehl
    default: break;
    }
    return 1;
}

uint8_t spg290::COPX()
{
    // User-defined coprocessor extension; no architectural side effects to
    // model in this emulator. Executed as a no-op.
    return 1;
}

// t{cond}: set/clear the T flag based on the condition-code test. EC lives in
// bits 20:16. (Note EC=14/CNZ depends on a count register we do not model, so
// it evaluates false.)
uint8_t spg290::TCOND()
{
    uint8_t ec = (instr >> 16) & 0xF;
    SetFlag(T, evalCondition(ec));
    return 1;
}

// Minimal exception entry. The faulting instruction sits at pc-1 (pc has
// already advanced to the following word); save it in EPC (cr5), record the
// cause in ECR (cr2), and vector to the handler address held in EXCPvec (cr3).
void spg290::raiseException(uint8_t code)
{
    cr[5] = pc - 1;
    cr[2] = code;
    pc = cr[3];
}

// trap{cond}: raise a trap exception when the condition holds, otherwise it is
// a no-op. EC=14 is defined as "no operation" and evalCondition() returns false
// for it, which matches that behaviour.
uint8_t spg290::TRAPCOND()
{
    uint8_t ec = (instr >> 16) & 0xF;
    if (evalCondition(ec))
        raiseException(EXC_TRAP);
    return 1;
}

// syscall: unconditional system-call exception.
uint8_t spg290::SYSCALL()
{
    raiseException(EXC_SYSCALL);
    return 1;
}

// rte: return from exception -> PC = EPC (cr5).
uint8_t spg290::RTE()
{
    pc = cr[5];
    return 1;
}

// drte: debug return from exception -> PC = DEPC (cr30).
uint8_t spg290::DRTE()
{
    pc = cr[30];
    return 1;
}

// ldcx: load a coprocessor data register from memory at GPRrD + sext(imm).
// rD=bits 25:21 (base), CrA=bits 20:16, CP#=bits 9:8, imm=bits 7:0 (signed).
uint8_t spg290::LDCX()
{
    uint8_t base   = (instr >> 21) & 0x1F;
    uint8_t creg   = (instr >> 16) & 0x1F;
    uint8_t cp     = (instr >> 8) & 0x3;
    int32_t imm    = (int32_t)(int8_t)(instr & 0xFF);
    uint16_t addr  = (uint16_t)(regs[base] + (uint32_t)imm);
    copData[cp][creg] = read(addr);
    return 1;
}

// stcx: store a coprocessor data register to memory at GPRrD + sext(imm).
uint8_t spg290::STCX()
{
    uint8_t base   = (instr >> 21) & 0x1F;
    uint8_t creg   = (instr >> 16) & 0x1F;
    uint8_t cp     = (instr >> 8) & 0x3;
    int32_t imm    = (int32_t)(int8_t)(instr & 0xFF);
    uint16_t addr  = (uint16_t)(regs[base] + (uint32_t)imm);
    write(addr, copData[cp][creg]);
    return 1;
}

// ============================================================
// Un-aligned load/store combine family (lcb/lcw/lce/scb/scw/sce).
//
// The architecture defines these in terms of byte-addressed memory and an
// endian-dependent byte-lane merge through the LCR/SCR special registers
// (sr1/sr2). This emulator's memory is word-addressed (one 32-bit word per
// slot) with no byte addressing, so a bit-exact endian byte-merge is not
// representable. The model below keeps the essential dataflow and produces a
// verifiable little-endian un-aligned access:
//
//   - rA is treated as a byte address; the accessed word lives in slot rA>>2
//     and rA is post-incremented by 4 (one word) per instruction, so a
//     sequence walks consecutive slots while off = rA&3 stays constant.
//   - off = rA & 3 is the byte offset of the un-aligned word.
//   - A load reconstructs the word spanning two consecutive slots; a store
//     splits a word across two consecutive slots, preserving the surrounding
//     bytes. lcb/lcw and scb/sce therefore round-trip the same value.
// ============================================================

// Reconstruct the un-aligned little-endian word: its low (4-off) bytes come
// from the high bytes of the first word (held in LCR) and its high off bytes
// come from the low bytes of the freshly read word.
uint32_t spg290::combineLoad(uint32_t lcr, uint32_t mem, uint8_t off)
{
    if (off == 0)
        return lcr;
    return (lcr >> (8u * off)) | (mem << (8u * (4u - off)));
}

uint8_t spg290::LCB()
{
    uint8_t a_reg = (instr >> 16) & 0x1F;
    uint32_t ra   = regs[a_reg];
    sr[1] = read((uint16_t)(ra >> 2));        // LCR = Mem[aligned word]
    writeReg(a_reg, ra + 4);
    return 1;
}

uint8_t spg290::LCW()
{
    uint8_t d_reg = (instr >> 21) & 0x1F;
    uint8_t a_reg = (instr >> 16) & 0x1F;
    uint32_t ra   = regs[a_reg];
    uint8_t off   = ra & 3;
    uint32_t mem  = read((uint16_t)(ra >> 2));
    writeReg(d_reg, combineLoad(sr[1], mem, off));
    sr[1] = mem;                              // LCR = newly read word
    writeReg(a_reg, ra + 4);
    return 1;
}

uint8_t spg290::LCE()
{
    uint8_t d_reg = (instr >> 21) & 0x1F;
    uint8_t a_reg = (instr >> 16) & 0x1F;
    uint32_t ra   = regs[a_reg];
    uint8_t off   = ra & 3;
    if (off == 0)
    {
        writeReg(d_reg, sr[1]);                  // aligned: rD = LCR, LCR unchanged
    }
    else
    {
        uint32_t mem = read((uint16_t)(ra >> 2));
        writeReg(d_reg, combineLoad(sr[1], mem, off));
        sr[1] = mem;
    }
    writeReg(a_reg, ra + 4);
    return 1;
}

uint8_t spg290::SCB()
{
    uint8_t d_reg = (instr >> 21) & 0x1F;
    uint8_t a_reg = (instr >> 16) & 0x1F;
    uint32_t ra   = regs[a_reg];
    uint8_t off   = ra & 3;
    uint16_t slot = (uint16_t)(ra >> 2);
    sr[2] = regs[d_reg];                      // SCR latches the source word
    if (off == 0)
    {
        write(slot, sr[2]);
    }
    else
    {
        uint32_t lowMask = (1u << (8u * off)) - 1u;
        // Keep the slot's low off bytes; place rD's low (4-off) bytes above.
        write(slot, (read(slot) & lowMask) | (sr[2] << (8u * off)));
    }
    writeReg(a_reg, ra + 4);
    return 1;
}

uint8_t spg290::SCW()
{
    uint8_t d_reg = (instr >> 21) & 0x1F;
    uint8_t a_reg = (instr >> 16) & 0x1F;
    uint32_t ra   = regs[a_reg];
    uint8_t off   = ra & 3;
    uint16_t slot = (uint16_t)(ra >> 2);
    uint32_t rd   = regs[d_reg];
    if (off == 0)
        write(slot, rd);
    else
        write(slot, (sr[2] >> (8u * (4u - off))) | (rd << (8u * off)));
    sr[2] = rd;
    writeReg(a_reg, ra + 4);
    return 1;
}

uint8_t spg290::SCE()
{
    uint8_t a_reg = (instr >> 16) & 0x1F;
    uint32_t ra   = regs[a_reg];
    uint8_t off   = ra & 3;
    uint16_t slot = (uint16_t)(ra >> 2);
    if (off != 0)
    {
        uint32_t lowMask = (1u << (8u * off)) - 1u;
        // Keep the slot's high (4-off) bytes; place SCR's high off bytes below.
        write(slot, (read(slot) & ~lowMask) | (sr[2] >> (8u * (4u - off))));
    }
    writeReg(a_reg, ra + 4);
    return 1;
}