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

uint8_t spg290::readByte(uint32_t a)
{
	return bus->read8(a);
}

void spg290::writeByte(uint32_t a, uint8_t d)
{
	bus->write8(a, d);
}

uint16_t spg290::readHalf(uint32_t a)
{
	return bus->read16(a);
}

void spg290::writeHalf(uint32_t a, uint16_t d)
{
	bus->write16(a, d);
}

uint32_t spg290::readWord(uint32_t a)
{
	return bus->read32(a);
}

void spg290::writeWord(uint32_t a, uint32_t d)
{
	bus->write32(a, d);
}

void spg290::writeReg(uint8_t r, uint32_t val)
{
	// r0 is the stack pointer, a normal writable register (not a hardwired zero).
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
		// Sleep ends when the IRQ line is asserted, whether or not it is taken.
		if (halted)
		{
			if (!irqLevel) return;
			halted = false;
		}

		// Level-sensitive IRQ, sampled between instructions.
		if (irqLevel && (cr[0] & PSR_IEC) && !irqMasked())
		{
			takeInterrupt();
			return;
		}

		// Fetch (PDF 1.1; the parallel pair follows MAME's execute_run).
		// The aligned word is classified by its two P-bits, w[31] and w[15],
		// read in the Bus's byte order so no endian-specific code is needed:
		//   0/0  two 16-bit instructions: run the halfword at pc, pc += 2
		//   0/1  invalid pair: P_EL exception, pc += 4
		//   1/0  parallel pair: low 15 bits if T = 1, else high 15 bits, pc += 4
		//   1/1  one 32-bit instruction, pc += 4
		// At pc&2 the instruction is always the 16-bit halfword at pc.
		const uint32_t w = readWord(pc & ~3u);
		const bool p31 = (w & 0x80000000u) != 0;
		const bool p15 = (w & 0x00008000u) != 0;
		const bool aligned = (pc & 2u) == 0;

		instrAddr = pc;

		if (!aligned || (!p31 && !p15))
		{
			instr = readHalf(pc);   // decode16() reads this from the low 16 bits
			instrSize = 2;
			pc += 2;
			recordTrace();
			decode16();
			instructionDone();
			return;
		}

		instrSize = 4;
		pc += 4;

		if (!p31)   // 0/1
		{
			instr = w;
			recordTrace();
			raiseException(EXC_PEL);
			return;
		}

		if (!p15)   // 1/0: both halves are 15-bit instructions, decoded as 16-bit with P = 0
		{
			instr = GetFlag(T) ? (w & 0x7FFFu) : ((w >> 16) & 0x7FFFu);
			recordTrace();
			decode16();
			instructionDone();
			return;
		}

		instr = w;
		recordTrace();

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
			case F6_NOP:        NOP();       break;
			case F6_SDBBP:    break;             // sdbbp: no debug probe modelled
			case F6_PFLUSH:   break;             // pflush: no-op
			case F6_SYSCALL:    SYSCALL();   break;
			case F6_TRAPCOND:   TRAPCOND();  break;
			case F6_BRCOND:     BR();        break;
			case F6_ADDX:       ADDX();      break;
			case F6_ADDCX:      ADDCX();     break;
			case F6_SUBX:       SUBX();      break;
			case F6_SUBCX:      SUBCX();     break;
			case F6_CMP:        CMP();       break;
			case F6_CMPZ:       CMPZ();      break;
			case F6_NEGX:       NEGX();      break;
			case F6_ANDX:       ANDX();      break;
			case F6_ORX:        ORX();       break;
			case F6_NOTX:       NOTX();      break;
			case F6_XORX:       XORX();      break;
			case F6_BITCLR:     BITCLR();    break;
			case F6_BITSET:     BITSET();    break;
			case F6_BITTST:     BITTSTC();   break;
			case F6_BITTGL:     BITTGL();    break;
			case F6_SLLX:       SLLX();      break;
			case F6_SRLX:       SRLX();      break;
			case F6_SRAX:       SRAX();      break;
			case F6_RORX:       RORX();      break;
			case F6_RORC:       RORC();      break;
			case F6_ROLX:       ROLX();      break;
			case F6_ROLC:       ROLC();      break;
			case F6_MUL:        MUL();       break;
			case F6_MULU:       MULU();      break;
			case F6_DIV:        DIV();       break;
			case F6_DIVU:       DIVU();      break;
			case F6_MFSR:       MFSR();      break;
			case F6_MTSR:       MTSR();      break;
			case F6_TCOND:      TCOND();     break;
			case F6_MVCOND:     MVCOND();    break;
			case F6_EXTSBX:     EXTSBX();    break;
			case F6_EXTSHX:     EXTSHX();    break;
			case F6_EXTZBX:     EXTZBX();    break;
			case F6_EXTZHX:     EXTZHX();    break;
			case F6_LCB:        LCB();       break;
			case F6_LCW:        LCW();       break;
			case F6_LCE:        LCE();       break;
			case F6_SCB:        SCB();       break;
			case F6_SCW:        SCW();       break;
			case F6_SCE:        SCE();       break;
			case F6_SLLIX:      SLLIX();     break;
			case F6_SRLIX:      SRLIX();     break;
			case F6_SRAIX:      SRAIX();     break;
			case F6_RORIX:      RORIX();     break;
			case F6_RORIC:      RORIC();     break;
			case F6_ROLIX:      ROLIX();     break;
			case F6_ROLIC:      ROLIC();     break;
			case F6_MFCE:  // mfcel/mfceh/mfcehl selected by rB (1/2/3)
				if (((instr & 0x7C00) >> 10) == 2) MFCEH();
				else if (((instr & 0x7C00) >> 10) == 3)
				{
					uint8_t d_reg = (instr & 0x3E00000) >> 21;
					uint8_t a_reg = (instr & 0x1F0000) >> 16;
					writeReg(d_reg, ceh);
					writeReg(a_reg, cel);
				}
				else MFCEL();
				break;
			case F6_MTCE:  // mtcel/mtceh/mtcehl selected by rB (1/2/3)
				if (((instr & 0x7C00) >> 10) == 2) MTCEH();
				else if (((instr & 0x7C00) >> 10) == 3)
				{
					uint8_t d_reg = (instr & 0x3E00000) >> 21;
					uint8_t a_reg = (instr & 0x1F0000) >> 16;
					ceh = regs[d_reg];
					cel = regs[a_reg];
				}
				else MTCEL();
				break;
			default: unimplemented("special func6"); break;   // 06 07 0E 19 26 27 32 36 39
			}
			break;
		}
		case OP_ITYPE1:
		{
			uint8_t func3 = (instr & func3_MASK) >> 18;
			switch (func3)
			{
			case F3_ADDIX:  ADDIX();  break;
			case F3_ORIX:   ORIX();   break;  // ori:  func3=5
			case F3_CMPI:   CMPI();   break;
			case F3_LDI:    LDI();    break;  // ldi:  func3=6
			case F3_ANDIX:  ANDIX();  break;  // andi: func3=4
			default: unimplemented("I-form func3"); break;
			}
			break;
		}
		case OP_ITYPE_S:  // opcode=5: shifted-immediate (addis/andis/oris/ldis)
		{
			uint8_t func3 = (instr & func3_MASK) >> 18;
			switch (func3)
			{
			case F3_ADDISX: ADDISX(); break;  // addis: func3=0
			case F3_CMPISX: CMPIS();  break;  // cmpis: func3=2
			case F3_ANDISX: ANDISX(); break;  // andis: func3=4
			case F3_ORISX:  ORISX();  break;  // oris:  func3=5
			case F3_LDISX:  LDIS();   break;  // ldis:  func3=6, confirmed via score-elf-as
			default: unimplemented("I-form (opcode 5) func3"); break;
			}
			break;
		}
		case OP_ANDRIX: ANDRIX(); break;
		case OP_CRFORM: CRFORM(); break;
		case OP_ADDRIX: ADDRIX(); break;
		case OP_ORRIX:  ORRIX();  break;
		case OP_LW:     LW();     break;
		case OP_SW:     SW();     break;
		case OP_LBU:    LBU();    break;
		case OP_LB:     LB();     break;
		case OP_LHU:    LHU();    break;
		case OP_LH:     LH();     break;
		case OP_SB:     SB();     break;
		case OP_SH:     SH();     break;
		// b{cond} and b{cond}l share one opcode; CU bit (LK) selects the link form.
		case OP_BCOND:  if (instr & CU_MASK) BCONDL(); else BCOND(); break;
		case OP_JX:     JX();     break;
		case OP_RIX_PRE:  RIX(true);  break;   // pre-index:  rA += imm12, then access [rA]
		case OP_RIX_POST: RIX(false); break;   // post-index: access [rA], then rA += imm12
		case OP_CACHE:    break;               // cache: no-op
		default: unimplemented("opcode"); break;   // 0x1C (custom engine), coprocessor forms and the rest
		}

		instructionDone();

		// Just setting cycle per instruction to 1 for all instructions for now
		// This will need to be changed later to by cycle accurate
		cycles = 1;
	}

	cycles--;
}

