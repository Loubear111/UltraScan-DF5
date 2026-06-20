#pragma once

#include <vector>
#include <stdint.h>
#include <string>

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
		T = (1 << 8),	// T (true) condition flag, set by cmp{TCS}.c / cmpz{TCS}.c
	};

	// Custom Engine result registers (a.k.a {HI,LO}). mul/mad/msb/div place
	// their results here; mfcel/mfceh/mtcel/mtceh move between these and GPRs.
	uint32_t ceh = 0x00000000; // CEH (high word / remainder)
	uint32_t cel = 0x00000000; // CEL (low word / quotient)

	// Control registers (cr0..cr31: PSR, ECR, EPC, ...), special registers
	// (sr0..sr31: counter / load-combine / store-combine), and the three
	// coprocessor (CP1..CP3) data/control register files. These back the
	// mfcr/mtcr, mfsr/mtsr, and mfcx/mtcx/mfccx/mtccx move instructions.
	uint32_t cr[32] = {0};
	uint32_t sr[32] = {0};
	uint32_t copData[4][32] = {{0}};
	uint32_t copCtrl[4][32] = {{0}};

	// General-purpose registers r0..r31.
	// r0 is hardwired to 0 — writes are silently ignored (see writeReg()).
	uint32_t regs[32] = {0};

	// I don't think SPG290 has an accumulator, so we probs don't need this
	uint8_t acc = 0x00; // Accumulator register (this is where we store temp numbers for arithmetic)
	uint8_t x = 0x00; // X register
	uint8_t y = 0x00; // Y register
	uint8_t stkp = 0x00; // stack pointer (points to location on bus)

	// program counter, 32 bits
	uint32_t pc = 0x00000000;

	// status register (widened to 32 bits so the T flag at bit 8 fits)
	uint32_t status = 0x00;

	uint8_t GetFlag(FLAGS290 f);

	void clock();

