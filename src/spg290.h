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
		// Emulator-internal condition flags (the real Condition register, cr1,
		// has unknown bit positions; mfcr/mtcr cr1 read/write this word).
		// Interrupt enable lives in PSR (cr0), not here.
		C = (1 << 0),	// Carry Bit
		Z = (1 << 1),	// Zero
		V = (1 << 6),	// Overflow
		N = (1 << 7),	// Negative
		T = (1 << 8),	// T (true) condition flag, set by cmp{TCS}.c / cmpz{TCS}.c
	};

	// Processor Status Register (cr0). IEc is confirmed (Linux score port);
	// UMs is confirmed as bit 3, UMc/IEs positions follow the rte formula
	// PSR.{UMs,IEs,UMc,IEc} read high-to-low.
	enum PSR_BITS : uint32_t
	{
		PSR_IEC = 1u << 0,	// interrupts enabled
		PSR_UMC = 1u << 1,	// user mode (current)
		PSR_IES = 1u << 2,	// saved IEc
		PSR_UMS = 1u << 3,	// saved UMc
	};

	// Exception Cause Register (cr2): Exc_code in 4:0, vectored IRQ number
	// (IP[7:2]) in 23:18.
	static constexpr uint32_t ECR_CODE_MASK     = 0x1Fu;
	static constexpr uint32_t ECR_VECTOR_SHIFT  = 18;
	static constexpr uint32_t ECR_VECTOR_MASK   = 0x3Fu << ECR_VECTOR_SHIFT;

	// Drive the core's single IRQ input. `vector` is the controller-supplied
	// vector number (0..63) and is sampled when the interrupt is taken.
	void setIRQ(bool level, uint8_t vector) { irqLevel = level; irqVector = vector & 0x3F; }
	bool isHalted() const { return halted; }

	// Entry addresses derived from EXCPvec (cr3): bit 0 selects 16-byte (1)
	// or 4-byte (0) vector spacing.
	uint32_t generalVectorAddr() const;
	uint32_t irqVectorAddr(uint8_t vector) const;

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
	// r0 is the stack pointer, a normal writable register.
	// r1 = assembler temp, r2 = frame/base pointer, r3 = link register.
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

	// What the last fetch produced: the instruction word (a 16-bit or 15-bit
	// payload sits in the low bits) and its size in bytes (2, or 4 for a 32-bit
	// instruction, a parallel pair or an invalid word). Read-only, for tests.
	uint32_t lastInstr() const     { return instr; }
	uint8_t  lastInstrSize() const { return instrSize; }

