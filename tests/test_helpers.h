#pragma once
#include <gtest/gtest.h>
#include "Bus.h"

// ============================================================
// CPUFixture
//
// Base test fixture. A fresh Bus (and therefore a fresh CPU
// with zeroed RAM and a connected bus) is constructed before
// every test case automatically.
// ============================================================
class CPUFixture : public ::testing::Test
{
protected:
    Bus bus;

    // Write a value into the CPU general-purpose register file.
    void setReg(uint8_t reg, uint32_t val)
    {
        bus.cpu.regs[reg] = val;
    }

    // Read a value from the CPU general-purpose register file.
    uint32_t getReg(uint8_t reg)
    {
        return bus.cpu.regs[reg];
    }

    // Place an instruction at the current PC and execute one clock cycle.
    // One call to clock() with cycles==0 fully fetches and executes the
    // instruction, then leaves cycles back at 0.
    void execute(uint32_t instr_word)
    {
        bus.ram[bus.cpu.pc] = instr_word;
        bus.cpu.clock();
    }

    // Read a CPU status flag
    uint8_t getFlag(spg290::FLAGS290 f)
    {
        return bus.cpu.GetFlag(f);
    }

    // Direct bus RAM access — use these for memory operands in load/store tests,
    // not for CPU registers (which are in bus.cpu.regs[]).
    void setMem(uint16_t addr, uint32_t val) { bus.ram[addr] = val; }
    uint32_t getMem(uint16_t addr) const     { return bus.ram[addr]; }
};

// ============================================================
// ROM protocol constants
//
// Test ROM programs must follow this two-word handshake at the
// end of their sequence:
//   1. Write PASS_VALUE or an error code to ROM_RESULT_ADDR.
//   2. Write any non-zero value to ROM_DONE_ADDR.
//
// The addresses are placed near the top of the 64K address space
// so they never collide with a short instruction sequence loaded
// at address 0.
// ============================================================
static constexpr uint32_t ROM_DONE_ADDR   = 0xFF00u; // write non-zero when done
static constexpr uint32_t ROM_RESULT_ADDR = 0xFF01u; // PASS_VALUE or error code
static constexpr uint32_t PASS_VALUE      = 0xDEADu;


// ============================================================
// ROMFixture
//
// Runs a self-contained test ROM (a uint32_t instruction array)
// to completion and exposes the pass/fail result.
//
// ROM contract (see ROM protocol constants above):
//   - Instructions are loaded starting at address 0 (PC starts at 0).
//   - CPU registers (r0-r31) are separate from RAM — no conflict.
//   - ROM writes PASS_VALUE to ROM_RESULT_ADDR when all checks pass.
//   - ROM writes a non-zero error code on failure.
//   - ROM then writes any non-zero value to ROM_DONE_ADDR to signal done.
//
// Usage:
//   static constexpr uint32_t prog[] = { encodeANDX(...), encodeSW(...), ... };
//   runROM(prog, std::size(prog));
//   EXPECT_TRUE(romPassed()) << "error code: " << romResult();
// ============================================================
class ROMFixture : public ::testing::Test
{
protected:
    Bus bus;

    // Load and run a ROM program. Clocks until ROM_DONE_ADDR is set or
    // max_cycles is exhausted, whichever comes first.
    void runROM(const uint32_t* prog, size_t len, uint32_t max_cycles = 100000)
    {
        bus.loadROM(prog, len);
        for (uint32_t i = 0; i < max_cycles; ++i)
        {
            if (bus.ram[ROM_DONE_ADDR] != 0)
                break;
            bus.cpu.clock();
        }
    }

    bool     romPassed() const { return bus.ram[ROM_RESULT_ADDR] == PASS_VALUE; }
    uint32_t romResult() const { return bus.ram[ROM_RESULT_ADDR]; }
};


// ============================================================
// Instruction word encoders
//
// Each function constructs the 32-bit instruction word from its
// constituent fields. Bit 31 is set on all encoders to match the
// real HyperScan instruction encoding (32-bit instruction
// indicator), though the current decoder treats it as don't-care.
//
// All three verified encoders are cross-checked against the
// known-good instruction words in src/main.cpp:
//   encodeANDX (12,10,11,false) == 0x818A2C20
//   encodeANDIX(12,0xFFFF,false) == 0x85937FFE
//   encodeANDRIX(12,10,0x234E,false) == 0xB18A469C
// ============================================================

// --- ANDX : d = a & b  (R-type, opcode=0, func6 bit-5 set) ---
// bits 30:26 = 0 (opcode)
// bits 25:21 = d_reg
// bits 20:16 = a_reg
// bits 14:10 = b_reg
// bits  6:1  = 0x20 (func6 sentinel)
// bit    0   = CU
static inline uint32_t encodeANDX(uint8_t d, uint8_t a, uint8_t b, bool cu)
{
    return 0x80000000u
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(b & 0x1F) << 10)
         | 0x20u          // func6: satisfies (instr & 0x7E) >> 1 == 0x10
         | (cu ? 1u : 0u);
}

