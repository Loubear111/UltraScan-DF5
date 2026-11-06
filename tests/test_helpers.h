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
        bus.write32(bus.cpu.pc, instr_word);
        bus.cpu.clock();
    }

    // Place a 16-bit instruction at the current PC and execute one clock cycle.
    void execute16(uint16_t instr_half)
    {
        bus.write16(bus.cpu.pc, instr_half);
        bus.cpu.clock();
    }

    // Read a CPU status flag
    uint8_t getFlag(spg290::FLAGS290 f)
    {
        return bus.cpu.GetFlag(f);
    }

    // Direct bus memory access (byte-addressed, full 32-bit word) — use
    // these for memory operands in load/store tests, not for CPU registers
    // (which are in bus.cpu.regs[]).
    void setMem(uint32_t addr, uint32_t val) { bus.write32(addr, val); }
    uint32_t getMem(uint32_t addr) const     { return bus.read32(addr); }
};

// ============================================================
// ROM protocol constants
//
// Test ROM programs must follow this two-word handshake at the
// end of their sequence:
//   1. Write PASS_VALUE or an error code to ROM_RESULT_ADDR.
//   2. Write any non-zero value to ROM_DONE_ADDR.
//
// These are real byte addresses, word-aligned and 4 bytes apart so a
// 32-bit SW to each does not overlap the other; placed near the top of
// the address space so they never collide with a short instruction
// sequence loaded at address 0.
// ============================================================
static constexpr uint32_t ROM_DONE_ADDR   = 0xFF00u; // write non-zero when done
static constexpr uint32_t ROM_RESULT_ADDR = 0xFF04u; // PASS_VALUE or error code
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
            if (bus.read32(ROM_DONE_ADDR) != 0)
                break;
            bus.cpu.clock();
        }
    }

    bool     romPassed() const { return bus.read32(ROM_RESULT_ADDR) == PASS_VALUE; }
    uint32_t romResult() const { return bus.read32(ROM_RESULT_ADDR); }

    // Like runROM, but raises a PPU vblank-start event every `period` clocks
    // (period 0 = never), standing in for the Phase 4 video timing.
    void runROMWithVBlank(const uint32_t* prog, size_t len, uint32_t period,
                          uint32_t max_cycles = 200000)
    {
        bus.loadROM(prog, len);
        for (uint32_t i = 0; i < max_cycles; ++i)
        {
            if (bus.read32(ROM_DONE_ADDR) != 0)
                break;
            if (period != 0 && i % period == period - 1)
            {
                bus.ppu.signalVBlankStart();
                bus.updateIRQ();
            }
            bus.cpu.clock();
        }
    }
};


// ============================================================
// Instruction word encoders (spec-derived)
//
// Layouts come from s_core7.pdf (section 1.1 and the per-instruction Form
// diagrams) and docs/phase4a-conformance-fix-plan.md section 2. They are NOT
// derived from the emulator's decoder, so a decoder bug can no longer be
// hidden by a matching encoder bug. tests/test_encoding_conformance.cpp
// cross-checks every encoder against an independent decoder (MAME's S+core7
// disassembler, tools/sdis).
//
// 32-bit instructions: the architectural word has two P-bits (bit 31 and
// bit 15, both 1). Fields are laid out in the 30-bit P-bit-free word `op`
// (op bits 29:25 = opcode); enc32_pack() inserts the P-bits:
//     raw = 0x80008000 | ((op & 0x3FFF8000) << 1) | (op & 0x7FFF)
//
// 16-bit instructions: the encoders return the raw 16-bit instruction
// (P-bit 15 = 0) as a uint16_t, e.g. nop! == 0x0000, b! (always) disp 0 ==
// 0x4F00. Where it lands in memory depends on bus byte order and pc & 2;
// that is the fetch rewrite's concern (plan story 5), not the encoder's.
// ============================================================

// --- 32-bit builders, one per form ---

// Insert the two P-bits into a 30-bit P-bit-free instruction word.
static inline uint32_t enc32_pack(uint32_t op)
{
    return 0x80008000u | ((op & 0x3FFF8000u) << 1) | (op & 0x7FFFu);
}

// Special-form (opcode 0): rD[24:20] rA[19:15] rB[14:10] func6[6:1] CU[0]
// (field positions are in `op`; in the raw word they are 25:21, 20:16, 14:10).
static inline uint32_t enc32_special(uint8_t rd, uint8_t ra, uint8_t rb, uint32_t func6, bool cu)
{
    return enc32_pack(((uint32_t)(rd & 0x1F) << 20)
                    | ((uint32_t)(ra & 0x1F) << 15)
                    | ((uint32_t)(rb & 0x1F) << 10)
                    | ((func6 & 0x3F) << 1)
                    | (cu ? 1u : 0u));
}