void spg290::reset(uint32_t resetPc)
{
	for (uint32_t& r : regs) r = 0;
	for (uint32_t& r : cr)   r = 0;
	for (uint32_t& r : sr)   r = 0;
	for (auto& bank : copData) for (uint32_t& r : bank) r = 0;
	for (auto& bank : copCtrl) for (uint32_t& r : bank) r = 0;
	ceh = cel = 0;
	status = 0;
	cycles = 0;
	instr = 0;
	instrSize = 4;
	halted = false;
	irqLevel = false;
	irqVector = 0;
	instrAddr = 0;
	stopFlag = false;
	hang = false;
	selfLoopCount = 0;
	faults.clear();
	trace.clear();
	pc = resetPc;
}

// ---- Fault diagnostics ---------------------------------------------
// One funnel for undefined and not-yet-implemented encodings. The event is
// recorded (ring of the last FAULT_EVENT_CAP) and the architectural
// reserved-instruction exception is raised, unless stop-on-unimplemented is
// on, in which case the CPU asks its driver to stop instead.
void spg290::unimplemented(const char* what)
{
    recordFault(FaultEvent::Unimplemented, what);
    if (stopOnUnimpl)
        stopFlag = true;
    else
        raiseException(EXC_RI);
}

void spg290::recordFault(FaultEvent::Kind kind, const char* what)
{
    FaultEvent e;
    e.kind  = kind;
    e.pc    = instrAddr;
    e.word  = instr;
    e.cause = cr[2];
    e.what  = what;
    faults.push_back(e);
    if (faults.size() > FAULT_EVENT_CAP)
        faults.pop_front();
}