// --- ANDIX : d = d & imm16  (I-type, opcode=1, func3=4) ---
// bits 30:26 = 1 (opcode)
// bits 25:21 = d_reg
// bits 20:18 = func3 = 0b100
// bits 17:16 = imm[15:14]
// bit    15  = 0 (unused)
// bits 14:1  = imm[13:0]
// bit    0   = CU
static inline uint32_t encodeANDIX(uint8_t d, uint16_t imm, bool cu)
{
    return 0x80000000u
         | (1u << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | (0x4u << 18)
         | ((uint32_t)((imm >> 14) & 0x3) << 16)
         | (1u << 15)  // 32-bit instruction marker (matches GAS output)
         | ((uint32_t)(imm & 0x3FFF) << 1)
         | (cu ? 1u : 0u);
}

// --- ANDRIX : d = a & imm14  (RI-type, opcode=0b01100=12) ---
// bits 30:26 = 12 (opcode)
// bits 25:21 = d_reg
// bits 20:16 = a_reg
// bits 14:1  = imm14
// bit    0   = CU
static inline uint32_t encodeANDRIX(uint8_t d, uint8_t a, uint16_t imm14, bool cu)
{
    return 0x80000000u
         | (12u << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(imm14 & 0x3FFF) << 1)
         | (cu ? 1u : 0u);
}

// ============================================================
// Generic encoders for this emulator's instruction formats.
// (See the encoding convention documented in src/spg290.h.)
//
//   R-type   : opcode 0,        rD[25:21] rA[20:16] rB/SA[14:10]
//              func6[6:1] CU[0]
//   I-type-1 : opcode 1,        rD[25:21] func3[20:18] imm16[16:1] CU[0]
//   I-type-2 : opcode 2,        rD[25:21] func3[20:18] imm16[16:1] CU[0]
//   RIX-type : opcode 0x0C-0x0F rD[25:21] rA[20:16] imm14[14:1] CU[0]
// ============================================================

// EF6 placeholder values updated to valid 6-bit range (0x26-0x37):
// MVCOND=0x26, REM=0x27, REMU=0x28, ROLIC=0x29, RORIC=0x2A
// ADDS=0x2B, SUBS=0x30, SLLS=0x31, MSBU=0x32, BITREV=0x33
// CLZ/ABS/MIN/MAX kept at original values 0x34-0x37 (no conflict)
enum : uint32_t {
    EF6_NOP=0x00,
    // Arithmetic (confirmed)
    EF6_ADDX=0x08, EF6_ADDCX=0x09, EF6_SUBX=0x0A, EF6_SUBCX=0x0B, EF6_NEGX=0x0F,
    // TODO unknowns (placeholder values in valid 6-bit range)
    EF6_CMP=0x06, EF6_CMPZ=0x07, EF6_MVCOND=0x26, EF6_REM=0x27, EF6_REMU=0x28,
    // Bitwise (confirmed)
    EF6_ANDX=0x10, EF6_ORX=0x11, EF6_NOTX=0x12, EF6_XORX=0x13,
    // TODO unknown
    EF6_BITTSTC=0x14,
    // Shifts/rotates (confirmed)
    EF6_SLLX=0x18, EF6_SRLX=0x1A, EF6_SRAX=0x1B, EF6_ROLX=0x1E, EF6_RORX=0x1C,
    EF6_SLLIX=0x38, EF6_SRLIX=0x3A, EF6_SRAIX=0x3B, EF6_ROLIX=0x3E, EF6_RORIX=0x3C,
    // TODO rotate-through-carry (placeholder values in valid 6-bit range)
    EF6_ROLC=0x17, EF6_ROLIC=0x29, EF6_RORC=0x19, EF6_RORIC=0x2A,
    // Extend (confirmed)
    EF6_EXTSBX=0x2C, EF6_EXTSHX=0x2D, EF6_EXTZBX=0x2E, EF6_EXTZHX=0x2F,
    // Multiply/divide (confirmed)
    EF6_MUL=0x20, EF6_MULU=0x21, EF6_DIV=0x22, EF6_DIVU=0x23,
    // CE register moves (confirmed; rB=1 for CEL, rB=2 for CEH)
    EF6_MFCEL=0x24, EF6_MFCEH=0x24, EF6_MTCEL=0x25, EF6_MTCEH=0x25,
    // HWMAC sub-fields (opcode 0x1C, not R-type)
    EF6_MAD=0x0C, EF6_MADU=0x0D, EF6_MSB=0x0E, EF6_MSBU=0x32,
    // Saturating/misc → HWMAC, placeholders in valid 6-bit range
    EF6_ADDS=0x2B, EF6_SUBS=0x30, EF6_ABSS=0x1D, EF6_SLLS=0x31, EF6_MSBF=0x1F,
    EF6_CLZ=0x34, EF6_ABS=0x35, EF6_MIN=0x36, EF6_MAX=0x37, EF6_BITREV=0x33,
    EF6_MULF=0x15, EF6_MADF=0x16
};

// Raw format builders.
static inline uint32_t encR(uint32_t func6, uint8_t d, uint8_t a, uint8_t b, bool cu)
{
    return 0x80000000u
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(b & 0x1F) << 10)
         | ((func6 & 0x3F) << 1)
         | (cu ? 1u : 0u);
}
// I-type-1 (opcode 1): GAS non-contiguous immediate format.
//   imm16[15:14] → bits 17:16, bit 15 = 1 (32-bit marker), imm16[13:0] → bits 14:1.
// This matches real score-elf-as output exactly.
static inline uint32_t encI1(uint32_t func3, uint8_t d, uint16_t imm16, bool cu)
{
    return 0x80000000u | (1u << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((func3 & 0x7) << 18)
         | ((uint32_t)((imm16 >> 14) & 0x3) << 16)  // imm[15:14] → bits 17:16
         | (1u << 15)                                  // 32-bit instruction marker
         | ((uint32_t)(imm16 & 0x3FFF) << 1)          // imm[13:0] → bits 14:1
         | (cu ? 1u : 0u);
}
// I-type-S (opcode 5): shifted-immediate class (addis/andis/oris).
// Same non-contiguous immediate format as I-type-1.
static inline uint32_t encIS(uint32_t func3, uint8_t d, uint16_t imm16, bool cu)
{
    return 0x80000000u | (5u << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((func3 & 0x7) << 18)
         | ((uint32_t)((imm16 >> 14) & 0x3) << 16)
         | (1u << 15)
         | ((uint32_t)(imm16 & 0x3FFF) << 1)
         | (cu ? 1u : 0u);
}
// I-type-2 (placeholder opcode 0x03): SUBIX/LDIS — real opcodes TODO.
static inline uint32_t encI2(uint32_t func3, uint8_t d, uint16_t imm16, bool cu)
{
    return 0x80000000u | (0x03u << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((func3 & 0x7) << 18)
         | ((uint32_t)((imm16 >> 14) & 0x3) << 16)
         | (1u << 15)
         | ((uint32_t)(imm16 & 0x3FFF) << 1)
         | (cu ? 1u : 0u);
}
static inline uint32_t encRIX(uint32_t opcode, uint8_t d, uint8_t a, uint16_t imm14, bool cu)
{
    return 0x80000000u | ((opcode & 0x1F) << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(imm14 & 0x3FFF) << 1)
         | (cu ? 1u : 0u);
}

// --- Previously-scaffolded instructions (now wired up) ---
static inline uint32_t encodeANDISX(uint8_t d, uint16_t imm16, bool cu) { return encIS(4, d, imm16, cu); }  // andis opcode=5 func3=4
static inline uint32_t encodeADDCX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_ADDCX, d, a, b, cu); }
static inline uint32_t encodeADDIX(uint8_t d, uint16_t imm16, bool cu) { return encI1(0, d, imm16, cu); }  // addi opcode=1 func3=0
static inline uint32_t encodeADDISX(uint8_t d, uint16_t imm16, bool cu) { return encIS(0, d, imm16, cu); }  // addis opcode=5 func3=0
static inline uint32_t encodeADDRIX(uint8_t d, uint8_t a, uint16_t imm14, bool cu) { return encRIX(0x08, d, a, imm14, cu); }  // addri opcode=0x08
static inline uint32_t encodeORX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_ORX, d, a, b, cu); }
static inline uint32_t encodeBITTSTC(uint8_t a, uint8_t bn, bool cu) { return encR(EF6_BITTSTC, 0, a, bn, cu); }