// I-form (opcodes 1 and 5): rD, func3[19:17], Imm16[16:1], CU[0].
static inline uint32_t enc32_I(uint32_t opc, uint8_t rd, uint32_t func3, uint16_t imm16, bool cu)
{
    return enc32_pack(((opc & 0x1F) << 25)
                    | ((uint32_t)(rd & 0x1F) << 20)
                    | ((func3 & 0x7) << 17)
                    | ((uint32_t)imm16 << 1)
                    | (cu ? 1u : 0u));
}

// RI-form (opcodes 0x08, 0x0C, 0x0D): rD, rA, Imm14[14:1], CU[0].
static inline uint32_t enc32_RI(uint32_t opc, uint8_t rd, uint8_t ra, uint16_t imm14, bool cu)
{
    return enc32_pack(((opc & 0x1F) << 25)
                    | ((uint32_t)(rd & 0x1F) << 20)
                    | ((uint32_t)(ra & 0x1F) << 15)
                    | ((uint32_t)(imm14 & 0x3FFF) << 1)
                    | (cu ? 1u : 0u));
}

// RIX-form (opcode 3 pre-index, opcode 7 post-index): rD, rA, Imm12[14:3], func3[2:0].
static inline uint32_t enc32_RIX(uint32_t opc, uint8_t rd, uint8_t ra, int32_t imm12, uint32_t func3)
{
    return enc32_pack(((opc & 0x1F) << 25)
                    | ((uint32_t)(rd & 0x1F) << 20)
                    | ((uint32_t)(ra & 0x1F) << 15)
                    | (((uint32_t)imm12 & 0xFFFu) << 3)
                    | (func3 & 0x7));
}

// LS-form (opcodes 0x10-0x18): rD, rA, Imm15[14:0].
static inline uint32_t enc32_LS(uint32_t opc, uint8_t rd, uint8_t ra, int32_t imm15)
{
    return enc32_pack(((opc & 0x1F) << 25)
                    | ((uint32_t)(rd & 0x1F) << 20)
                    | ((uint32_t)(ra & 0x1F) << 15)
                    | ((uint32_t)imm15 & 0x7FFFu));
}

// J-form (opcode 2): Disp24[24:1], LK[0].
static inline uint32_t enc32_J(uint32_t disp24, bool lk)
{
    return enc32_pack((0x02u << 25) | ((disp24 & 0xFFFFFFu) << 1) | (lk ? 1u : 0u));
}

// BC-form (opcode 4): Disp19 split as Disp[18:9] -> op 24:15, Disp[8:0] -> op 9:1;
// EC[14:10], LK[0].
static inline uint32_t enc32_BC(uint8_t ec, int32_t disp19, bool lk)
{
    const uint32_t d = (uint32_t)disp19 & 0x7FFFFu;
    return enc32_pack((0x04u << 25)
                    | (((d >> 9) & 0x3FFu) << 15)
                    | ((uint32_t)(ec & 0x1F) << 10)
                    | ((d & 0x1FFu) << 1)
                    | (lk ? 1u : 0u));
}

// CR-form (opcode 6): rD, CR[19:15], CR_OP[7:0] (mtcr 0x00, mfcr 0x01, rte 0x84,
// drte 0xA4, sleep 0xC4 -- the last three from the PDF Form diagrams).
static inline uint32_t enc32_CR(uint8_t rd, uint8_t crn, uint32_t cr_op)
{
    return enc32_pack((0x06u << 25)
                    | ((uint32_t)(rd & 0x1F) << 20)
                    | ((uint32_t)(crn & 0x1F) << 15)
                    | (cr_op & 0xFFu));
}

// func6 values (plan section 2.4).
enum : uint32_t {
    EF6_NOP=0x00, EF6_SYSCALL=0x01, EF6_TRAPCOND=0x02, EF6_SDBBP=0x03, EF6_BRCOND=0x04, EF6_PFLUSH=0x05,
    EF6_ADDX=0x08, EF6_ADDCX=0x09, EF6_SUBX=0x0A, EF6_SUBCX=0x0B, EF6_CMP=0x0C, EF6_CMPZ=0x0D, EF6_NEGX=0x0F,
    EF6_ANDX=0x10, EF6_ORX=0x11, EF6_NOTX=0x12, EF6_XORX=0x13,
    EF6_BITCLR=0x14, EF6_BITSET=0x15, EF6_BITTST=0x16, EF6_BITTGL=0x17,
    EF6_SLLX=0x18, EF6_SRLX=0x1A, EF6_SRAX=0x1B, EF6_RORX=0x1C, EF6_RORC=0x1D, EF6_ROLX=0x1E, EF6_ROLC=0x1F,
    EF6_MUL=0x20, EF6_MULU=0x21, EF6_DIV=0x22, EF6_DIVU=0x23, EF6_MFCE=0x24, EF6_MTCE=0x25,
    EF6_MFSR=0x28, EF6_MTSR=0x29, EF6_TCOND=0x2A, EF6_MVCOND=0x2B,
    EF6_EXTSBX=0x2C, EF6_EXTSHX=0x2D, EF6_EXTZBX=0x2E, EF6_EXTZHX=0x2F,
    EF6_LCB=0x30, EF6_LCW=0x31, EF6_LCE=0x33, EF6_SCB=0x34, EF6_SCW=0x35, EF6_SCE=0x37,
    EF6_SLLIX=0x38, EF6_SRLIX=0x3A, EF6_SRAIX=0x3B, EF6_RORIX=0x3C, EF6_RORIC=0x3D, EF6_ROLIX=0x3E, EF6_ROLIC=0x3F
};

