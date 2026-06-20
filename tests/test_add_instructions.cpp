#include "test_helpers.h"

// All ADD instruction tests require their opcodes to be confirmed from
// the S+core7 ISA manual, added to encodeADD*() in test_helpers.h, and
// dispatched in spg290::clock() before the GTEST_SKIP() guard is removed.

// ============================================================
// ADDCX  —  d = a + b + carry
// ============================================================

TEST_F(CPUFixture, ADDCX_BasicAddNoCarry)
{
    GTEST_SKIP() << "ADDCX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(5, 10u);  // rA
    setReg(6, 20u);  // rB
    execute(encodeADDCX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 30u);
}

TEST_F(CPUFixture, ADDCX_AddWithCarryIn)
{
    GTEST_SKIP() << "ADDCX opcode not yet mapped — add opcode and dispatch to clock() first";
    // Pre-set the carry flag by executing a prior instruction that sets it,
    // or write to bus.cpu.status directly if the member is public.
    // (status is public in spg290.h)
    bus.cpu.status |= spg290::C; // force carry flag on
    setReg(5, 10u);
    setReg(6, 20u);
    execute(encodeADDCX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 31u); // 10 + 20 + 1 (carry)
}

TEST_F(CPUFixture, ADDCX_CarryOut_CUSet)
{
    GTEST_SKIP() << "ADDCX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(5, 0xFFFFFFFFu);
    setReg(6, 1u);
    execute(encodeADDCX(7, 5, 6, true)); // CU=1
    EXPECT_EQ(getReg(7), 0u);
    EXPECT_EQ(getFlag(spg290::C), 1);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

TEST_F(CPUFixture, ADDCX_NoCarryOut_CUSet)
{
    GTEST_SKIP() << "ADDCX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(5, 1u);
    setReg(6, 1u);
    execute(encodeADDCX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 2u);
    EXPECT_EQ(getFlag(spg290::C), 0);
    EXPECT_EQ(getFlag(spg290::Z), 0);
}

TEST_F(CPUFixture, ADDCX_NoCU_FlagsUntouched)
{
    GTEST_SKIP() << "ADDCX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(5, 0xFFFFFFFFu);
    setReg(6, 1u);
    execute(encodeADDCX(7, 5, 6, false)); // carry would set, but CU=0
    EXPECT_EQ(getFlag(spg290::C), 0);
}


// ============================================================
// ADDIX  —  d = d + sext(imm16)
// ============================================================

TEST_F(CPUFixture, ADDIX_PositiveImmediate)
{
    GTEST_SKIP() << "ADDIX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(12, 100u);
    execute(encodeADDIX(12, 50u, false));
    EXPECT_EQ(getReg(12), 150u);
}

// imm16 = 0xFFFF = -1 when sign-extended to 32 bits
TEST_F(CPUFixture, ADDIX_NegativeImmediateSignExtended)
{
    GTEST_SKIP() << "ADDIX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(12, 100u);
    execute(encodeADDIX(12, 0xFFFFu, false)); // imm = -1 signed
    EXPECT_EQ(getReg(12), 99u);
}

TEST_F(CPUFixture, ADDIX_CU_SetsZFlagWhenResultIsZero)
{
    GTEST_SKIP() << "ADDIX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(12, 1u);
    execute(encodeADDIX(12, 0xFFFFu, true)); // 1 + (-1) = 0
    EXPECT_EQ(getReg(12), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

TEST_F(CPUFixture, ADDIX_NoCU_FlagsUntouched)
{
    GTEST_SKIP() << "ADDIX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(12, 1u);
    execute(encodeADDIX(12, 0xFFFFu, false));
    EXPECT_EQ(getFlag(spg290::Z), 0);
}


// ============================================================
// ADDISX  —  d = d + (sext(imm16) << 16)
// ============================================================

TEST_F(CPUFixture, ADDISX_ShiftedAdd)
{
    GTEST_SKIP() << "ADDISX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(12, 0u);
    execute(encodeADDISX(12, 1u, false)); // adds 0x00010000
    EXPECT_EQ(getReg(12), 0x00010000u);
}

TEST_F(CPUFixture, ADDISX_CU_SetsZFlagWhenResultIsZero)
{
    GTEST_SKIP() << "ADDISX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(12, 0xFFFF0000u);
    execute(encodeADDISX(12, 0x0001u, true)); // 0xFFFF0000 + 0x00010000 = 0 with overflow
    EXPECT_EQ(getFlag(spg290::Z), 1);
}


// ============================================================
// ADDRIX  —  d = a + sext(imm14)
// ============================================================

TEST_F(CPUFixture, ADDRIX_PositiveImmediate)
{
    GTEST_SKIP() << "ADDRIX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 50u); // rA
    execute(encodeADDRIX(12, 10, 10u, false));
    EXPECT_EQ(getReg(12), 60u);
}

// imm14 = 0x3FFF = -1 when the 14-bit value is sign-extended
TEST_F(CPUFixture, ADDRIX_NegativeImmediateSignExtended)
{
    GTEST_SKIP() << "ADDRIX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 5u);
    // 14-bit -1 = 0x3FFF; sign-extended via (imm14 << 18) >> 18
    execute(encodeADDRIX(12, 10, 0x3FFFu, false));
    EXPECT_EQ(getReg(12), 4u); // 5 + (-1) = 4
}

TEST_F(CPUFixture, ADDRIX_CU_SetsZFlag)
{
    GTEST_SKIP() << "ADDRIX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0u);
    execute(encodeADDRIX(12, 10, 0u, true));
    EXPECT_EQ(getReg(12), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}