// --- R-type arithmetic / logic ---
static inline uint32_t encodeADDX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_ADDX, d, a, b, cu); }
static inline uint32_t encodeSUBX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_SUBX, d, a, b, cu); }
static inline uint32_t encodeSUBCX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_SUBCX, d, a, b, cu); }
static inline uint32_t encodeNEGX(uint8_t d, uint8_t b, bool cu) { return encR(EF6_NEGX, d, 0, b, cu); }
static inline uint32_t encodeNOTX(uint8_t d, uint8_t a, bool cu) { return encR(EF6_NOTX, d, a, 0, cu); }
static inline uint32_t encodeXORX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_XORX, d, a, b, cu); }
static inline uint32_t encodeNOP() { return encR(EF6_NOP, 0, 0, 0, false); }
// cmp / cmpz carry their 2-bit TCS in the low bits of the rD field.
static inline uint32_t encodeCMP(uint8_t a, uint8_t b, uint8_t tcs) { return encR(EF6_CMP, tcs & 0x3, a, b, true); }
static inline uint32_t encodeCMPZ(uint8_t a, uint8_t tcs) { return encR(EF6_CMPZ, tcs & 0x3, a, 0, true); }
// mv{cond} carries its 4-bit EC in the low bits of the rB field.
static inline uint32_t encodeMVCOND(uint8_t d, uint8_t a, uint8_t ec) { return encR(EF6_MVCOND, d, a, ec & 0xF, false); }

// --- Immediate forms ---
static inline uint32_t encodeORIX(uint8_t d, uint16_t imm16, bool cu) { return encI1(5, d, imm16, cu); }  // ori opcode=1 func3=5
static inline uint32_t encodeORISX(uint8_t d, uint16_t imm16, bool cu) { return encIS(5, d, imm16, cu); }  // oris opcode=5 func3=5
static inline uint32_t encodeCMPI(uint8_t d, uint16_t imm16) { return encI1(2, d, imm16, true); }
static inline uint32_t encodeLDI(uint8_t d, uint16_t imm16) { return encI1(6, d, imm16, false); }  // ldi opcode=1 func3=6
static inline uint32_t encodeLDIS(uint8_t d, uint16_t imm16) { return encI2(0, d, imm16, false); }
static inline uint32_t encodeSUBIX(uint8_t d, uint16_t imm16, bool cu) { return encI2(1, d, imm16, cu); }
static inline uint32_t encodeSUBISX(uint8_t d, uint16_t imm16, bool cu) { return encI2(2, d, imm16, cu); }
static inline uint32_t encodeORRIX(uint8_t d, uint8_t a, uint16_t imm14, bool cu) { return encRIX(0x0D, d, a, imm14, cu); }  // orri opcode=0x0D
static inline uint32_t encodeSUBRIX(uint8_t d, uint8_t a, uint16_t imm14, bool cu) { return encRIX(0x0F, d, a, imm14, cu); }