// Name-compatible shorthands used by the instruction encoders below.
static inline uint32_t encR(uint32_t func6, uint8_t d, uint8_t a, uint8_t b, bool cu)
{
    return enc32_special(d, a, b, func6, cu);
}
static inline uint32_t encI1(uint32_t func3, uint8_t d, uint16_t imm16, bool cu) { return enc32_I(0x01, d, func3, imm16, cu); }
static inline uint32_t encIS(uint32_t func3, uint8_t d, uint16_t imm16, bool cu) { return enc32_I(0x05, d, func3, imm16, cu); }
static inline uint32_t encLS(uint32_t opcode, uint8_t d, uint8_t a, uint16_t imm15) { return enc32_LS(opcode, d, a, imm15); }

// --- Special-form: arithmetic / logic ---
static inline uint32_t encodeANDX(uint8_t d, uint8_t a, uint8_t b, bool cu)  { return encR(EF6_ANDX, d, a, b, cu); }
static inline uint32_t encodeADDX(uint8_t d, uint8_t a, uint8_t b, bool cu)  { return encR(EF6_ADDX, d, a, b, cu); }
static inline uint32_t encodeADDCX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_ADDCX, d, a, b, cu); }
static inline uint32_t encodeSUBX(uint8_t d, uint8_t a, uint8_t b, bool cu)  { return encR(EF6_SUBX, d, a, b, cu); }
static inline uint32_t encodeSUBCX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_SUBCX, d, a, b, cu); }
static inline uint32_t encodeORX(uint8_t d, uint8_t a, uint8_t b, bool cu)   { return encR(EF6_ORX, d, a, b, cu); }
static inline uint32_t encodeXORX(uint8_t d, uint8_t a, uint8_t b, bool cu)  { return encR(EF6_XORX, d, a, b, cu); }
static inline uint32_t encodeNEGX(uint8_t d, uint8_t b, bool cu)             { return encR(EF6_NEGX, d, 0, b, cu); }
static inline uint32_t encodeNOTX(uint8_t d, uint8_t a, bool cu)             { return encR(EF6_NOTX, d, a, 0, cu); }
static inline uint32_t encodeNOP()                                           { return encR(EF6_NOP, 0, 0, 0, false); }
// cmp / cmpz carry their 2-bit TCS in the low bits of the rD field; both are .c forms.
static inline uint32_t encodeCMP(uint8_t a, uint8_t b, uint8_t tcs)         { return encR(EF6_CMP, tcs & 0x3, a, b, true); }
static inline uint32_t encodeCMPZ(uint8_t a, uint8_t tcs)                   { return encR(EF6_CMPZ, tcs & 0x3, a, 0, true); }
// mv{cond} / t{cond} carry their 4-bit EC in the rB field.
static inline uint32_t encodeMVCOND(uint8_t d, uint8_t a, uint8_t ec)       { return encR(EF6_MVCOND, d, a, ec & 0xF, false); }
static inline uint32_t encodeTCOND(uint8_t ec)                              { return encR(EF6_TCOND, 0, 0, ec & 0xF, false); }

// --- Special-form: bit operations (rA = register, rB = bit number) ---
static inline uint32_t encodeBITCLR(uint8_t a, uint8_t bn, bool cu) { return encR(EF6_BITCLR, 0, a, bn, cu); }
static inline uint32_t encodeBITSET(uint8_t a, uint8_t bn, bool cu) { return encR(EF6_BITSET, 0, a, bn, cu); }
static inline uint32_t encodeBITTST(uint8_t a, uint8_t bn, bool cu) { return encR(EF6_BITTST, 0, a, bn, cu); }
static inline uint32_t encodeBITTGL(uint8_t a, uint8_t bn, bool cu) { return encR(EF6_BITTGL, 0, a, bn, cu); }
static inline uint32_t encodeBITTSTC(uint8_t a, uint8_t bn, bool cu) { return encodeBITTST(a, bn, cu); }  // historical name

