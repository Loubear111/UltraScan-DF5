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

    // Write a value into the RAM slot that acts as a register
    void setReg(uint8_t reg, uint32_t val)
    {
        bus.ram[reg] = val;
    }

    // Read back a register value
    uint32_t getReg(uint8_t reg)
    {
        return bus.ram[reg];
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
// Encoders below require their opcodes to be confirmed from the
// S+core7 ISA manual before they can be used. Each is guarded
// in the corresponding test file with GTEST_SKIP() until the
// opcode value is filled in and dispatch is added to clock().
//
// Replace OPCODE_UNKNOWN with the correct 5-bit value and
// remove the GTEST_SKIP() call in the test.
// ============================================================
static constexpr uint32_t OPCODE_UNKNOWN = 0x1Fu; // will never match a valid dispatch

// --- ANDISX : d = d & (imm16 << 16)  (I-type) ---
static inline uint32_t encodeANDISX(uint8_t d, uint16_t imm16, bool cu)
{
    return 0x80000000u
         | (OPCODE_UNKNOWN << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)imm16)
         | (cu ? 1u : 0u);
}

// --- ADDCX : d = a + b + carry  (R-type) ---
static inline uint32_t encodeADDCX(uint8_t d, uint8_t a, uint8_t b, bool cu)
{
    return 0x80000000u
         | (OPCODE_UNKNOWN << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(b & 0x1F) << 10)
         | (cu ? 1u : 0u);
}

// --- ADDIX : d = d + sext(imm16)  (I-type) ---
static inline uint32_t encodeADDIX(uint8_t d, uint16_t imm16, bool cu)
{
    return 0x80000000u
         | (OPCODE_UNKNOWN << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)imm16)
         | (cu ? 1u : 0u);
}

// --- ADDISX : d = d + (sext(imm16) << 16)  (I-type) ---
static inline uint32_t encodeADDISX(uint8_t d, uint16_t imm16, bool cu)
{
    return 0x80000000u
         | (OPCODE_UNKNOWN << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)imm16)
         | (cu ? 1u : 0u);
}

// --- ADDRIX : d = a + sext(imm14)  (RI-type) ---
static inline uint32_t encodeADDRIX(uint8_t d, uint8_t a, uint16_t imm14, bool cu)
{
    return 0x80000000u
         | (OPCODE_UNKNOWN << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(imm14 & 0x3FFF))
         | (cu ? 1u : 0u);
}

// --- ORX : d = a | b  (R-type) ---
static inline uint32_t encodeORX(uint8_t d, uint8_t a, uint8_t b, bool cu)
{
    return 0x80000000u
         | (OPCODE_UNKNOWN << 26)
         | ((uint32_t)(d & 0x1F) << 21)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(b & 0x1F) << 10)
         | (cu ? 1u : 0u);
}

// --- BITTSTC : test bit BN of rA, always updates flags  (R-type) ---
static inline uint32_t encodeBITTSTC(uint8_t a, uint8_t bn, bool cu)
{
    return 0x80000000u
         | (OPCODE_UNKNOWN << 26)
         | ((uint32_t)(a & 0x1F) << 16)
         | ((uint32_t)(bn & 0x1F) << 10)
         | (cu ? 1u : 0u);
}