// Called after every executed instruction. A run of instructions that each
// leave the PC where it started (b! / j to itself) is a hang: in the demos every
// exception vector leads to such a loop, so this reports the last cause.
void spg290::instructionDone()
{
    if (pc != instrAddr)
    {
        selfLoopCount = 0;
        return;
    }
    if (hangThreshold != 0 && ++selfLoopCount == hangThreshold)
    {
        hang = true;
        recordFault(FaultEvent::Hang, "branch to self");
        if (stopOnHang)
            stopFlag = true;
    }
}

void spg290::recordTrace()
{
    if (traceDepth == 0)
        return;
    trace.push_back({ instrAddr, instr, instrSize });
    if (trace.size() > traceDepth)
        trace.pop_front();
}

void spg290::setTraceDepth(size_t n)
{
    traceDepth = n;
    while (trace.size() > n)
        trace.pop_front();
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
        SetFlag(Z, d == 0);
        SetFlag(N, (d >> 31) & 1);       // N = R[31]
        SetFlag(C, carry_out);
        SetFlag(V, ((~(a ^ b) & (a ^ d)) >> 31) & 1);
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

    // Extract 16-bit signed immediate: imm[15:14] from bits 17:16, imm[13:0] from bits 14:1.
    imm = (int16_t)(((instr & 0x30000) >> 2) | ((instr & 0x7FFE) >> 1));
    uint32_t sign_extended_imm = (uint32_t)imm;

    // Read the current value of the destination register
    d_val = regs[d_reg];

    // Perform the addition
    const uint32_t before = d_val;
    const uint64_t full = (uint64_t)before + (uint64_t)sign_extended_imm;
    d_val = (uint32_t)full;

    // PDF: N = R[31], Z, C = carry out, V = signed overflow
    if (instr & CU_MASK)
    {
        SetFlag(N, (d_val >> 31) & 1);
        SetFlag(Z, d_val == 0);
        SetFlag(C, (full >> 32) & 1);
        SetFlag(V, ((~(before ^ sign_extended_imm) & (before ^ d_val)) >> 31) & 1);
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

    // Extract 16-bit signed immediate: imm[15:14] from bits 17:16, imm[13:0] from bits 14:1.
    imm16 = (int16_t)(((instr & 0x30000) >> 2) | ((instr & 0x7FFE) >> 1));

    // Read the current value of register rD
    d_val = regs[d_reg];

    // Calculate the 16-bit left-shifted immediate value
    uint32_t shifted_imm = (uint32_t)imm16 << 16;

    // Perform the addition
    const uint32_t before = d_val;
    const uint64_t full = (uint64_t)before + (uint64_t)shifted_imm;
    d_val = (uint32_t)full;

    // PDF: N = R[31], Z, C = carry out, V = signed overflow
    if (instr & CU_MASK)
    {
        SetFlag(N, (d_val >> 31) & 1);
        SetFlag(Z, d_val == 0);
        SetFlag(C, (full >> 32) & 1);
        SetFlag(V, ((~(before ^ shifted_imm) & (before ^ d_val)) >> 31) & 1);
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
    const uint64_t full = (uint64_t)a + (uint64_t)(uint32_t)imm14;
    d = (uint32_t)full;

    // PDF: N = R[31], Z, C = carry out, V = signed overflow
    if (instr & CU_MASK)
    {
        SetFlag(N, (d >> 31) & 1);
        SetFlag(Z, d == 0);
        SetFlag(C, (full >> 32) & 1);
        SetFlag(V, ((~(a ^ (uint32_t)imm14) & (a ^ d)) >> 31) & 1);
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
		SetFlag(N, (d >> 31) & 1);
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
		SetFlag(N, (d >> 31) & 1);
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

    // Extract Imm16: non-contiguous layout — imm[15:14] from bits 17:16, imm[13:0] from bits 14:1.
    imm16 = ((instr & 0x30000) >> 2) | ((instr & 0x7FFE) >> 1);

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
        SetFlag(N, (d >> 31) & 1);
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
		SetFlag(N, (d >> 31) & 1);
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
        SetFlag(N, (d >> 31) & 1);      // Set N flag based on sign bit (matches ANDX logic)
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

// t{cond} and the branches also know EC 14, "CNT > 0" (CNT is sr0). evalCondition()
// keeps EC 14 as "no operation", which is what mv{cond} and trap{cond} want.
bool spg290::testCondition(uint8_t ec)
{
    return (ec & 0xF) == 14 ? sr[0] > 0 : evalCondition(ec);
}

// A branch on EC 14 (bcnz, bcnz!, brcnz!...) is taken while CNT > 0 and counts
// CNT down when it is taken, so CNT = n - 1 gives n passes through the loop.
bool spg290::branchTaken(uint8_t ec)
{
    const bool taken = testCondition(ec);
    if (taken && (ec & 0xF) == 14)
        --sr[0];
    return taken;
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
    else if (tcs == 2) SetFlag(T, true);       // TCS 10: set T unconditionally

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
    else if (tcs == 2) SetFlag(T, true);       // TCS 10: set T unconditionally

    return 1;
}

// br{cond}[l]: special-form func6 0x04. EC = rB field (14:10), target = rA,
// LK = raw bit 0. The link (r3 = next PC) is written whether or not the
// branch is taken.
uint8_t spg290::BR()
{
    uint8_t ec    = (instr >> 10) & 0xF;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint32_t target = regs[a_reg];

    if (instr & CU_MASK)
        writeReg(3, pc);

    if (branchTaken(ec))
        pc = target;
    return 1;
}

// t{cond}: T = condition (EC in the rB field, 14:10).
uint8_t spg290::TCOND()
{
    uint8_t ec = (instr >> 10) & 0xF;
    SetFlag(T, testCondition(ec));
    return 1;
}

// bitclr / bitset / bittgl rA, BN: rA is both source and destination and BN is
// the rB field. The .c form sets N and Z from the result. (The PDF only
// documents bittst.c; the flag effect of the other three is an assumption.)
uint8_t spg290::BITCLR() { return bitOp(0); }
uint8_t spg290::BITSET() { return bitOp(1); }
uint8_t spg290::BITTGL() { return bitOp(2); }
uint8_t spg290::bitOp(int kind)
{
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t bn    = (instr & 0x7C00) >> 10;
    uint32_t mask = 1u << bn;
    uint32_t r    = regs[a_reg];
    r = (kind == 0) ? (r & ~mask) : (kind == 1) ? (r | mask) : (r ^ mask);

    if (instr & CU_MASK)
    {
        SetFlag(N, (r >> 31) & 1);
        SetFlag(Z, r == 0);
    }
    writeReg(a_reg, r);
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

// cmpis rD, imm16: compare rD with imm16 << 16 (flags as cmpi).
uint8_t spg290::CMPIS()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    uint32_t imm16 = ((instr & 0x30000) >> 2) | ((instr & 0x7FFE) >> 1);
    uint32_t a = regs[d_reg];
    uint32_t b = imm16 << 16;
    uint32_t r = a - b;

    SetFlag(N, (r >> 31) & 1);
    SetFlag(Z, r == 0);
    SetFlag(C, a >= b);
    SetFlag(V, (((a ^ b) & (a ^ r)) >> 31) & 1);
    SetFlag(T, GetFlag(Z));

    return 1;
}

uint8_t spg290::ORIX()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    uint32_t imm16 = ((instr & 0x30000) >> 2) | ((instr & 0x7FFE) >> 1);  // zero-extended
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
    uint32_t imm16 = ((instr & 0x30000) >> 2) | ((instr & 0x7FFE) >> 1);
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

uint8_t spg290::CMPI()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    int32_t  imm   = (int16_t)(((instr & 0x30000) >> 2) | ((instr & 0x7FFE) >> 1));
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
    int32_t imm   = (int16_t)(((instr & 0x30000) >> 2) | ((instr & 0x7FFE) >> 1));  // sign-extended
    writeReg(d_reg, (uint32_t)imm);
    return 1;
}

uint8_t spg290::LDIS()
{
    uint8_t  d_reg = (instr & 0x3E00000) >> 21;
    uint32_t imm16 = ((instr & 0x30000) >> 2) | ((instr & 0x7FFE) >> 1);
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
    uint8_t d_reg = (instr & 0x3E00000) >> 21;   // source GPR (rD field)
    cel = regs[d_reg];
    return 1;
}

uint8_t spg290::MTCEH()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;   // source GPR (rD field)
    ceh = regs[d_reg];
    return 1;
}

// ---- Loads / stores ------------------------------------------------
// The effective address is GPRrA + sext(Imm15), a real 32-bit byte address
// routed through the Bus; each access uses the accessor matching its own
// size instead of reading a whole word and masking.

uint8_t spg290::LW()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = instr & 0x7FFF;               // Imm15 = bits 14:0 (bit 15 is the P-bit)
    imm = (imm << 17) >> 17;                       // sign-extend 15-bit
    uint32_t addr = regs[a_reg] + (uint32_t)imm;
    writeReg(d_reg, readWord(addr));
    return 1;
}

uint8_t spg290::SW()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = instr & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint32_t addr = regs[a_reg] + (uint32_t)imm;
    writeWord(addr, regs[d_reg]);
    return 1;
}