// --- Special-form: shifts / rotates (register and immediate) ---
static inline uint32_t encodeSLLX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_SLLX, d, a, b, cu); }
static inline uint32_t encodeSRLX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_SRLX, d, a, b, cu); }
static inline uint32_t encodeSRAX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_SRAX, d, a, b, cu); }
static inline uint32_t encodeROLX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_ROLX, d, a, b, cu); }
static inline uint32_t encodeRORX(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_RORX, d, a, b, cu); }
static inline uint32_t encodeROLC(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_ROLC, d, a, b, cu); }
static inline uint32_t encodeRORC(uint8_t d, uint8_t a, uint8_t b, bool cu) { return encR(EF6_RORC, d, a, b, cu); }
static inline uint32_t encodeSLLIX(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_SLLIX, d, a, sa, cu); }
static inline uint32_t encodeSRLIX(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_SRLIX, d, a, sa, cu); }
static inline uint32_t encodeSRAIX(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_SRAIX, d, a, sa, cu); }
static inline uint32_t encodeROLIX(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_ROLIX, d, a, sa, cu); }
static inline uint32_t encodeRORIX(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_RORIX, d, a, sa, cu); }
static inline uint32_t encodeROLIC(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_ROLIC, d, a, sa, cu); }
static inline uint32_t encodeRORIC(uint8_t d, uint8_t a, uint8_t sa, bool cu) { return encR(EF6_RORIC, d, a, sa, cu); }

// --- Special-form: extend ---
static inline uint32_t encodeEXTSBX(uint8_t d, uint8_t a, bool cu) { return encR(EF6_EXTSBX, d, a, 0, cu); }
static inline uint32_t encodeEXTSHX(uint8_t d, uint8_t a, bool cu) { return encR(EF6_EXTSHX, d, a, 0, cu); }
static inline uint32_t encodeEXTZBX(uint8_t d, uint8_t a, bool cu) { return encR(EF6_EXTZBX, d, a, 0, cu); }
static inline uint32_t encodeEXTZHX(uint8_t d, uint8_t a, bool cu) { return encR(EF6_EXTZHX, d, a, 0, cu); }

// --- Special-form: multiply / divide / CE moves ---
static inline uint32_t encodeMUL(uint8_t a, uint8_t b)  { return encR(EF6_MUL, 0, a, b, false); }
static inline uint32_t encodeMULU(uint8_t a, uint8_t b) { return encR(EF6_MULU, 0, a, b, false); }
static inline uint32_t encodeDIV(uint8_t a, uint8_t b)  { return encR(EF6_DIV, 0, a, b, false); }
static inline uint32_t encodeDIVU(uint8_t a, uint8_t b) { return encR(EF6_DIVU, 0, a, b, false); }
// rB selects the CE half: 1 = CEL, 2 = CEH, 3 = both (CEH to rD, CEL to rA for mfce;
// mtce reads rD for CEL/CEH-single, rD = CEH and rA = CEL for both). mtce takes its
// register in rD (PDF: "mtcel rD").
static inline uint32_t encodeMFCEL(uint8_t d) { return encR(EF6_MFCE, d, 0, 1, false); }
static inline uint32_t encodeMFCEH(uint8_t d) { return encR(EF6_MFCE, d, 0, 2, false); }
static inline uint32_t encodeMFCEHL(uint8_t hi, uint8_t lo) { return encR(EF6_MFCE, hi, lo, 3, false); }
static inline uint32_t encodeMTCEL(uint8_t r) { return encR(EF6_MTCE, r, 0, 1, false); }
static inline uint32_t encodeMTCEH(uint8_t r) { return encR(EF6_MTCE, r, 0, 2, false); }
static inline uint32_t encodeMTCEHL(uint8_t hi, uint8_t lo) { return encR(EF6_MTCE, hi, lo, 3, false); }