// --- Shifts / rotates (register and immediate) ---
static inline uint32_t encodeSLLX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_SLLX, d, a, b, cu); }
static inline uint32_t encodeSRLX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_SRLX, d, a, b, cu); }
static inline uint32_t encodeSRAX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_SRAX, d, a, b, cu); }
static inline uint32_t encodeROLX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_ROLX, d, a, b, cu); }
static inline uint32_t encodeRORX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_RORX, d, a, b, cu); }
static inline uint32_t encodeSLLIX(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_SLLIX, d, a, sa, cu); }
static inline uint32_t encodeSRLIX(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_SRLIX, d, a, sa, cu); }
static inline uint32_t encodeSRAIX(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_SRAIX, d, a, sa, cu); }
static inline uint32_t encodeROLIX(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_ROLIX, d, a, sa, cu); }
static inline uint32_t encodeRORIX(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_RORIX, d, a, sa, cu); }

// --- Bit / extend / misc ---
static inline uint32_t encodeEXTSBX(uint8_t d, uint8_t a, bool cu) { return encR(EF6_EXTSBX, d, a, 0, cu); }
static inline uint32_t encodeEXTSHX(uint8_t d, uint8_t a, bool cu) { return encR(EF6_EXTSHX, d, a, 0, cu); }
static inline uint32_t encodeEXTZBX(uint8_t d, uint8_t a, bool cu) { return encR(EF6_EXTZBX, d, a, 0, cu); }
static inline uint32_t encodeEXTZHX(uint8_t d, uint8_t a, bool cu) { return encR(EF6_EXTZHX, d, a, 0, cu); }
static inline uint32_t encodeCLZ(uint8_t d, uint8_t a) { return encR(EF6_CLZ, d, a, 0, false); }
static inline uint32_t encodeABS(uint8_t d, uint8_t a) { return encR(EF6_ABS, d, a, 0, false); }
static inline uint32_t encodeMIN(uint8_t d, uint8_t a, uint8_t b) { return encR(EF6_MIN, d, a, b, false); }
static inline uint32_t encodeMAX(uint8_t d, uint8_t a, uint8_t b) { return encR(EF6_MAX, d, a, b, false); }
static inline uint32_t encodeBITREV(uint8_t d, uint8_t a, uint8_t b) { return encR(EF6_BITREV, d, a, b, false); }

// --- Custom engine: multiply / divide / moves ---
static inline uint32_t encodeMUL(uint8_t a, uint8_t b) { return encR(EF6_MUL, 0, a, b, false); }
static inline uint32_t encodeMULU(uint8_t a, uint8_t b) { return encR(EF6_MULU, 0, a, b, false); }
static inline uint32_t encodeDIV(uint8_t a, uint8_t b) { return encR(EF6_DIV, 0, a, b, false); }
static inline uint32_t encodeDIVU(uint8_t a, uint8_t b) { return encR(EF6_DIVU, 0, a, b, false); }
static inline uint32_t encodeREM(uint8_t d, uint8_t a, uint8_t b) { return encR(EF6_REM, d, a, b, false); }
static inline uint32_t encodeREMU(uint8_t d, uint8_t a, uint8_t b) { return encR(EF6_REMU, d, a, b, false); }
// rB=1 distinguishes CEL; rB=2 distinguishes CEH (both share func6=0x24/0x25)
static inline uint32_t encodeMFCEL(uint8_t d) { return encR(EF6_MFCEL, d, 0, 1, false); }  // rB=1
static inline uint32_t encodeMFCEH(uint8_t d) { return encR(EF6_MFCEH, d, 0, 2, false); }  // rB=2
static inline uint32_t encodeMTCEL(uint8_t a) { return encR(EF6_MTCEL, 0, a, 1, false); }  // rB=1
static inline uint32_t encodeMTCEH(uint8_t a) { return encR(EF6_MTCEH, 0, a, 2, false); }  // rB=2

// --- Custom engine: multiply-accumulate (func6 0x0C-0x0F, 0x15, 0x16) ---
static inline uint32_t encodeMAD(uint8_t a, uint8_t b)  { return encR(0x0C, 0, a, b, false); }
static inline uint32_t encodeMADU(uint8_t a, uint8_t b) { return encR(0x0D, 0, a, b, false); }
static inline uint32_t encodeMSB(uint8_t a, uint8_t b)  { return encR(0x0E, 0, a, b, false); }
static inline uint32_t encodeMSBU(uint8_t a, uint8_t b) { return encR(EF6_MSBU, 0, a, b, false); }
static inline uint32_t encodeMULF(uint8_t a, uint8_t b) { return encR(0x15, 0, a, b, false); }
static inline uint32_t encodeMADF(uint8_t a, uint8_t b) { return encR(0x16, 0, a, b, false); }