uint8_t spg290::LBU()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = instr & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint32_t addr = regs[a_reg] + (uint32_t)imm;
    writeReg(d_reg, readByte(addr));
    return 1;
}

uint8_t spg290::LB()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = instr & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint32_t addr = regs[a_reg] + (uint32_t)imm;
    writeReg(d_reg, (uint32_t)(int32_t)(int8_t)readByte(addr));
    return 1;
}

uint8_t spg290::LHU()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = instr & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint32_t addr = regs[a_reg] + (uint32_t)imm;
    writeReg(d_reg, readHalf(addr));
    return 1;
}

uint8_t spg290::LH()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = instr & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint32_t addr = regs[a_reg] + (uint32_t)imm;
    writeReg(d_reg, (uint32_t)(int32_t)(int16_t)readHalf(addr));
    return 1;
}

uint8_t spg290::SB()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = instr & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint32_t addr = regs[a_reg] + (uint32_t)imm;
    writeByte(addr, (uint8_t)regs[d_reg]);
    return 1;
}

uint8_t spg290::SH()
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    int32_t imm   = instr & 0x7FFF;
    imm = (imm << 17) >> 17;
    uint32_t addr = regs[a_reg] + (uint32_t)imm;
    writeHalf(addr, (uint16_t)regs[d_reg]);
    return 1;
}