private:

	Bus		*bus = nullptr;
	uint32_t read(uint16_t a);
	void	write(uint16_t a, uint32_t d);
	// Write to a GPR, ignoring writes to r0 (hardwired zero).
	void    writeReg(uint8_t r, uint32_t val);

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

	// masks out the first bit (CU bit) of instruction
	// this is used to determine if CPU flags are set or not
	uint8_t		CU_MASK		= 0x1;
	
	// masks out bits 6-1 of instr which are func6 (for "Special-Form" Instructions) 
	// see page 12 of S+Core7 programmers manual
	uint8_t		func6_MASK	= 0x7E;

	// masks out bits 20-18 of instr which are func3 (for "I-Form" Instructions) 
	// see page 12 of S+Core7 programmers manual
	uint32_t	func3_MASK	= 0x1C0000;

	// ============================================================
	// Internal instruction-encoding convention (this emulator).
	//
	// The S+core7 manual's binary "Form" diagrams are not available,
	// so this project defines its own consistent 32-bit encoding that
	// matches the existing ANDX/ANDIX/ANDRIX decoders and the encoder
	// helpers in tests/test_helpers.h.
	//
	//   bit   31     : 32-bit instruction indicator (don't-care on decode)
	//   bits  30:26  : opcode      (OPCODE_MASK)
	//   bits  25:21  : rD
	//   bits  20:16  : rA
	//   bits  14:10  : rB  (also used as the 5-bit shift amount SA)
	//   bits   6:1   : func6  (R-type sub-function, when opcode == 0)
	//   bit    0     : CU (condition-flag update bit)
	//
	//   I-type  (opcode 0x01 / 0x02): func3 = bits 20:18, imm16 = bits 16:1
	//   RIX-type(opcode 0x0C..0x0F) : rA = bits 20:16,    imm14 = bits 14:1
	//
	// imm16/imm14 deliberately start at bit 1 so they never collide with
	// the CU bit at bit 0.
	// ============================================================

	// Opcode-0 func6 values
	enum FUNC6
	{
		F6_NOP    = 0x00,
		F6_ADDX   = 0x01,
		F6_ADDCX  = 0x02,
		F6_SUBX   = 0x03,
		F6_SUBCX  = 0x04,
		F6_NEGX   = 0x05,
		F6_CMP    = 0x06,
		F6_CMPZ   = 0x07,
		F6_MVCOND = 0x08,
		F6_MTCEH  = 0x09,
		F6_REM    = 0x0A,
		F6_REMU   = 0x0B,
		F6_MAD    = 0x0C,
		F6_MADU   = 0x0D,
		F6_MSB    = 0x0E,
		F6_MSBU   = 0x0F,
		F6_MULF   = 0x15,
		F6_MADF   = 0x16,
		F6_ANDX   = 0x10,
		F6_ORX    = 0x11,
		F6_XORX   = 0x12,
		F6_NOTX   = 0x13,
		F6_BITTSTC= 0x14,
		// 0x17..0x1F: filled in with the second wave of instructions.
		F6_ROLC   = 0x17,   // rolc.c  : rotate left through carry (register)
		F6_ROLIC  = 0x18,   // rolic.c : rotate left through carry (immediate)
		F6_RORC   = 0x19,   // rorc.c  : rotate right through carry (register)
		F6_RORIC  = 0x1A,   // roric.c : rotate right through carry (immediate)
		F6_ADDS   = 0x1B,   // add.s   : add with signed saturation
		F6_SUBS   = 0x1C,   // sub.s   : subtract with signed saturation
		F6_ABSS   = 0x1D,   // abs.s   : absolute value with signed saturation
		F6_SLLS   = 0x1E,   // sll.s   : shift-left logical with signed saturation
		F6_MSBF   = 0x1F,   // msb.f   : {CEH,CEL} -= (rA*rB)<<1 (signed fractional)
		F6_SLLX   = 0x20,
		F6_SRLX   = 0x21,
		F6_SRAX   = 0x22,
		F6_ROLX   = 0x23,
		F6_RORX   = 0x24,
		F6_SLLIX  = 0x25,
		F6_SRLIX  = 0x26,
		F6_SRAIX  = 0x27,
		F6_ROLIX  = 0x28,
		F6_RORIX  = 0x29,
		F6_EXTSBX = 0x30,
		F6_EXTSHX = 0x31,
		F6_EXTZBX = 0x32,
		F6_EXTZHX = 0x33,
		F6_CLZ    = 0x34,
		F6_ABS    = 0x35,
		F6_MIN    = 0x36,
		F6_MAX    = 0x37,
		F6_BITREV = 0x38,
		F6_MUL    = 0x39,
		F6_MULU   = 0x3A,
		F6_DIV    = 0x3B,
		F6_DIVU   = 0x3C,
		F6_MFCEL  = 0x3D,
		F6_MFCEH  = 0x3E,
		F6_MTCEL  = 0x3F,
	};

	// I-type opcodes / func3 values
	enum OPCODES_ENUM
	{
		OP_RTYPE    = 0x00,
		OP_ITYPE1   = 0x01,
		OP_ITYPE2   = 0x02,
		OP_ANDRIX   = 0x0C,
		OP_ADDRIX   = 0x0D,
		OP_ORRIX    = 0x0E,
		OP_SUBRIX   = 0x0F,
		// Load / store (rD, rA, signed Imm15). Effective address is a word index.
		OP_LW       = 0x10,
		OP_SW       = 0x11,
		OP_LBU      = 0x12,
		OP_LB       = 0x13,
		OP_LHU      = 0x14,
		OP_LH       = 0x15,
		OP_SB       = 0x16,
		OP_SH       = 0x17,
		// Control flow
		OP_BCOND    = 0x18,   // BC[25:22], signed Disp[21:1]
		OP_JX       = 0x19,   // Disp24[24:1], LK bit 0
		OP_BR       = 0x1A,   // BC[25:22], rA[20:16]
		// Pre/post-index load/store family. rD[25:21], rA[20:16],
		// sub-op[15:13] (0=lw,1=sw,2=lbu,3=lb,4=lhu,5=lh,6=sb,7=sh),
		// signed Imm12[12:1], pre-index flag at bit 0 (1=pre, 0=post).
		OP_LSU_UPD  = 0x1B,
		// Half-word multiply-accumulate family. rA[20:16], rB[14:10],
		// HZFSU control field in bits [25:21] (H=high half, Z=zero start,
		// F=fractional, S=subtract, U=unsigned).
		OP_HWMAC    = 0x1C,
		OP_BCONDL   = 0x1D,   // b{cond}l : BCOND + link r3
		OP_BRL      = 0x1E,   // br{cond}l: BR + link r3
		// System / coprocessor register moves. The last available opcode is
		// shared by the whole group; the specific operation is chosen by a
		// 5-bit sub-op field in bits 14:10 (see SYS_SUBOP).
		//   rD = bits 25:21, regnum / rA = bits 20:16, CP#/{HI,LO} = bits 9:8.
		OP_SYS      = 0x1F,
	};

	// Sub-op selector for OP_SYS (bits 14:10).
	enum SYS_SUBOP
	{
		SYS_MFCR  = 0x00,
		SYS_MTCR  = 0x01,
		SYS_MFSR  = 0x02,
		SYS_MTSR  = 0x03,
		SYS_MFCX  = 0x04,
		SYS_MTCX  = 0x05,
		SYS_MFCCX = 0x06,
		SYS_MTCCX = 0x07,
		SYS_MFCEX = 0x08,
		SYS_MTCEX = 0x09,
		SYS_COPX  = 0x0A,
		SYS_TCOND = 0x0B,   // t{cond}    : T = condition (EC in bits 20:16)
		SYS_TRAP  = 0x0C,   // trap{cond} : exception if condition (EC in 20:16)
		SYS_SYSCALL = 0x0D, // syscall    : unconditional system-call exception
		SYS_RTE   = 0x0E,   // rte        : PC = EPC (cr5)
		SYS_DRTE  = 0x0F,   // drte       : PC = DEPC (cr30)
		SYS_SLEEP = 0x10,   // sleep      : no-op here
		SYS_CACHE = 0x11,   // cache      : no-op here
		SYS_PFLUSH= 0x12,   // pflush     : no-op here
		SYS_SDBBP = 0x13,   // sdbbp      : debug breakpoint -> no-op
		SYS_CEINST= 0x14,   // ceinst     : user-defined custom-engine op -> no-op
		SYS_LDCX  = 0x15,   // ldcx       : copData[CP#][CrA] = Mem[rD + imm]
		SYS_STCX  = 0x16,   // stcx       : Mem[rD + imm] = copData[CP#][CrA]
		SYS_LCB   = 0x17,   // lcb        : load-combine begin  (LCR = Mem; rA+=4)
		SYS_LCW   = 0x18,   // lcw        : load-combine word
		SYS_LCE   = 0x19,   // lce        : load-combine end
		SYS_SCB   = 0x1A,   // scb        : store-combine begin
		SYS_SCW   = 0x1B,   // scw        : store-combine word
		SYS_SCE   = 0x1C,   // sce        : store-combine end
	};

	// Exception cause codes (subset used by this emulator).
	enum EXC_CODE
	{
		EXC_SYSCALL = 7,
		EXC_TRAP    = 10,
	};

	// func3 within OP_ITYPE1
	enum FUNC3_I1
	{
		F3_ADDIX  = 0x0,
		F3_ORIX   = 0x1,
		F3_CMPI   = 0x2,
		F3_LDI    = 0x3,
		F3_ANDIX  = 0x4,
		F3_ANDISX = 0x5,
		F3_ADDISX = 0x6,
		F3_ORISX  = 0x7,
	};

	// func3 within OP_ITYPE2
	enum FUNC3_I2
	{
		F3_LDIS   = 0x0,
		F3_SUBIX  = 0x1,
		F3_SUBISX = 0x2,
	};

	// Shared helper: evaluate a 4-bit execution condition against the flags.
	bool evalCondition(uint8_t ec);

	/* OPCODES */
	uint8_t ADDCX();
	uint8_t ADDIX();
	uint8_t ADDISX();
	uint8_t ADDRIX();
	uint8_t ANDX();		// Logical AND	
	uint8_t ANDIX();	// Logical AND with Immediate
	uint8_t ANDISX();
	uint8_t ANDRIX();	// Logical AND Register with Immediate
	uint8_t BITTSTC();	
	uint8_t CEINST();	
	uint8_t ORX();

	// --- R-type arithmetic / logic ---
	uint8_t ADDX();		// Add
	uint8_t SUBX();		// Subtract
	uint8_t SUBCX();	// Subtract with carry
	uint8_t NEGX();		// Negate
	uint8_t NOTX();		// Logical NOT
	uint8_t NOP();		// No operation
	uint8_t XORX();		// Logical XOR
	uint8_t CMP();		// Compare (cmp{TCS}.c)
	uint8_t CMPZ();		// Compare with zero
	uint8_t MVCOND();	// Conditional move

	// --- Immediate forms ---
	uint8_t ORIX();		// OR with imm16 (zero-extended)
	uint8_t ORISX();	// OR with imm16 << 16
	uint8_t ORRIX();	// OR register with imm14 (zero-extended)
	uint8_t SUBIX();	// Subtract imm16 (signed)
	uint8_t SUBISX();	// Subtract imm16 << 16
	uint8_t SUBRIX();	// Subtract register with imm14 (signed)
	uint8_t CMPI();		// Compare with imm16 (signed)
	uint8_t LDI();		// Load sign-extended imm16
	uint8_t LDIS();		// Load imm16 << 16

	// --- Shifts / rotates ---
	uint8_t SLLX();		// Shift left logical (register)
	uint8_t SRLX();		// Shift right logical (register)
	uint8_t SRAX();		// Shift right arithmetic (register)
	uint8_t ROLX();		// Rotate left (register)
	uint8_t RORX();		// Rotate right (register)
	uint8_t SLLIX();	// Shift left logical (immediate)
	uint8_t SRLIX();	// Shift right logical (immediate)
	uint8_t SRAIX();	// Shift right arithmetic (immediate)
	uint8_t ROLIX();	// Rotate left (immediate)
	uint8_t RORIX();	// Rotate right (immediate)

	// --- Bit / extend / misc ---
	uint8_t EXTSBX();	// Extend sign of byte
	uint8_t EXTSHX();	// Extend sign of half-word
	uint8_t EXTZBX();	// Extend zero of byte
	uint8_t EXTZHX();	// Extend zero of half-word
	uint8_t CLZ();		// Count leading zeros
	uint8_t ABS();		// Absolute value
	uint8_t MIN();		// Minimum (signed)
	uint8_t MAX();		// Maximum (signed)
	uint8_t BITREV();	// Bit reverse with shift right logical

	// --- Custom engine: multiply / divide / moves ---
	uint8_t MUL();		// Signed multiply -> {CEH,CEL}
	uint8_t MULU();		// Unsigned multiply -> {CEH,CEL}
	uint8_t DIV();		// Signed divide -> CEL=quotient, CEH=remainder
	uint8_t DIVU();		// Unsigned divide -> CEL=quotient, CEH=remainder
	uint8_t REM();		// Signed remainder -> rD
	uint8_t REMU();		// Unsigned remainder -> rD
	uint8_t MFCEL();	// rD = CEL
	uint8_t MFCEH();	// rD = CEH
	uint8_t MTCEL();	// CEL = rA
	uint8_t MTCEH();	// CEH = rA

	// --- Custom engine: multiply-accumulate ---
	uint8_t MAD();		// {CEH,CEL} += rA * rB (signed)
	uint8_t MADU();		// {CEH,CEL} += rA * rB (unsigned)
	uint8_t MSB();		// {CEH,CEL} -= rA * rB (signed)
	uint8_t MSBU();		// {CEH,CEL} -= rA * rB (unsigned)
	uint8_t MULF();		// {CEH,CEL} = (rA * rB) << 1 (signed fractional)
	uint8_t MADF();		// {CEH,CEL} += (rA * rB) << 1 (signed fractional)

	// --- Loads / stores ---
	uint8_t LW();
	uint8_t SW();
	uint8_t LBU();
	uint8_t LB();
	uint8_t LHU();
	uint8_t LH();
	uint8_t SB();
	uint8_t SH();

	// --- Control flow ---
	uint8_t BCOND();	// Conditional relative branch
	uint8_t JX();		// Jump (and link)
	uint8_t BR();		// Conditional branch to register
	uint8_t BCONDL();	// Conditional relative branch and link
	uint8_t BRL();		// Conditional branch to register and link

	// --- Rotate through carry ---
	uint8_t ROLC();		// Rotate left through carry (register amount)
	uint8_t ROLIC();	// Rotate left through carry (immediate amount)
	uint8_t RORC();		// Rotate right through carry (register amount)
	uint8_t RORIC();	// Rotate right through carry (immediate amount)

	// --- Saturating arithmetic ---
	uint8_t ADDS();		// Add with signed saturation
	uint8_t SUBS();		// Subtract with signed saturation
	uint8_t ABSS();		// Absolute value with signed saturation
	uint8_t SLLS();		// Shift-left logical with signed saturation

	// --- Custom engine extras ---
	uint8_t MSBF();		// {CEH,CEL} -= (rA*rB)<<1 (signed fractional)
	uint8_t HWMAC();	// Half-word multiply-accumulate family (HZFSU control)

	// --- Pre/post-index load/store family ---
	uint8_t LSU_UPD();

	// ============================================================
	// 16-bit instruction set ("!"-suffixed forms).
	//
	// A word whose bit 31 is 0 is decoded as a 16-bit instruction
	// (all 32-bit encoders set bit 31). The 16-bit payload lives in
	// the low 16 bits, laid out by this emulator's own convention:
	//
	//   op16 = bits 15:12 selects the format/group:
	//     0x0 RR-ALU : func4[11:8] rD[7:4] rA[3:0]
	//     0x1 ldiu   : rD[11:8] Imm8[7:0]
	//     0x2 R-Imm  : func2[11:10] rD[9:6] Imm[5:0]   (slli/srli/addei/subei)
	//     0x3 bittst : rD[11:8] BN[7:3]
	//     0x4 xbank  : func1[11] rD[7:4] rA[3:0]        (mlfh/mhfl)
	//     0x5 mem    : func3[11:9] rD[8:5] rA[4:1]      (lw/lh/lbu/sw/sh/sb)
	//     0x6 mem-bp : func3[11:9] rD[8:5] Imm5[4:0]    (lwp/lhp/lbup/swp/shp/sbp, BP=r2)
	//     0x7 stack  : func1[11] H[10] rD[9:6] rA[5:2]  (pop/push)
	//     0x8 bcond  : cond[11:8] Disp8[7:0]
	//     0x9 brcond : cond[11:8] rA[7:4]
	//     0xA jx     : LK[11] Disp11[10:0]
	//     0xB system : func2[11:10] cond[9:6]           (nop/sdbbp/t{cond})
	//
	// 16-bit GPR fields address r0..r15; the high bank (r16..r31) is
	// reached through the H bit (stack) or the xbank moves.
	// ============================================================
	uint8_t decode16();

	// --- System / coprocessor register moves (OP_SYS dispatcher) ---
	uint8_t SYS();
	uint8_t MFCR();		// rD = cr[regnum]
	uint8_t MTCR();		// cr[regnum] = rD
	uint8_t MFSR();		// rD = sr[regnum]
	uint8_t MTSR();		// sr[regnum] = rA
	uint8_t MFCX();		// rD = copData[CP#][regnum]
	uint8_t MTCX();		// copData[CP#][regnum] = rD
	uint8_t MFCCX();	// rD = copCtrl[CP#][regnum]
	uint8_t MTCCX();	// copCtrl[CP#][regnum] = rD
	uint8_t MFCEX();	// rD/rA <- CEH/CEL per {HI,LO}
	uint8_t MTCEX();	// CEH/CEL <- rD/rA per {HI,LO}
	uint8_t COPX();		// user-defined coprocessor op (no architectural effect)

	// --- Conditional test / trap / system control ---
	uint8_t TCOND();	// t{cond}    : T = condition
	uint8_t TRAPCOND();	// trap{cond} : conditional trap exception
	uint8_t SYSCALL();	// syscall    : unconditional system-call exception
	uint8_t RTE();		// rte        : return from exception (PC = EPC)
	uint8_t DRTE();		// drte       : debug return from exception (PC = DEPC)
	uint8_t LDCX();		// ldcx       : load coprocessor data register from memory
	uint8_t STCX();		// stcx       : store coprocessor data register to memory

	// --- Un-aligned load/store combine (LCR=sr1, SCR=sr2) ---
	uint8_t LCB();		// lcb        : load-combine begin
	uint8_t LCW();		// lcw        : load-combine word
	uint8_t LCE();		// lce        : load-combine end
	uint8_t SCB();		// scb        : store-combine begin
	uint8_t SCW();		// scw        : store-combine word
	uint8_t SCE();		// sce        : store-combine end

	// Minimal exception entry: save the faulting PC into EPC (cr5), record
	// the cause in ECR (cr2), and vector to EXCPvec (cr3).
	void raiseException(uint8_t code);

	// Little-endian funnel that reconstructs an un-aligned word from two
	// consecutive memory words (low part from LCR, high part from mem).
	static uint32_t combineLoad(uint32_t lcr, uint32_t mem, uint8_t off);
};