// --- Special-form: system ---
// trap{cond}: EC in rB. syscall/sdbbp32/pflush take no operands here.
static inline uint32_t encodeTRAPCOND(uint8_t ec) { return encR(EF6_TRAPCOND, 0, 0, ec, false); }
static inline uint32_t encodeSYSCALL()            { return encR(EF6_SYSCALL, 0, 0, 0, false); }
static inline uint32_t encodeSDBBP32()            { return encR(EF6_SDBBP, 0, 0, 0, false); }
static inline uint32_t encodePFLUSH()             { return encR(EF6_PFLUSH, 0, 0, 0, false); }
// mfsr/mtsr: func6 0x28/0x29 per the plan. Operand field assignment is UNVERIFIED: the
// existing tests put the GPR in rD (mfsr) / rA (mtsr) and the sr number in rA / rD, but
// MAME prints mfsr as "sr<rB>, r<rA>". Not cross-checked until plan story 7c.
static inline uint32_t encodeMFSR(uint8_t d, uint8_t srn)  { return encR(EF6_MFSR, d, srn, 0, false); }
static inline uint32_t encodeMTSR(uint8_t a, uint8_t srn)  { return encR(EF6_MTSR, srn, a, 0, false); }
// br{cond}[l]: EC in rB, target register in rA, LK in the CU bit.
static inline uint32_t encodeBR(uint8_t ec, uint8_t a)  { return encR(EF6_BRCOND, 0, a, ec & 0xF, false); }
static inline uint32_t encodeBRL(uint8_t ec, uint8_t a) { return encR(EF6_BRCOND, 0, a, ec & 0xF, true); }
// Load/store combine: rD = bits 25:21, rA (base) = bits 20:16; lcb/sce take only the base.
static inline uint32_t encodeLCB(uint8_t a)            { return encR(EF6_LCB, 0, a, 0, false); }
static inline uint32_t encodeLCW(uint8_t d, uint8_t a) { return encR(EF6_LCW, d, a, 0, false); }
static inline uint32_t encodeLCE(uint8_t d, uint8_t a) { return encR(EF6_LCE, d, a, 0, false); }
static inline uint32_t encodeSCB(uint8_t d, uint8_t a) { return encR(EF6_SCB, d, a, 0, false); }
static inline uint32_t encodeSCW(uint8_t d, uint8_t a) { return encR(EF6_SCW, d, a, 0, false); }
static inline uint32_t encodeSCE(uint8_t a)            { return encR(EF6_SCE, 0, a, 0, false); }

// --- I-form (opcode 1 and 5) ---
// Opcode 1: addi=0 cmpi=2 andi=4 ori=5 ldi=6.  Opcode 5: addis=0 cmpis=2 andis=4 oris=5 ldis=6.
static inline uint32_t encodeADDIX(uint8_t d, uint16_t imm16, bool cu)  { return encI1(0, d, imm16, cu); }
static inline uint32_t encodeANDIX(uint8_t d, uint16_t imm, bool cu)    { return encI1(4, d, imm, cu); }
static inline uint32_t encodeORIX(uint8_t d, uint16_t imm16, bool cu)   { return encI1(5, d, imm16, cu); }
static inline uint32_t encodeCMPI(uint8_t d, uint16_t imm16)            { return encI1(2, d, imm16, true); }
static inline uint32_t encodeLDI(uint8_t d, uint16_t imm16)             { return encI1(6, d, imm16, false); }
static inline uint32_t encodeADDISX(uint8_t d, uint16_t imm16, bool cu) { return encIS(0, d, imm16, cu); }
static inline uint32_t encodeCMPIS(uint8_t d, uint16_t imm16)           { return encIS(2, d, imm16, true); }
static inline uint32_t encodeANDISX(uint8_t d, uint16_t imm16, bool cu) { return encIS(4, d, imm16, cu); }
static inline uint32_t encodeORISX(uint8_t d, uint16_t imm16, bool cu)  { return encIS(5, d, imm16, cu); }
static inline uint32_t encodeLDIS(uint8_t d, uint16_t imm16)            { return encIS(6, d, imm16, false); }

// --- RI-form: addri 0x08, andri 0x0C, orri 0x0D ---
static inline uint32_t encodeADDRIX(uint8_t d, uint8_t a, uint16_t imm14, bool cu) { return enc32_RI(0x08, d, a, imm14, cu); }
static inline uint32_t encodeANDRIX(uint8_t d, uint8_t a, uint16_t imm14, bool cu) { return enc32_RI(0x0C, d, a, imm14, cu); }
static inline uint32_t encodeORRIX(uint8_t d, uint8_t a, uint16_t imm14, bool cu)  { return enc32_RI(0x0D, d, a, imm14, cu); }

// --- LS-form (opcodes 0x10-0x17): rD, [rA, SImm15]; 0x18 = cache ---
static inline uint32_t encodeLW(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x10, d, a, imm); }
static inline uint32_t encodeLH(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x11, d, a, imm); }
static inline uint32_t encodeLHU(uint8_t d, uint8_t a, uint16_t imm) { return encLS(0x12, d, a, imm); }
static inline uint32_t encodeLB(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x13, d, a, imm); }
static inline uint32_t encodeSW(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x14, d, a, imm); }
static inline uint32_t encodeSH(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x15, d, a, imm); }
static inline uint32_t encodeLBU(uint8_t d, uint8_t a, uint16_t imm) { return encLS(0x16, d, a, imm); }
static inline uint32_t encodeSB(uint8_t d, uint8_t a, uint16_t imm)  { return encLS(0x17, d, a, imm); }
// cache: the rD field carries Cache-OP; no operands here (cache_op 0, [r0, 0]).
static inline uint32_t encodeCACHE() { return enc32_LS(0x18, 0, 0, 0); }