// --- Loads / stores : opcode, rD, rA, signed Imm15[15:1] ---
static inline uint32_t encLS(uint32_t opcode, uint8_t d, uint8_t a, uint16_t imm15)
{
    return 0x80000000u | ((opcode & 0x1F) << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(imm15 & 0x7FFF) << 1);
}
static inline uint32_t encodeLW(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x10, d, a, imm); }  // OP_LW=0x10 confirmed
static inline uint32_t encodeLH(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x11, d, a, imm); }  // OP_LH=0x11 (was 0x15)
static inline uint32_t encodeLHU(uint8_t d, uint8_t a, uint16_t imm) { return encLS(0x12, d, a, imm); }  // OP_LHU=0x12 (was 0x14)
static inline uint32_t encodeLB(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x13, d, a, imm); }  // OP_LB=0x13 confirmed
static inline uint32_t encodeSW(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x14, d, a, imm); }  // OP_SW=0x14 (was 0x11)
static inline uint32_t encodeSH(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x15, d, a, imm); }  // OP_SH=0x15 (was 0x17)
static inline uint32_t encodeLBU(uint8_t d, uint8_t a, uint16_t imm) { return encLS(0x16, d, a, imm); }  // OP_LBU=0x16 (was 0x12)
static inline uint32_t encodeSB(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x17, d, a, imm); }  // OP_SB=0x17 (was 0x16)

// --- Control flow ---
static inline uint32_t encodeBCOND(uint8_t bc, int32_t disp)
{
    return 0x80000000u | (0x18u << 26)
         | ((uint32_t)(bc & 0xF) << 22)
         | (((uint32_t)disp & 0x1FFFFF) << 1);
}
static inline uint32_t encodeJX(uint32_t disp24, bool lk)
{
    return 0x80000000u | (0x02u << 26)  // OP_JX=0x02 (was 0x19)
         | ((disp24 & 0xFFFFFF) << 1)
         | (lk ? 1u : 0u);
}
static inline uint32_t encodeBR(uint8_t bc, uint8_t a)
{
    return 0x80000000u | (0x1Au << 26)
         | ((uint32_t)(bc & 0xF) << 22)
         | ((uint32_t)(a & 0x1F) << 16);
}
static inline uint32_t encodeBCONDL(uint8_t bc, int32_t disp)
{
    return 0x80000000u | (0x1Du << 26)
         | ((uint32_t)(bc & 0xF) << 22)
         | (((uint32_t)disp & 0x1FFFFF) << 1);
}
static inline uint32_t encodeBRL(uint8_t bc, uint8_t a)
{
    return 0x80000000u | (0x1Eu << 26)
         | ((uint32_t)(bc & 0xF) << 22)
         | ((uint32_t)(a & 0x1F) << 16);
}

// --- Rotate through carry (func6 0x17-0x1A) ---
static inline uint32_t encodeROLC(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(0x17, d, a, b, cu); }
static inline uint32_t encodeROLIC(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_ROLIC, d, a, sa, cu); }
static inline uint32_t encodeRORC(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(0x19, d, a, b, cu); }
static inline uint32_t encodeRORIC(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_RORIC, d, a, sa, cu); }

// --- Saturating arithmetic (func6 0x1B-0x1E) ---
static inline uint32_t encodeADDS(uint8_t d, uint8_t a, uint8_t b) { return encR(EF6_ADDS, d, a, b, false); }
static inline uint32_t encodeSUBS(uint8_t d, uint8_t a, uint8_t b) { return encR(EF6_SUBS, d, a, b, false); }
static inline uint32_t encodeABSS(uint8_t d, uint8_t a) { return encR(0x1D, d, a, 0, false); }
static inline uint32_t encodeSLLS(uint8_t d, uint8_t a, uint8_t b) { return encR(EF6_SLLS, d, a, b, false); }

// --- Custom engine: 32-bit fractional multiply-subtract (func6 0x1F) ---
static inline uint32_t encodeMSBF(uint8_t a, uint8_t b) { return encR(0x1F, 0, a, b, false); }

// --- Half-word multiply-accumulate family (opcode 0x1C) ---
// HZFSU control field occupies bits [25:21].
static inline uint32_t encHWMAC(uint8_t hzfsu, uint8_t a, uint8_t b)
{
    return 0x80000000u | (0x1Cu << 26)
         | ((uint32_t)(hzfsu & 0x1F) << 21)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(b & 0x1F) << 10);
}
// HZFSU bit layout: H=0x10, Z=0x08, F=0x04, S=0x02, U=0x01.
// Lower-half forms (H=0):
static inline uint32_t encodeMAZL(uint8_t a, uint8_t b)   { return encHWMAC(0x08, a, b); }            // Z
static inline uint32_t encodeMADL(uint8_t a, uint8_t b)   { return encHWMAC(0x00, a, b); }            // -
static inline uint32_t encodeMSZL(uint8_t a, uint8_t b)   { return encHWMAC(0x0A, a, b); }            // Z,S
static inline uint32_t encodeMSBL(uint8_t a, uint8_t b)   { return encHWMAC(0x02, a, b); }            // S
static inline uint32_t encodeMAZLF(uint8_t a, uint8_t b)  { return encHWMAC(0x0C, a, b); }            // Z,F
static inline uint32_t encodeMSZLF(uint8_t a, uint8_t b)  { return encHWMAC(0x0E, a, b); }            // Z,S,F
static inline uint32_t encodeMADLFS(uint8_t a, uint8_t b) { return encHWMAC(0x04, a, b); }            // F (sat)
static inline uint32_t encodeMSBLFS(uint8_t a, uint8_t b) { return encHWMAC(0x06, a, b); }            // S,F (sat)
// Higher-half forms (H=1):
static inline uint32_t encodeMAZH(uint8_t a, uint8_t b)   { return encHWMAC(0x18, a, b); }            // H,Z
static inline uint32_t encodeMADH(uint8_t a, uint8_t b)   { return encHWMAC(0x10, a, b); }            // H
static inline uint32_t encodeMSZH(uint8_t a, uint8_t b)   { return encHWMAC(0x1A, a, b); }            // H,Z,S
static inline uint32_t encodeMSBH(uint8_t a, uint8_t b)   { return encHWMAC(0x12, a, b); }            // H,S
static inline uint32_t encodeMAZHF(uint8_t a, uint8_t b)  { return encHWMAC(0x1C, a, b); }            // H,Z,F
static inline uint32_t encodeMSZHF(uint8_t a, uint8_t b)  { return encHWMAC(0x1E, a, b); }            // H,Z,S,F
static inline uint32_t encodeMADHFS(uint8_t a, uint8_t b) { return encHWMAC(0x14, a, b); }            // H,F (sat)
static inline uint32_t encodeMSBHFS(uint8_t a, uint8_t b) { return encHWMAC(0x16, a, b); }            // H,S,F (sat)