private:

	Bus		*bus = nullptr;
	// Byte-addressed memory access, routed through the Bus (RAM, PPU, ...).
	// LW/SW-style word ops use readWord/writeWord; byte/half loads and
	// stores use the matching narrower accessor instead of reading a whole
	// word and masking, since the address is no longer assumed word-aligned.
	uint8_t  readByte(uint32_t a);
	void     writeByte(uint32_t a, uint8_t d);
	uint16_t readHalf(uint32_t a);
	void     writeHalf(uint32_t a, uint16_t d);
	uint32_t readWord(uint32_t a);
	void     writeWord(uint32_t a, uint32_t d);
	// Write to a GPR (r0 is the stack pointer and is writable).
	void    writeReg(uint8_t r, uint32_t val);

	void	SetFlag(FLAGS290 f, bool v);

	uint32_t  instr = 0x00;    // Is the instruction byte
	uint8_t  cycles = 0;	   // Counts how many cycles the instruction has remaining
	// Size in bytes of the instruction currently executing (2 or 4), set by
	// clock() during fetch. Used to recover PCcurrent (= pc - instrSize) for
	// PC-relative branches and exception EPC capture, now that pc is a real
	// byte address rather than a word index.
	uint8_t  instrSize = 4;

	// IRQ input (set via setIRQ), and sleep state.
	bool     irqLevel  = false;
	uint8_t  irqVector = 0;
	bool     halted    = false;
	// Condition flags saved on exception entry and restored by rte.
	uint32_t condSaved = 0;

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
	// 32-bit instruction fields, as fetched (raw word, both P-bits set):
	//   bit   31, 15 : P-bits (both 1 for a 32-bit instruction)
	//   bits  30:26  : opcode      (OPCODE_MASK)
	//   bits  25:21  : rD
	//   bits  20:16  : rA
	//   bits  14:10  : rB  (also the shift amount, bit number or EC)
	//   bits   6:1   : func6  (Special-form, opcode 0)
	//   bit    0     : CU (condition-flag update bit), LK for br/b/j
	// Immediates skip the P-bit at 15: I-form Imm16 = raw 17:16 | 14:1,
	// RI-form Imm14 = raw 14:1, RIX-form Imm12 = raw 14:3 with func3 = raw 2:0,
	// LS-form Imm15 = raw 14:0. See plan section 2.3 for the opcode table.
	// ============================================================

	// Special-form (opcode 0) func6, raw bits 6:1 (plan 2.4, S+core7 PDF and MAME).
	// Undefined values (06 07 0E 19 26 27 32 36 39) raise RI.
	enum FUNC6
	{
		F6_NOP      = 0x00,
		F6_SYSCALL  = 0x01,
		F6_TRAPCOND = 0x02,  // trap{cond}: EC = rB
		F6_SDBBP    = 0x03,
		F6_BRCOND   = 0x04,  // br{cond}[l]: EC = rB, target = rA, LK = raw bit 0
		F6_PFLUSH   = 0x05,
		F6_ADDX     = 0x08,
		F6_ADDCX    = 0x09,
		F6_SUBX     = 0x0A,
		F6_SUBCX    = 0x0B,
		F6_CMP      = 0x0C,  // cmp{TCS}.c: TCS = rD & 3
		F6_CMPZ     = 0x0D,  // cmpz{TCS}.c
		F6_NEGX     = 0x0F,
		F6_ANDX     = 0x10,
		F6_ORX      = 0x11,
		F6_NOTX     = 0x12,
		F6_XORX     = 0x13,
		F6_BITCLR   = 0x14,
		F6_BITSET   = 0x15,
		F6_BITTST   = 0x16,
		F6_BITTGL   = 0x17,
		F6_SLLX     = 0x18,
		F6_SRLX     = 0x1A,
		F6_SRAX     = 0x1B,
		F6_RORX     = 0x1C,
		F6_RORC     = 0x1D,
		F6_ROLX     = 0x1E,
		F6_ROLC     = 0x1F,
		F6_MUL      = 0x20,
		F6_MULU     = 0x21,
		F6_DIV      = 0x22,
		F6_DIVU     = 0x23,
		F6_MFCE     = 0x24,  // mfcel/mfceh/mfcehl: rB = 1/2/3
		F6_MTCE     = 0x25,  // mtcel/mtceh/mtcehl: rB = 1/2/3
		F6_MFSR     = 0x28,  // rD = dest, rA = srn
		F6_MTSR     = 0x29,  // rA = source, rD = srn (unverified, see plan)
		F6_TCOND    = 0x2A,  // t{cond}: EC = rB
		F6_MVCOND   = 0x2B,  // mv{cond}: EC = rB; EC 14 = no-op
		F6_EXTSBX   = 0x2C,
		F6_EXTSHX   = 0x2D,
		F6_EXTZBX   = 0x2E,
		F6_EXTZHX   = 0x2F,
		F6_LCB      = 0x30,
		F6_LCW      = 0x31,
		F6_LCE      = 0x33,
		F6_SCB      = 0x34,
		F6_SCW      = 0x35,
		F6_SCE      = 0x37,
		F6_SLLIX    = 0x38,
		F6_SRLIX    = 0x3A,
		F6_SRAIX    = 0x3B,
		F6_RORIX    = 0x3C,
		F6_RORIC    = 0x3D,
		F6_ROLIX    = 0x3E,
		F6_ROLIC    = 0x3F,
	};

	// Opcode field (bits 30:26), plan 2.3. Undefined opcodes raise RI.
	enum OPCODES_ENUM
	{
		OP_RTYPE    = 0x00,
		OP_ITYPE1   = 0x01,  // addi/ori/ldi/andi/cmpi (func3 selects)
		OP_JX       = 0x02,  // j/jl: absolute jump (was 0x19)
		OP_ITYPE_S  = 0x05,  // addis/andis/oris: shifted-immediate class
		// CR-form: mfcr/mtcr/rte/drte/sleep (confirmed via score-elf-as).
		OP_CRFORM   = 0x06,
		OP_ANDRIX   = 0x0C,  // andri: rD=rA & imm14   confirmed
		OP_ADDRIX   = 0x08,  // addri: rD=rA + imm14   (was 0x0D)
		OP_ORRIX    = 0x0D,  // orri:  rD=rA | imm14   (was 0x0E)
		// Load / store opcodes — all confirmed from score-elf-as:
		OP_LW       = 0x10,  // lw   confirmed
		OP_LH       = 0x11,  // lh   (was OP_SW=0x11)
		OP_LHU      = 0x12,  // lhu  (was OP_LBU=0x12)
		OP_LB       = 0x13,  // lb   confirmed
		OP_SW       = 0x14,  // sw   (was OP_LHU=0x14)
		OP_SH       = 0x15,  // sh   (was OP_LH=0x15)
		OP_LBU      = 0x16,  // lbu  (was OP_SB=0x16)
		OP_SB       = 0x17,  // sb   (was OP_SH=0x17)
		// Control flow. b{cond} and b{cond}l share ONE real opcode (confirmed via
		// score-elf-as); the link variant is selected by the CU bit (bit 0), same
		// as every other CU-suffixed instruction -- there is no separate OP_BCONDL.
		OP_BCOND    = 0x04,   // BC[13:10], split Disp[18:9]|Disp[8:0] (see BCOND())
		// Pre/post-index load/store (RIX-form): Imm12 = raw 14:3, func3 = raw 2:0.
		OP_RIX_PRE  = 0x03,
		OP_RIX_POST = 0x07,
		OP_CACHE    = 0x18,   // cache: no-op
	};

	// Exception cause codes (ECR.Exc_code) from the Sunplus Linux score port.
	enum EXC_CODE
	{
		EXC_NMI       = 1,
		EXC_ADEL_INSN = 2,
		EXC_IBE       = 5,
		EXC_PEL       = 6,
		EXC_SYSCALL   = 7,
		EXC_CCU       = 8,
		EXC_RI        = 9,
		EXC_TRAP      = 10,
		EXC_ADEL_DATA = 11,
		EXC_ADES_DATA = 12,
		EXC_CEE       = 16,
		EXC_CPE       = 17,
		EXC_DBE       = 18,
	};

	// func6 within OP_CRFORM (bit 0 / CU distinguishes mfcr=1 from mtcr=0).
	enum CRFORM_F6
	{
		CRF6_MOVCR = 0x00,
		CRF6_RTE   = 0x02,
		CRF6_DRTE  = 0x12,
		CRF6_SLEEP = 0x22,
	};

	// func3 within OP_ITYPE1
	enum FUNC3_I1
	{
		F3_ADDIX  = 0x0,  // addi  confirmed
		F3_ORIX   = 0x5,  // ori   (was 0x1)
		F3_CMPI   = 0x2,  // cmpi  (PDF/MAME: opcode 1, func3 2)
		F3_LDI    = 0x6,  // ldi   (was 0x3)
		F3_ANDIX  = 0x4,  // andi  confirmed
	};

	// func3 within OP_ITYPE_S (opcode 0x05: shifted-immediate class)
	// Confirmed from score-elf-as: addis=0, andis=4, oris=5
	enum FUNC3_IS
	{
		F3_CMPISX = 0x2,  // cmpis (Imm16 << 16)
		F3_ADDISX = 0x0,  // addis  (was F3_ADDISX=6 under OP_ITYPE1)
		F3_ANDISX = 0x4,  // andis  (was F3_ANDISX=5 under OP_ITYPE1)
		F3_ORISX  = 0x5,  // oris   (was F3_ORISX=7 under OP_ITYPE1)
		F3_LDISX  = 0x6,  // ldis   (func3 6, parallels ldi under OP_ITYPE1)
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
	uint8_t BITCLR();
	uint8_t BITSET();
	uint8_t BITTGL();
	uint8_t bitOp(int kind);   // 0 clear, 1 set, 2 toggle
	uint8_t BR();		// br{cond}[l]: conditional branch to register
	uint8_t TCOND();	// t{cond}	
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
	uint8_t CMPIS();	// Compare with imm16 << 16
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

	// --- Custom engine: multiply / divide / moves ---
	uint8_t MUL();		// Signed multiply -> {CEH,CEL}
	uint8_t MULU();		// Unsigned multiply -> {CEH,CEL}
	uint8_t DIV();		// Signed divide -> CEL=quotient, CEH=remainder
	uint8_t DIVU();		// Unsigned divide -> CEL=quotient, CEH=remainder
	uint8_t MFCEL();	// rD = CEL
	uint8_t MFCEH();	// rD = CEH
	uint8_t MTCEL();	// CEL = rA
	uint8_t MTCEH();	// CEH = rA

	// --- Custom engine: multiply-accumulate ---

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
	uint8_t BCONDL();	// Conditional relative branch and link

	// --- Rotate through carry ---
	uint8_t ROLC();		// Rotate left through carry (register amount)
	uint8_t ROLIC();	// Rotate left through carry (immediate amount)
	uint8_t RORC();		// Rotate right through carry (register amount)
	uint8_t RORIC();	// Rotate right through carry (immediate amount)

	// --- Saturating arithmetic ---

	// --- Custom engine extras ---

	// --- Pre/post-index load/store family (RIX-form) ---
	uint8_t RIX(bool pre);

	// ============================================================
	// 16-bit instruction set ("!"-suffixed forms), S+core7 PDF 2.2.
	//
	// Called with the 15-bit payload in instr[14:0] (P-bit 15 = 0). OP = bits 14:12:
	//   0  rD[11:8] rA[7:4] func4[3:0]  nop! mlfh! mhfl! mv! br{c}! t{c}! sll! addc! srl! sra! brl{c}!
	//   1  undefined (RI)
	//   2  rD[11:8] rA[7:4] func4[3:0]  add! sub! neg! cmp! and! or! not! xor! lw! lh! pop! lbu! sw! sh! push! sb!
	//   3  Disp11[11:1] LK[0]           j! / jl!
	//   4  EC[11:8] Disp8[7:0]          b{cond}!
	//   5  rD[11:8] Imm8[7:0]           ldiu!
	//   6  rD[11:8] Imm5[7:3] func3     addei!/subei! slli! sdbbp! srli! bitclr! bitset! bittst! bittgl!
	//   7  rD[11:8] Imm5[7:3] func3     BP (r2) relative: lwp! lhp! lbup! swp! shp! sbp!
	//
	// 16-bit GPR fields address r0..r15; push!/pop! use rDgh (H = bit 7 selects
	// r16..r31) and an r0..r7 base in bits 6:4.
	uint8_t decode16();

	// --- Control / special register moves ---
	uint8_t MFCR();		// rD = cr[regnum]
	uint8_t MTCR();		// cr[regnum] = rD
	uint8_t MFSR();		// rD = sr[regnum]
	uint8_t MTSR();		// sr[regnum] = rA

	// --- Conditional test / trap / system control ---
	uint8_t TRAPCOND();	// trap{cond} : conditional trap exception
	uint8_t SYSCALL();	// syscall    : unconditional system-call exception
	uint8_t RTE();		// rte        : return from exception (PC = EPC)
	uint8_t DRTE();		// drte       : debug return from exception (PC = DEPC)
	uint8_t SLEEP();	// sleep      : halt until the IRQ line is asserted
	uint8_t CRFORM();	// opcode 0x06 dispatcher

	// --- Un-aligned load/store combine (LCR=sr1, SCR=sr2) ---
	uint8_t LCB();		// lcb        : load-combine begin
	uint8_t LCW();		// lcw        : load-combine word
	uint8_t LCE();		// lce        : load-combine end
	uint8_t SCB();		// scb        : store-combine begin
	uint8_t SCW();		// scw        : store-combine word
	uint8_t SCE();		// sce        : store-combine end

	// Common exception/interrupt entry: shift PSR/condition state into the
	// "saved" slots, save `epc`, and jump to `target`.
	void enterException(uint8_t code, uint32_t epc, uint32_t target);
	void raiseException(uint8_t code);	// synchronous: EPC = faulting instruction
	void takeInterrupt();
	// Seam for ECR.IM masking once its bit position is known.
	bool irqMasked() const { return false; }

	// Little-endian funnel that reconstructs an un-aligned word from two
	// consecutive memory words (low part from LCR, high part from mem).
	static uint32_t combineLoad(uint32_t lcr, uint32_t mem, uint8_t off);
};