// --- RIX-form pre-index (opcode 3: rA += SImm12, then access [rA]) and
//     post-index (opcode 7: access [rA], then rA += SImm12). func3: lw 0 lh 1 lhu 2 lb 3 sw 4 sh 5 lbu 6 sb 7. ---
static inline uint32_t encodeLW_pre(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x03, d, a, imm, 0); }
static inline uint32_t encodeLH_pre(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x03, d, a, imm, 1); }
static inline uint32_t encodeLHU_pre(uint8_t d, uint8_t a, int16_t imm) { return enc32_RIX(0x03, d, a, imm, 2); }
static inline uint32_t encodeLB_pre(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x03, d, a, imm, 3); }
static inline uint32_t encodeSW_pre(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x03, d, a, imm, 4); }
static inline uint32_t encodeSH_pre(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x03, d, a, imm, 5); }
static inline uint32_t encodeLBU_pre(uint8_t d, uint8_t a, int16_t imm) { return enc32_RIX(0x03, d, a, imm, 6); }
static inline uint32_t encodeSB_pre(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x03, d, a, imm, 7); }
static inline uint32_t encodeLW_post(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x07, d, a, imm, 0); }
static inline uint32_t encodeLH_post(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x07, d, a, imm, 1); }
static inline uint32_t encodeLHU_post(uint8_t d, uint8_t a, int16_t imm) { return enc32_RIX(0x07, d, a, imm, 2); }
static inline uint32_t encodeLB_post(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x07, d, a, imm, 3); }
static inline uint32_t encodeSW_post(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x07, d, a, imm, 4); }
static inline uint32_t encodeSH_post(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x07, d, a, imm, 5); }
static inline uint32_t encodeLBU_post(uint8_t d, uint8_t a, int16_t imm) { return enc32_RIX(0x07, d, a, imm, 6); }
static inline uint32_t encodeSB_post(uint8_t d, uint8_t a, int16_t imm)  { return enc32_RIX(0x07, d, a, imm, 7); }
// Older call-site names: `pre` selects opcode 3 (pre-index) or 7 (post-index).
static inline uint32_t encodeLW_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return pre ? encodeLW_pre(d, a, imm)  : encodeLW_post(d, a, imm); }
static inline uint32_t encodeSW_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return pre ? encodeSW_pre(d, a, imm)  : encodeSW_post(d, a, imm); }
static inline uint32_t encodeLBU_idx(uint8_t d, uint8_t a, int16_t imm, bool pre) { return pre ? encodeLBU_pre(d, a, imm) : encodeLBU_post(d, a, imm); }
static inline uint32_t encodeLB_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return pre ? encodeLB_pre(d, a, imm)  : encodeLB_post(d, a, imm); }
static inline uint32_t encodeLHU_idx(uint8_t d, uint8_t a, int16_t imm, bool pre) { return pre ? encodeLHU_pre(d, a, imm) : encodeLHU_post(d, a, imm); }
static inline uint32_t encodeLH_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return pre ? encodeLH_pre(d, a, imm)  : encodeLH_post(d, a, imm); }
static inline uint32_t encodeSB_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return pre ? encodeSB_pre(d, a, imm)  : encodeSB_post(d, a, imm); }
static inline uint32_t encodeSH_idx(uint8_t d, uint8_t a, int16_t imm, bool pre)  { return pre ? encodeSH_pre(d, a, imm)  : encodeSH_post(d, a, imm); }

// --- Control flow ---
// b{cond}[l]: opcode 4. j[l]: opcode 2 (Disp24 is the halfword offset field).
static inline uint32_t encodeBC(uint8_t bc, int32_t disp19, bool link) { return enc32_BC(bc, disp19, link); }
static inline uint32_t encodeBCOND(uint8_t bc, int32_t disp)  { return encodeBC(bc, disp, false); }
static inline uint32_t encodeBCONDL(uint8_t bc, int32_t disp) { return encodeBC(bc, disp, true); }
static inline uint32_t encodeJX(uint32_t disp24, bool lk)     { return enc32_J(disp24, lk); }

// --- CR-form (opcode 6) ---
static inline uint32_t encodeMTCR(uint8_t d, uint8_t crn) { return enc32_CR(d, crn, 0x00); }
static inline uint32_t encodeMFCR(uint8_t d, uint8_t crn) { return enc32_CR(d, crn, 0x01); }
static inline uint32_t encodeRTE()                        { return enc32_CR(0, 0, 0x84); }
static inline uint32_t encodeDRTE()                       { return enc32_CR(0, 0, 0xA4); }
static inline uint32_t encodeSLEEP()                      { return enc32_CR(0, 0, 0xC4); }