// --- Pre/post-index load/store family (opcode 0x1B) ---
// sub-op[15:13], signed Imm12[12:1], pre-index flag at bit 0.
static inline uint32_t encLSU(uint8_t sub, uint8_t d, uint8_t a, int16_t imm12, bool pre)
{
    return 0x80000000u | (0x1Bu << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(sub & 0x7) << 13)
         | (((uint32_t)imm12 & 0xFFF) << 1)
         | (pre ? 1u : 0u);
}
static inline uint32_t encodeLW_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return encLSU(0, d, a, imm, pre); }
static inline uint32_t encodeSW_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return encLSU(1, d, a, imm, pre); }
static inline uint32_t encodeLBU_idx(uint8_t d, uint8_t a, int16_t imm, bool pre) { return encLSU(2, d, a, imm, pre); }
static inline uint32_t encodeLB_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return encLSU(3, d, a, imm, pre); }
static inline uint32_t encodeLHU_idx(uint8_t d, uint8_t a, int16_t imm, bool pre) { return encLSU(4, d, a, imm, pre); }
static inline uint32_t encodeLH_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return encLSU(5, d, a, imm, pre); }
static inline uint32_t encodeSB_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return encLSU(6, d, a, imm, pre); }
static inline uint32_t encodeSH_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return encLSU(7, d, a, imm, pre); }

// ============================================================
// 16-bit ("!") instruction encoders. All clear bit 31 so the decoder
// routes them through decode16(). See the layout in src/spg290.h.
// ============================================================

// op16 0x0 : RR-ALU  (func4, rD, rA)
static inline uint32_t enc16_rr(uint8_t func4, uint8_t d, uint8_t a)
{
    return ((uint32_t)0x0 << 12) | ((uint32_t)(func4 & 0xF) << 8)
         | ((uint32_t)(d & 0xF) << 4) | (uint32_t)(a & 0xF);
}
static inline uint32_t encodeADD16(uint8_t d, uint8_t a)  { return enc16_rr(0, d, a); }
static inline uint32_t encodeADDC16(uint8_t d, uint8_t a) { return enc16_rr(1, d, a); }
static inline uint32_t encodeSUB16(uint8_t d, uint8_t a)  { return enc16_rr(2, d, a); }
static inline uint32_t encodeAND16(uint8_t d, uint8_t a)  { return enc16_rr(3, d, a); }
static inline uint32_t encodeOR16(uint8_t d, uint8_t a)   { return enc16_rr(4, d, a); }
static inline uint32_t encodeXOR16(uint8_t d, uint8_t a)  { return enc16_rr(5, d, a); }
static inline uint32_t encodeMV16(uint8_t d, uint8_t a)   { return enc16_rr(6, d, a); }
static inline uint32_t encodeNEG16(uint8_t d, uint8_t a)  { return enc16_rr(7, d, a); }
static inline uint32_t encodeNOT16(uint8_t d, uint8_t a)  { return enc16_rr(8, d, a); }
static inline uint32_t encodeCMP16(uint8_t d, uint8_t a)  { return enc16_rr(9, d, a); }
static inline uint32_t encodeSLL16(uint8_t d, uint8_t a)  { return enc16_rr(10, d, a); }
static inline uint32_t encodeSRL16(uint8_t d, uint8_t a)  { return enc16_rr(11, d, a); }
static inline uint32_t encodeSRA16(uint8_t d, uint8_t a)  { return enc16_rr(12, d, a); }

// op16 0x1 : ldiu!  (rD, Imm8)
static inline uint32_t encodeLDIU16(uint8_t d, uint8_t imm8)
{
    return ((uint32_t)0x1 << 12) | ((uint32_t)(d & 0xF) << 8) | (uint32_t)imm8;
}