// ---- Control flow --------------------------------------------------
// pc is now a real byte address, already advanced past the current
// instruction by the time these execute, so PCcurrent == (pc - instrSize).
//
// BC-form bit layout (opcode 0x04, with the P-bits excluded from the
// displacement): bits25:16 = Disp[18:9], bit15 = P-bit, bit14 = reserved,
// bits13:10 = BC (condition code), bits9:1 = Disp[8:0], bit0 = CU
// (0=b{cond}, 1=b{cond}l). The two displacement halves concatenate into a
// 19-bit signed halfword count; the final byte offset is disp<<1, added to
// PCcurrent (PC = PCcurrent + SignExtend({Disp, 1'b0})).
static inline int32_t decodeBCondDisp19(uint32_t instr)
{
    uint32_t hi = (instr >> 16) & 0x3FFu;   // bits 25:16
    uint32_t lo = (instr >> 1) & 0x1FFu;    // bits 9:1
    int32_t disp = (int32_t)((hi << 9) | lo);
    return (disp << 13) >> 13;               // sign-extend 19 bits
}

uint8_t spg290::BCOND()
{
    uint8_t bc   = (instr >> 10) & 0xF;
    int32_t disp = decodeBCondDisp19(instr);

    if (branchTaken(bc))
        pc = (pc - instrSize) + (uint32_t)(disp << 1);

    return 1;
}

uint8_t spg290::JX()
{
    // Disp24 = bits 25:16 (upper 10) | bits 14:1 (lower 14); bit 15 is the P-bit.
    uint32_t disp = (((instr >> 16) & 0x3FFu) << 14) | ((instr >> 1) & 0x3FFFu);
    uint8_t  lk   = instr & 0x1;

    if (lk)
        writeReg(3, pc);     // link register r3 = address of following instruction

    uint32_t pcCurrent = pc - instrSize;
    pc = (pcCurrent & 0xFE000000u) | (disp << 1);
    return 1;
}