// ============================================================
// 16-bit ("!") instruction encoders (PDF 2.2; plan section 2.2).
// P-bit = bit 15 = 0, OP = bits 14:12. Each returns the raw 16-bit instruction.
// ============================================================

// OP 0 / OP 2: rD[11:8] rA[7:4] func4[3:0]
static inline uint16_t enc16_R(uint8_t op, uint8_t rd, uint8_t ra, uint8_t func4)
{
    return (uint16_t)(((op & 0x7) << 12) | ((rd & 0xF) << 8) | ((ra & 0xF) << 4) | (func4 & 0xF));
}
// OP 3: Disp11[11:1] LK[0]
static inline uint16_t enc16_J(uint16_t disp11, bool lk)
{
    return (uint16_t)((0x3 << 12) | ((disp11 & 0x7FF) << 1) | (lk ? 1 : 0));
}
// OP 4: EC[11:8] Disp8[7:0]
static inline uint16_t enc16_BX(uint8_t ec, int8_t disp8)
{
    return (uint16_t)((0x4 << 12) | ((ec & 0xF) << 8) | (uint8_t)disp8);
}
// OP 5: rD[11:8] Imm8[7:0]
static inline uint16_t enc16_I2(uint8_t rd, uint8_t imm8)
{
    return (uint16_t)((0x5 << 12) | ((rd & 0xF) << 8) | imm8);
}
// OP 6 / OP 7: rD[11:8] Imm5[7:3] func3[2:0]
static inline uint16_t enc16_I1(uint8_t op, uint8_t rd, uint8_t imm5, uint8_t func3)
{
    return (uint16_t)(((op & 0x7) << 12) | ((rd & 0xF) << 8) | ((imm5 & 0x1F) << 3) | (func3 & 0x7));
}
// push!/pop! (OP 2, func4 E/A): rDg[11:8] plus H = bit 7 (rDg bit 4), rAg[6:4] (r0-r7).
static inline uint16_t enc16_push(uint8_t rdg, uint8_t rag)
{
    return (uint16_t)((0x2 << 12) | ((rdg & 0xF) << 8) | (((rdg >> 4) & 1) << 7) | ((rag & 0x7) << 4) | 0xE);
}
static inline uint16_t enc16_pop(uint8_t rdg, uint8_t rag)
{
    return (uint16_t)((0x2 << 12) | ((rdg & 0xF) << 8) | (((rdg >> 4) & 1) << 7) | ((rag & 0x7) << 4) | 0xA);
}