// op16 0x2 : R-Imm  (func2, rD, Imm)
static inline uint32_t enc16_rimm(uint8_t func2, uint8_t d, uint8_t imm)
{
    return ((uint32_t)0x2 << 12) | ((uint32_t)(func2 & 0x3) << 10)
         | ((uint32_t)(d & 0xF) << 6) | (uint32_t)(imm & 0x3F);
}
static inline uint32_t encodeSLLI16(uint8_t d, uint8_t sa5)   { return enc16_rimm(0, d, sa5 & 0x1F); }
static inline uint32_t encodeSRLI16(uint8_t d, uint8_t sa5)   { return enc16_rimm(1, d, sa5 & 0x1F); }
static inline uint32_t encodeADDEI16(uint8_t d, uint8_t imm4) { return enc16_rimm(2, d, imm4 & 0xF); }
static inline uint32_t encodeSUBEI16(uint8_t d, uint8_t imm4) { return enc16_rimm(3, d, imm4 & 0xF); }

// op16 0x3 : bittst!  (rD, BN)
static inline uint32_t encodeBITTST16(uint8_t d, uint8_t bn)
{
    return ((uint32_t)0x3 << 12) | ((uint32_t)(d & 0xF) << 8) | ((uint32_t)(bn & 0x1F) << 3);
}

// op16 0x4 : cross-bank moves  (func1, rD, rA)
static inline uint32_t encodeMLFH16(uint8_t d, uint8_t a)  // rD(lo) = rA(hi)
{
    return ((uint32_t)0x4 << 12) | ((uint32_t)(d & 0xF) << 4) | (uint32_t)(a & 0xF);
}
static inline uint32_t encodeMHFL16(uint8_t d, uint8_t a)  // rD(hi) = rA(lo)
{
    return ((uint32_t)0x4 << 12) | ((uint32_t)0x1 << 11)
         | ((uint32_t)(d & 0xF) << 4) | (uint32_t)(a & 0xF);
}

// op16 0x5 : memory simple  (func3, rD, rA)
static inline uint32_t enc16_mem(uint8_t func3, uint8_t d, uint8_t a)
{
    return ((uint32_t)0x5 << 12) | ((uint32_t)(func3 & 0x7) << 9)
         | ((uint32_t)(d & 0xF) << 5) | ((uint32_t)(a & 0xF) << 1);
}
static inline uint32_t encodeLW16(uint8_t d, uint8_t a)  { return enc16_mem(0, d, a); }
static inline uint32_t encodeLH16(uint8_t d, uint8_t a)  { return enc16_mem(1, d, a); }
static inline uint32_t encodeLBU16(uint8_t d, uint8_t a) { return enc16_mem(2, d, a); }
static inline uint32_t encodeSW16(uint8_t d, uint8_t a)  { return enc16_mem(3, d, a); }
static inline uint32_t encodeSH16(uint8_t d, uint8_t a)  { return enc16_mem(4, d, a); }
static inline uint32_t encodeSB16(uint8_t d, uint8_t a)  { return enc16_mem(5, d, a); }

// op16 0x6 : memory BP-relative  (func3, rD, Imm5)  (BP = r2)
static inline uint32_t enc16_membp(uint8_t func3, uint8_t d, uint8_t imm5)
{
    return ((uint32_t)0x6 << 12) | ((uint32_t)(func3 & 0x7) << 9)
         | ((uint32_t)(d & 0xF) << 5) | (uint32_t)(imm5 & 0x1F);
}
static inline uint32_t encodeLWP16(uint8_t d, uint8_t imm5)  { return enc16_membp(0, d, imm5); }
static inline uint32_t encodeLHP16(uint8_t d, uint8_t imm5)  { return enc16_membp(1, d, imm5); }
static inline uint32_t encodeLBUP16(uint8_t d, uint8_t imm5) { return enc16_membp(2, d, imm5); }
static inline uint32_t encodeSWP16(uint8_t d, uint8_t imm5)  { return enc16_membp(3, d, imm5); }
static inline uint32_t encodeSHP16(uint8_t d, uint8_t imm5)  { return enc16_membp(4, d, imm5); }
static inline uint32_t encodeSBP16(uint8_t d, uint8_t imm5)  { return enc16_membp(5, d, imm5); }

// op16 0x7 : stack  (func1, H, rD, rA)
static inline uint32_t encodePOP16(uint8_t d, uint8_t a, bool high)
{
    return ((uint32_t)0x7 << 12) | ((uint32_t)(high ? 1u : 0u) << 10)
         | ((uint32_t)(d & 0xF) << 6) | ((uint32_t)(a & 0xF) << 2);
}
static inline uint32_t encodePUSH16(uint8_t d, uint8_t a, bool high)
{
    return ((uint32_t)0x7 << 12) | ((uint32_t)0x1 << 11) | ((uint32_t)(high ? 1u : 0u) << 10)
         | ((uint32_t)(d & 0xF) << 6) | ((uint32_t)(a & 0xF) << 2);
}

// op16 0x8 : b{cond}!  (cond, Disp8)
static inline uint32_t encodeBCOND16(uint8_t cond, int8_t disp8)
{
    return ((uint32_t)0x8 << 12) | ((uint32_t)(cond & 0xF) << 8) | (uint32_t)((uint8_t)disp8);
}

// op16 0x9 : br{cond}!  (cond, rA)
static inline uint32_t encodeBRCOND16(uint8_t cond, uint8_t a)
{
    return ((uint32_t)0x9 << 12) | ((uint32_t)(cond & 0xF) << 8) | ((uint32_t)(a & 0xF) << 4);
}

// op16 0xA : jx!  (LK, Disp11)
static inline uint32_t encodeJX16(uint16_t disp11, bool lk)
{
    return ((uint32_t)0xA << 12) | ((uint32_t)(lk ? 1u : 0u) << 11) | (uint32_t)(disp11 & 0x7FF);
}