uint8_t spg290::BCONDL()
{
    uint8_t bc   = (instr >> 10) & 0xF;
    int32_t disp = decodeBCondDisp19(instr);

    // Link register r3 = address of the following instruction (pc already
    // advanced). The link is always written, regardless of the condition.
    writeReg(3, pc);

    if (branchTaken(bc))
        pc = (pc - instrSize) + (uint32_t)(disp << 1);

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

// ---- Half-word multiply-accumulate family --------------------------
// Control field HZFSU lives in bits [25:21]:
//   H = high half  (use bits [31:16] and target CEH; else low half -> CEL)
//   Z = zero start (accumulator base is 0; else current CE register)
//   F = fractional (product is left-shifted by 1)
//   S = subtract   (base - product; else base + product)
//   U = unsigned   (16-bit operands are zero-extended; else sign-extended)
// Saturation (.fs forms) applies when F && !Z.

// ---- Pre/post-index load/store family ------------------------------
// Effective address handling mirrors the base load/store ops, but the base
// register rA is always updated by sext(Imm12) afterwards. Pre-index uses
// (rA + imm) as the access address; post-index uses rA, then updates rA.

// ---- Pre/post-index load/store (RIX-form, opcodes 3 and 7) ---------
// Imm12 = raw 14:3 (signed), func3 = raw 2:0: 0 lw, 1 lh, 2 lhu, 3 lb,
// 4 sw, 5 sh, 6 lbu, 7 sb. Pre-index updates rA first and accesses [rA];
// post-index accesses [rA] and then updates it. A load into rA itself
// therefore keeps the loaded value for pre-index and the updated address
// for post-index.
uint8_t spg290::RIX(bool pre)
{
    uint8_t d_reg = (instr & 0x3E00000) >> 21;
    uint8_t a_reg = (instr & 0x1F0000) >> 16;
    uint8_t func3 = instr & 0x7;
    int32_t imm   = (int32_t)((instr >> 3) & 0xFFF);
    imm = (imm << 20) >> 20;                      // sign-extend 12 bits

    uint32_t base = regs[a_reg];
    uint32_t next = base + (uint32_t)imm;
    uint32_t addr = pre ? next : base;
    uint32_t src  = regs[d_reg];                  // store data is read before any writeback

    if (pre) writeReg(a_reg, next);

    switch (func3)
    {
    case 0: writeReg(d_reg, readWord(addr)); break;                                   // lw
    case 1: writeReg(d_reg, (uint32_t)(int32_t)(int16_t)readHalf(addr)); break;       // lh
    case 2: writeReg(d_reg, readHalf(addr)); break;                                   // lhu
    case 3: writeReg(d_reg, (uint32_t)(int32_t)(int8_t)readByte(addr)); break;        // lb
    case 4: writeWord(addr, src); break;                                              // sw
    case 5: writeHalf(addr, (uint16_t)src); break;                                    // sh
    case 6: writeReg(d_reg, readByte(addr)); break;                                   // lbu
    case 7: writeByte(addr, (uint8_t)src); break;                                     // sb
    }

    if (!pre) writeReg(a_reg, next);
    return 1;
}

// ====================================================================
// 16-bit instruction decoder (PDF 2.2). The argument is the 15-bit payload
// in instr[14:0]; bit 15 is the P-bit. OP = bits 14:12. GPR fields address
// r0..r15; the high bank (r16..r31) is reached through mlfh!/mhfl! and the
// H bit of push!/pop!. OP 1 and undefined function codes raise RI.
//
// The "!" ALU forms are CU = 1: the PDF lists their flags as undefined
// (clobbered); they update the flags the way the 32-bit .c forms do.
// ====================================================================
uint8_t spg290::decode16()
{
    uint16_t w   = (uint16_t)(instr & 0xFFFF);
    uint8_t  op  = (w >> 12) & 0xF;

    // The "!" ALU forms are CU = 1: they update the condition flags the way the
    // 32-bit .c forms do (the PDF lists the flags as undefined/clobbered).
    auto setNZ = [this](uint32_t r) { SetFlag(N, (r >> 31) & 1); SetFlag(Z, r == 0); };
    auto add16 = [this, &setNZ](uint8_t d, uint32_t x, uint32_t y, uint32_t cin) {
        uint64_t full = (uint64_t)x + y + cin;
        uint32_t r = (uint32_t)full;
        setNZ(r);
        SetFlag(C, (full >> 32) & 1);
        SetFlag(V, ((~(x ^ y) & (x ^ r)) >> 31) & 1);
        writeReg(d, r);
    };
    // r = x - y; write=false is cmp! (flags only).
    auto sub16 = [this, &setNZ](uint8_t d, uint32_t x, uint32_t y, bool write) {
        uint32_t r = x - y;
        setNZ(r);
        SetFlag(C, x >= y);
        SetFlag(V, (((x ^ y) & (x ^ r)) >> 31) & 1);
        if (write) writeReg(d, r);
    };
    auto logic16 = [this, &setNZ](uint8_t d, uint32_t r, bool write) {
        setNZ(r);
        if (write) writeReg(d, r);
    };
    // kind: 0 sll, 1 srl, 2 sra. Amount = low 5 bits of the second operand.
    auto shift16 = [this, &setNZ](uint8_t d, uint32_t x, uint32_t amt, int kind) {
        uint8_t sa = amt & 0x1F;
        uint32_t r = x;
        bool c = false;
        if (sa) {
            if (kind == 0)      { r = x << sa;                 c = (x >> (32 - sa)) & 1; }
            else if (kind == 1) { r = x >> sa;                 c = (x >> (sa - 1)) & 1; }
            else                { r = (uint32_t)((int32_t)x >> sa); c = (x >> (sa - 1)) & 1; }
        }
        setNZ(r);
        SetFlag(C, c);
        writeReg(d, r);
    };

    switch (op)
    {
    case 0x0: // OP 0: rD[11:8] rA[7:4] func4[3:0]  (PDF 2.2)
    {
        uint8_t d    = (w >> 8) & 0xF;
        uint8_t a    = (w >> 4) & 0xF;
        uint8_t func = w & 0xF;
        uint32_t vd  = regs[d];
        uint32_t va  = regs[a];
        switch (func)
        {
        case 0x0: break;                                       // nop!
        case 0x1: writeReg(d, regs[a + 16]); break;            // mlfh! rD = r(16+rA)
        case 0x2: writeReg((uint8_t)(d + 16), va); break;      // mhfl! r(16+rD) = rA
        case 0x3: writeReg(d, va); break;                      // mv!
        case 0x4:                                              // br{cond}! (EC = rD field)
            if (branchTaken(d)) pc = va;
            break;
        case 0x5: SetFlag(T, testCondition(d)); break;         // t{cond}! (EC = rD field)
        case 0x8: shift16(d, vd, va, 0); break;                // sll!
        case 0x9: { add16(d, vd, va, GetFlag(C)); break; }     // addc!
        case 0xA: shift16(d, vd, va, 1); break;                // srl!
        case 0xB: shift16(d, vd, va, 2); break;                // sra!
        case 0xC:                                              // brl{cond}!: r3 always written
            writeReg(3, pc);
            if (branchTaken(d)) pc = va;
            break;
        default: unimplemented("16-bit OP 0 func4"); break;
        }
        break;
    }
    case 0x2: // OP 2: rD[11:8] rA[7:4] func4[3:0]  (PDF 2.2)
    {
        uint8_t d    = (w >> 8) & 0xF;
        uint8_t a    = (w >> 4) & 0xF;
        uint8_t func = w & 0xF;
        uint32_t vd  = regs[d];
        uint32_t va  = regs[a];
        switch (func)
        {
        case 0x0: add16(d, vd, va, 0); break;                  // add!
        case 0x1: sub16(d, vd, va, true); break;               // sub!
        case 0x2: sub16(d, 0, va, true); break;                // neg!
        case 0x3: sub16(d, vd, va, false); break;              // cmp!
        case 0x4: logic16(d, vd & va, true); break;            // and!
        case 0x5: logic16(d, vd | va, true); break;            // or!
        case 0x6: logic16(d, ~va, true); break;                // not!
        case 0x7: logic16(d, vd ^ va, true); break;            // xor!
        case 0x8: writeReg(d, readWord(va)); break;            // lw!
        case 0x9: writeReg(d, (uint32_t)(int32_t)(int16_t)readHalf(va)); break; // lh!
        case 0xB: writeReg(d, readByte(va)); break;            // lbu!
        case 0xC: writeWord(va, vd); break;                    // sw!
        case 0xD: writeHalf(va, (uint16_t)vd); break;          // sh!
        case 0xF: writeByte(va, (uint8_t)vd); break;           // sb!
        case 0xA: case 0xE:                                    // pop! / push!: rDgh[11:8]+H[7], rAg0[6:4]
        {
            uint8_t dg   = (uint8_t)(d | ((w >> 7) & 1) << 4);
            uint8_t base = (w >> 4) & 0x7;
            uint32_t b   = regs[base];
            if (func == 0xA) { writeReg(dg, readWord(b)); writeReg(base, b + 4); }
            else             { writeWord(b - 4, regs[dg]); writeReg(base, b - 4); }
            break;
        }
        default: break;
        }
        break;
    }
    case 0x3: // OP 3: j!/jl!  Disp11[11:1] LK[0]
    {
        uint32_t pcCurrent = pc - instrSize;
        if (w & 1) writeReg(3, pcCurrent + 2);
        pc = (pcCurrent & 0xFFFFF000u) | (uint32_t)(w & 0xFFE);
        break;
    }
    case 0x4: // OP 4: b{cond}!  EC[11:8] Disp8[7:0]
    {
        int32_t disp = (int8_t)(w & 0xFF);
        if (branchTaken((w >> 8) & 0xF))
            pc = (pc - instrSize) + (uint32_t)(disp << 1);
        break;
    }
    case 0x5: // OP 5: ldiu!  rD[11:8] Imm8[7:0]
        writeReg((w >> 8) & 0xF, w & 0xFF);
        break;
    case 0x6: // OP 6: rD[11:8] Imm5[7:3] func3[2:0]
    {
        uint8_t d    = (w >> 8) & 0xF;
        uint8_t imm5 = (w >> 3) & 0x1F;
        uint32_t vd  = regs[d];
        switch (w & 0x7)
        {
        case 0: // addei! / subei!: Imm5[4] selects subtract, value = 1 << Imm5[3:0]
            if (imm5 & 0x10) sub16(d, vd, 1u << (imm5 & 0xF), true);
            else             add16(d, vd, 1u << (imm5 & 0xF), 0);
            break;
        case 1: shift16(d, vd, imm5, 0); break;                          // slli!
        case 2: break;                                                    // sdbbp! (no debug probe modelled)
        case 3: shift16(d, vd, imm5, 1); break;                          // srli!
        case 4: writeReg(d, vd & ~(1u << imm5)); break;                  // bitclr!
        case 5: writeReg(d, vd | (1u << imm5)); break;                   // bitset!
        case 6: SetFlag(N, (vd >> 31) & 1); SetFlag(Z, ((vd >> imm5) & 1) == 0); break; // bittst!
        case 7: writeReg(d, vd ^ (1u << imm5)); break;                   // bittgl!
        }
        break;
    }
    case 0x7: // OP 7: BP (r2) relative  rD[11:8] Imm5[7:3] func3[2:0]
    {
        uint8_t d    = (w >> 8) & 0xF;
        uint8_t imm5 = (w >> 3) & 0x1F;
        uint32_t bp  = regs[2];
        switch (w & 0x7)
        {
        case 0: writeReg(d, readWord(bp + (imm5 << 2))); break;                                     // lwp!
        case 1: writeReg(d, (uint32_t)(int32_t)(int16_t)readHalf(bp + (imm5 << 1))); break;          // lhp!
        case 3: writeReg(d, readByte(bp + imm5)); break;                                            // lbup!
        case 4: writeWord(bp + (imm5 << 2), regs[d]); break;                                        // swp!
        case 5: writeHalf(bp + (imm5 << 1), (uint16_t)regs[d]); break;                              // shp!
        case 7: writeByte(bp + imm5, (uint8_t)regs[d]); break;                                      // sbp!
        default: unimplemented("16-bit OP 7 func3"); break;
        }
        break;
    }
    default: unimplemented("16-bit OP 1"); break;   // OP 1 is undefined
    }

    return 1;
}

uint8_t spg290::MFCR()
{
    uint8_t d_reg  = (instr & 0x3E00000) >> 21;
    uint8_t regnum = (instr & 0x1F0000) >> 16;
    writeReg(d_reg, regnum == 1 ? status : cr[regnum]);
    return 1;
}

uint8_t spg290::MTCR()
{
    uint8_t d_reg  = (instr & 0x3E00000) >> 21;
    uint8_t regnum = (instr & 0x1F0000) >> 16;
    if (regnum == 1) status = regs[d_reg];
    else             cr[regnum] = regs[d_reg];
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
    // Confirmed via score-elf-as: the source GPR is in the rA field (20:16)
    // and the special-register number is in the rD field (25:21) -- the
    // opposite role split from mfsr's rD=dest/rA=regnum.
    uint8_t regnum = (instr & 0x3E00000) >> 21;
    uint8_t a_reg  = (instr & 0x1F0000) >> 16;
    sr[regnum] = regs[a_reg];
    return 1;
}

// Vector addresses from EXCPvec (cr3) [MAME]: base = cr3[31:16]; the general
// vector is base+0x200 and IRQ n is at general + (n << 2), or (n << 4) when cr3
// bit 0 is set.
uint32_t spg290::generalVectorAddr() const
{
    return (cr[3] & 0xFFFF0000u) + 0x200u;
}

uint32_t spg290::irqVectorAddr(uint8_t vector) const
{
    return generalVectorAddr() + ((uint32_t)vector << ((cr[3] & 1u) ? 4 : 2));
}

// Shift the current PSR pair into the saved pair, enter kernel mode with
// interrupts disabled, save the condition flags and EPC, then jump.
void spg290::enterException(uint8_t code, uint32_t epc, uint32_t target)
{
    uint32_t psr = cr[0];
    uint32_t next = psr & ~(PSR_UMS | PSR_IES | PSR_UMC | PSR_IEC);
    if (psr & PSR_UMC) next |= PSR_UMS;
    if (psr & PSR_IEC) next |= PSR_IES;
    cr[0] = next;

    status = (status & ~0x3E0u) | ((status & 0x1Fu) << 5);   // cr1[9:5] = cr1[4:0]
    cr[2] = (cr[2] & ~ECR_CODE_MASK) | (code & ECR_CODE_MASK);
    cr[5] = epc;
    pc = target;
    halted = false;
}

// Synchronous exceptions report the faulting instruction, which sits at
// pc-instrSize (pc has already advanced past it).
void spg290::raiseException(uint8_t code)
{
    enterException(code, pc - instrSize, generalVectorAddr());
}

// Hardware IRQs are taken between instructions: EPC is the next instruction.
void spg290::takeInterrupt()
{
    uint8_t vector = irqVector;
    enterException(EXC_INT, pc, irqVectorAddr(vector));
    cr[2] = (cr[2] & ~ECR_VECTOR_MASK) | ((uint32_t)vector << ECR_VECTOR_SHIFT);
    if (onIrqTaken)
        onIrqTaken(vector);
}

// trap{cond}: raise a trap exception when the condition holds, otherwise it is
// a no-op. EC=14 is defined as "no operation" and evalCondition() returns false
// for it, which matches that behaviour. EC lives in the rB field (confirmed via
// score-elf-as); rA carries an unused software parameter (Imm5).
uint8_t spg290::TRAPCOND()
{
    uint8_t ec = (instr & 0x7C00) >> 10;
    if (evalCondition(ec & 0xF))
        raiseException(EXC_TRAP);
    return 1;
}

// syscall: unconditional system-call exception.
uint8_t spg290::SYSCALL()
{
    raiseException(EXC_SYSCALL);
    return 1;
}

// rte: restore the current PSR pair and condition flags from the saved ones
// and return to EPC (bit 0 is masked off).
uint8_t spg290::RTE()
{
    uint32_t psr = cr[0] & ~(PSR_UMC | PSR_IEC);
    if (cr[0] & PSR_UMS) psr |= PSR_UMC;
    if (cr[0] & PSR_IES) psr |= PSR_IEC;
    cr[0] = psr;
    status = (status & ~0x1Fu) | ((status >> 5) & 0x1Fu);   // cr1[4:0] = cr1[9:5]
    pc = cr[5] & ~1u;
    return 1;
}

// drte: debug return from exception -> PC = DEPC (cr30).
uint8_t spg290::DRTE()
{
    pc = cr[30];
    return 1;
}

// sleep: stop fetching until the IRQ line is asserted (see clock()).
uint8_t spg290::SLEEP()
{
    halted = true;
    return 1;
}

// CR-form (opcode 0x06): func6 selects the instruction; mfcr/mtcr share
// func6 0 and are told apart by the CU bit (1 = mfcr, 0 = mtcr).
uint8_t spg290::CRFORM()
{
    switch ((instr & func6_MASK) >> 1)
    {
    case CRF6_MOVCR: return (instr & CU_MASK) ? MFCR() : MTCR();
    case CRF6_RTE:   return RTE();
    case CRF6_DRTE:  return DRTE();
    case CRF6_SLEEP: return SLEEP();
    default:         return 1;
    }
}

// ============================================================
// Un-aligned load/store combine family (lcb/lcw/lce/scb/scw/sce).
//
// rA is a real byte address; the word-aligned slot it falls in is
// (rA & ~3) and off = rA & 3 is the byte offset within that word. rA is
// post-incremented by 4 per instruction, so a sequence walks consecutive
// aligned words while off stays constant. A load reconstructs the
// unaligned word spanning two consecutive aligned words; a store splits a
// word across two consecutive aligned words, preserving the surrounding
// bytes. lcb/lcw and scb/sce round-trip the same value (little-endian
// byte-merge, matching the existing combineLoad() model).
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
    sr[1] = readWord(ra & ~3u);                // LCR = Mem[aligned word]
    writeReg(a_reg, ra + 4);
    return 1;
}

