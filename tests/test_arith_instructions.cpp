#include "test_helpers.h"

// ============================================================
// ADDX  —  d = a + b   (R-type, func6=0x01)
// Flags (when CU): N=bit31, Z, C=carry-out, V=signed overflow.
// ============================================================

TEST_F(CPUFixture, ADDX_Basic)
{
    setReg(5, 10u);
    setReg(6, 20u);
    execute(encodeADDX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 30u);
}

TEST_F(CPUFixture, ADDX_CarryOut_CUSet)
{
    setReg(5, 0xFFFFFFFFu);
    setReg(6, 1u);
    execute(encodeADDX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0u);
    EXPECT_EQ(getFlag(spg290::C), 1);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

TEST_F(CPUFixture, ADDX_SignedOverflow_CUSet)
{
    setReg(5, 0x7FFFFFFFu); // INT_MAX
    setReg(6, 1u);
    execute(encodeADDX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0x80000000u);
    EXPECT_EQ(getFlag(spg290::V), 1);
    EXPECT_EQ(getFlag(spg290::N), 1);
}

TEST_F(CPUFixture, ADDX_NoCU_FlagsUntouched)
{
    setReg(5, 0xFFFFFFFFu);
    setReg(6, 1u);
    execute(encodeADDX(7, 5, 6, false)); // would carry, but CU=0
    EXPECT_EQ(getFlag(spg290::C), 0);
    EXPECT_EQ(getFlag(spg290::Z), 0);
}

// ============================================================
// SUBX  —  d = a - b   (R-type, func6=0x03)
// C = ~borrow (set when a >= b unsigned).
// ============================================================

TEST_F(CPUFixture, SUBX_Basic)
{
    setReg(5, 30u);
    setReg(6, 10u);
    execute(encodeSUBX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 20u);
}

TEST_F(CPUFixture, SUBX_Zero_CUSet)
{
    setReg(5, 10u);
    setReg(6, 10u);
    execute(encodeSUBX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1); // a >= b -> no borrow
}

TEST_F(CPUFixture, SUBX_Borrow_CUSet)
{
    setReg(5, 0u);
    setReg(6, 1u);
    execute(encodeSUBX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0xFFFFFFFFu);
    EXPECT_EQ(getFlag(spg290::C), 0); // a < b -> borrow
    EXPECT_EQ(getFlag(spg290::N), 1);
}

// ============================================================
// SUBCX  —  d = a - b - (~C)   (R-type, func6=0x04)
// ============================================================

TEST_F(CPUFixture, SUBCX_CarrySet_NoExtraBorrow)
{
    bus.cpu.status |= spg290::C; // C=1 -> ~C=0
    setReg(5, 20u);
    setReg(6, 5u);
    execute(encodeSUBCX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 15u);
}

TEST_F(CPUFixture, SUBCX_CarryClear_ExtraBorrow)
{
    // C=0 -> ~C=1, so result is a - b - 1
    setReg(5, 20u);
    setReg(6, 5u);
    execute(encodeSUBCX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 14u);
}

// ============================================================
// NEGX  —  d = 0 - b   (R-type, func6=0x05, source in rB)
// ============================================================

TEST_F(CPUFixture, NEGX_Basic)
{
    setReg(6, 5u);
    execute(encodeNEGX(7, 6, false));
    EXPECT_EQ(getReg(7), (uint32_t)(-5));
}

TEST_F(CPUFixture, NEGX_Zero_CUSet)
{
    setReg(6, 0u);
    execute(encodeNEGX(7, 6, true));
    EXPECT_EQ(getReg(7), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1); // ~borrow(0-0)
}

TEST_F(CPUFixture, NEGX_NegativeResult_CUSet)
{
    setReg(6, 1u);
    execute(encodeNEGX(7, 6, true));
    EXPECT_EQ(getReg(7), 0xFFFFFFFFu);
    EXPECT_EQ(getFlag(spg290::N), 1);
}

// ============================================================
// NOTX  —  d = ~a   (R-type, func6=0x13)
// ============================================================

TEST_F(CPUFixture, NOTX_Basic)
{
    setReg(5, 0x00000000u);
    execute(encodeNOTX(7, 5, false));
    EXPECT_EQ(getReg(7), 0xFFFFFFFFu);
}

TEST_F(CPUFixture, NOTX_Zero_CUSet)
{
    setReg(5, 0xFFFFFFFFu);
    execute(encodeNOTX(7, 5, true));
    EXPECT_EQ(getReg(7), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

// ============================================================
// XORX  —  d = a ^ b   (R-type, func6=0x12)
// ============================================================

TEST_F(CPUFixture, XORX_Basic)
{
    setReg(5, 0x0000FF00u);
    setReg(6, 0x00000FF0u);
    execute(encodeXORX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 0x0000F0F0u);
}

TEST_F(CPUFixture, XORX_Self_IsZero_CUSet)
{
    setReg(5, 0xDEADBEEFu);
    setReg(6, 0xDEADBEEFu);
    execute(encodeXORX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

// ============================================================
// NOP  —  no operation   (R-type, func6=0x00)
// ============================================================

TEST_F(CPUFixture, NOP_DoesNothing)
{
    setReg(7, 0x12345678u);
    uint32_t pcBefore = bus.cpu.pc;
    execute(encodeNOP());
    EXPECT_EQ(getReg(7), 0x12345678u); // unchanged
    EXPECT_EQ(bus.cpu.pc, pcBefore + 1); // pc still advanced past the instr
}