// OP 0
static inline uint16_t encodeNOP16()                   { return enc16_R(0, 0, 0, 0x0); }
static inline uint16_t encodeMLFH16(uint8_t d, uint8_t a) { return enc16_R(0, d, a, 0x1); }  // rD = r(16+rA)
static inline uint16_t encodeMHFL16(uint8_t d, uint8_t a) { return enc16_R(0, d, a, 0x2); }  // r(16+rD) = rA
static inline uint16_t encodeMV16(uint8_t d, uint8_t a)   { return enc16_R(0, d, a, 0x3); }
static inline uint16_t encodeBRCOND16(uint8_t ec, uint8_t a)  { return enc16_R(0, ec, a, 0x4); }
static inline uint16_t encodeTCOND16(uint8_t ec)          { return enc16_R(0, ec, 0, 0x5); }
static inline uint16_t encodeSLL16(uint8_t d, uint8_t a)  { return enc16_R(0, d, a, 0x8); }
static inline uint16_t encodeADDC16(uint8_t d, uint8_t a) { return enc16_R(0, d, a, 0x9); }
static inline uint16_t encodeSRL16(uint8_t d, uint8_t a)  { return enc16_R(0, d, a, 0xA); }
static inline uint16_t encodeSRA16(uint8_t d, uint8_t a)  { return enc16_R(0, d, a, 0xB); }
static inline uint16_t encodeBRLCOND16(uint8_t ec, uint8_t a) { return enc16_R(0, ec, a, 0xC); }
// OP 2
static inline uint16_t encodeADD16(uint8_t d, uint8_t a)  { return enc16_R(2, d, a, 0x0); }
static inline uint16_t encodeSUB16(uint8_t d, uint8_t a)  { return enc16_R(2, d, a, 0x1); }
static inline uint16_t encodeNEG16(uint8_t d, uint8_t a)  { return enc16_R(2, d, a, 0x2); }
static inline uint16_t encodeCMP16(uint8_t d, uint8_t a)  { return enc16_R(2, d, a, 0x3); }
static inline uint16_t encodeAND16(uint8_t d, uint8_t a)  { return enc16_R(2, d, a, 0x4); }
static inline uint16_t encodeOR16(uint8_t d, uint8_t a)   { return enc16_R(2, d, a, 0x5); }
static inline uint16_t encodeNOT16(uint8_t d, uint8_t a)  { return enc16_R(2, d, a, 0x6); }
static inline uint16_t encodeXOR16(uint8_t d, uint8_t a)  { return enc16_R(2, d, a, 0x7); }
static inline uint16_t encodeLW16(uint8_t d, uint8_t a)   { return enc16_R(2, d, a, 0x8); }
static inline uint16_t encodeLH16(uint8_t d, uint8_t a)   { return enc16_R(2, d, a, 0x9); }
static inline uint16_t encodeLBU16(uint8_t d, uint8_t a)  { return enc16_R(2, d, a, 0xB); }
static inline uint16_t encodeSW16(uint8_t d, uint8_t a)   { return enc16_R(2, d, a, 0xC); }
static inline uint16_t encodeSH16(uint8_t d, uint8_t a)   { return enc16_R(2, d, a, 0xD); }
static inline uint16_t encodeSB16(uint8_t d, uint8_t a)   { return enc16_R(2, d, a, 0xF); }
// d = r0-r15 (or r16-r31 when `high`), a = r0-r7 base register
static inline uint16_t encodePOP16(uint8_t d, uint8_t a, bool high)  { return enc16_pop((uint8_t)((d & 0xF) | (high ? 0x10 : 0)), a); }
static inline uint16_t encodePUSH16(uint8_t d, uint8_t a, bool high) { return enc16_push((uint8_t)((d & 0xF) | (high ? 0x10 : 0)), a); }
// OP 3 / OP 4 / OP 5
static inline uint16_t encodeJX16(uint16_t disp11, bool lk)        { return enc16_J(disp11, lk); }
static inline uint16_t encodeBCOND16(uint8_t ec, int8_t disp8)     { return enc16_BX(ec, disp8); }
static inline uint16_t encodeB16(uint8_t ec, int8_t disp8)         { return enc16_BX(ec, disp8); }
static inline uint16_t encodeLDIU16(uint8_t d, uint8_t imm8)       { return enc16_I2(d, imm8); }
// OP 6: addei!/subei! take a 4-bit value (Imm5 bit 4 selects subtract); the rest take Imm5.
static inline uint16_t encodeADDEI16(uint8_t d, uint8_t imm4)  { return enc16_I1(6, d, imm4 & 0xF, 0); }
static inline uint16_t encodeSUBEI16(uint8_t d, uint8_t imm4)  { return enc16_I1(6, d, 0x10 | (imm4 & 0xF), 0); }
static inline uint16_t encodeSLLI16(uint8_t d, uint8_t sa5)    { return enc16_I1(6, d, sa5, 1); }
static inline uint16_t encodeSDBBP16(uint8_t d = 0, uint8_t imm5 = 0)  { return enc16_I1(6, d, imm5, 2); }
static inline uint16_t encodeSRLI16(uint8_t d, uint8_t sa5)    { return enc16_I1(6, d, sa5, 3); }
static inline uint16_t encodeBITCLR16(uint8_t d, uint8_t bn)   { return enc16_I1(6, d, bn, 4); }
static inline uint16_t encodeBITSET16(uint8_t d, uint8_t bn)   { return enc16_I1(6, d, bn, 5); }
static inline uint16_t encodeBITTST16(uint8_t d, uint8_t bn)   { return enc16_I1(6, d, bn, 6); }
static inline uint16_t encodeBITTGL16(uint8_t d, uint8_t bn)   { return enc16_I1(6, d, bn, 7); }
// OP 7: BP (r2)-relative. `imm5` is the raw field; the address offset is imm5<<2 (word),
// imm5<<1 (half) or imm5 (byte).
static inline uint16_t encodeLWP16(uint8_t d, uint8_t imm5)  { return enc16_I1(7, d, imm5, 0); }
static inline uint16_t encodeLHP16(uint8_t d, uint8_t imm5)  { return enc16_I1(7, d, imm5, 1); }
static inline uint16_t encodeLBUP16(uint8_t d, uint8_t imm5) { return enc16_I1(7, d, imm5, 3); }
static inline uint16_t encodeSWP16(uint8_t d, uint8_t imm5)  { return enc16_I1(7, d, imm5, 4); }
static inline uint16_t encodeSHP16(uint8_t d, uint8_t imm5)  { return enc16_I1(7, d, imm5, 5); }
static inline uint16_t encodeSBP16(uint8_t d, uint8_t imm5)  { return enc16_I1(7, d, imm5, 7); }