uint8_t spg290::LCW()
{
    uint8_t d_reg = (instr >> 21) & 0x1F;
    uint8_t a_reg = (instr >> 16) & 0x1F;
    uint32_t ra   = regs[a_reg];
    uint8_t off   = ra & 3;
    uint32_t mem  = readWord(ra & ~3u);
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
        uint32_t mem = readWord(ra & ~3u);
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
    uint32_t slot = ra & ~3u;
    sr[2] = regs[d_reg];                      // SCR latches the source word
    if (off == 0)
    {
        writeWord(slot, sr[2]);
    }
    else
    {
        uint32_t lowMask = (1u << (8u * off)) - 1u;
        // Keep the slot's low off bytes; place rD's low (4-off) bytes above.
        writeWord(slot, (readWord(slot) & lowMask) | (sr[2] << (8u * off)));
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
    uint32_t slot = ra & ~3u;
    uint32_t rd   = regs[d_reg];
    if (off == 0)
        writeWord(slot, rd);
    else
        writeWord(slot, (sr[2] >> (8u * (4u - off))) | (rd << (8u * off)));
    sr[2] = rd;
    writeReg(a_reg, ra + 4);
    return 1;
}

uint8_t spg290::SCE()
{
    uint8_t a_reg = (instr >> 16) & 0x1F;
    uint32_t ra   = regs[a_reg];
    uint8_t off   = ra & 3;
    uint32_t slot = ra & ~3u;
    if (off != 0)
    {
        uint32_t lowMask = (1u << (8u * off)) - 1u;
        // Keep the slot's high (4-off) bytes; place SCR's high off bytes below.
        writeWord(slot, (readWord(slot) & ~lowMask) | (sr[2] >> (8u * (4u - off))));
    }
    writeReg(a_reg, ra + 4);
    return 1;
}