// op16 0xB : system  (func2, cond)
static inline uint32_t encodeNOP16()  { return ((uint32_t)0xB << 12); }
static inline uint32_t encodeSDBBP16(){ return ((uint32_t)0xB << 12) | ((uint32_t)0x1 << 10); }
static inline uint32_t encodeTCOND16(uint8_t cond)
{
    return ((uint32_t)0xB << 12) | ((uint32_t)0x2 << 10) | ((uint32_t)(cond & 0xF) << 6);
}

// ============================================================
// System / coprocessor register moves (opcode 0x1F = OP_SYS).
//   rD = bits 25:21, regnum/rA = bits 20:16, sub-op = bits 14:10,
//   CP#/{HI,LO} = bits 9:8.
// ============================================================
static inline uint32_t encSYS(uint8_t sub, uint8_t d, uint8_t regnum, uint8_t cp_hilo)
{
    return 0x80000000u | ((uint32_t)0x1F << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)(regnum & 0x1F) << 16)
         | ((uint32_t)(sub & 0x1F) << 10)
         | ((uint32_t)(cp_hilo & 0x3) << 8);
}
static inline uint32_t encodeMFCR(uint8_t d, uint8_t crn)  { return encSYS(0x00, d, crn, 0); }
static inline uint32_t encodeMTCR(uint8_t d, uint8_t crn)  { return encSYS(0x01, d, crn, 0); }
static inline uint32_t encodeMFSR(uint8_t d, uint8_t srn)  { return encSYS(0x02, d, srn, 0); }
static inline uint32_t encodeMTSR(uint8_t a, uint8_t srn)  { return encSYS(0x03, a, srn, 0); }
static inline uint32_t encodeMFCX(uint8_t d, uint8_t crn, uint8_t cp)  { return encSYS(0x04, d, crn, cp); }
static inline uint32_t encodeMTCX(uint8_t d, uint8_t crn, uint8_t cp)  { return encSYS(0x05, d, crn, cp); }
static inline uint32_t encodeMFCCX(uint8_t d, uint8_t crn, uint8_t cp) { return encSYS(0x06, d, crn, cp); }
static inline uint32_t encodeMTCCX(uint8_t d, uint8_t crn, uint8_t cp) { return encSYS(0x07, d, crn, cp); }
// mfcex / mtcex: hilo = {HI,LO} (1=L only, 2=H only, 3=both).
static inline uint32_t encodeMFCEX(uint8_t d, uint8_t a, uint8_t hilo) { return encSYS(0x08, d, a, hilo); }
static inline uint32_t encodeMTCEX(uint8_t d, uint8_t a, uint8_t hilo) { return encSYS(0x09, d, a, hilo); }
static inline uint32_t encodeCOPX() { return encSYS(0x0A, 0, 0, 0); }
// t{cond} / trap{cond}: EC occupies the regnum field (bits 20:16).
static inline uint32_t encodeTCOND(uint8_t ec)    { return encSYS(0x0B, 0, ec, 0); }
static inline uint32_t encodeTRAPCOND(uint8_t ec) { return encSYS(0x0C, 0, ec, 0); }
static inline uint32_t encodeSYSCALL()            { return encSYS(0x0D, 0, 0, 0); }
static inline uint32_t encodeRTE()                { return encSYS(0x0E, 0, 0, 0); }
static inline uint32_t encodeDRTE()               { return encSYS(0x0F, 0, 0, 0); }
static inline uint32_t encodeSLEEP()              { return encSYS(0x10, 0, 0, 0); }
static inline uint32_t encodeCACHE()              { return encSYS(0x11, 0, 0, 0); }
static inline uint32_t encodePFLUSH()             { return encSYS(0x12, 0, 0, 0); }
static inline uint32_t encodeSDBBP32()            { return encSYS(0x13, 0, 0, 0); }
static inline uint32_t encodeCEINST()             { return encSYS(0x14, 0, 0, 0); }
// ldcx / stcx: base GPR=bits 25:21, CrA=bits 20:16, CP#=bits 9:8, imm8=bits 7:0.
static inline uint32_t encodeLDCX(uint8_t base, uint8_t creg, uint8_t cp, int8_t imm)
{
    return encSYS(0x15, base, creg, cp) | ((uint32_t)(uint8_t)imm);
}
static inline uint32_t encodeSTCX(uint8_t base, uint8_t creg, uint8_t cp, int8_t imm)
{
    return encSYS(0x16, base, creg, cp) | ((uint32_t)(uint8_t)imm);
}
// Load/store combine: rD = bits 25:21, rA (base) = bits 20:16.
// lcb/sce take only a base register (rD field unused).
static inline uint32_t encodeLCB(uint8_t a)            { return encSYS(0x17, 0, a, 0); }
static inline uint32_t encodeLCW(uint8_t d, uint8_t a) { return encSYS(0x18, d, a, 0); }
static inline uint32_t encodeLCE(uint8_t d, uint8_t a) { return encSYS(0x19, d, a, 0); }
static inline uint32_t encodeSCB(uint8_t d, uint8_t a) { return encSYS(0x1A, d, a, 0); }
static inline uint32_t encodeSCW(uint8_t d, uint8_t a) { return encSYS(0x1B, d, a, 0); }
static inline uint32_t encodeSCE(uint8_t a)            { return encSYS(0x1C, 0, a, 0); }
