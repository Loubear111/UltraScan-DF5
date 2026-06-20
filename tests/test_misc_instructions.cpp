#include "test_helpers.h"

// ORX and BITTSTC tests require their opcodes to be confirmed from the
// S+core7 ISA manual before the GTEST_SKIP() guards can be removed.

// ============================================================
// ORX  —  d = a | b
// ============================================================

TEST_F(CPUFixture, ORX_BasicResult)
{
    GTEST_SKIP() << "ORX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0xF0F0F0F0u);
    setReg(11, 0x0F0F0F0Fu);
    execute(encodeORX(12, 10, 11, false));
    EXPECT_EQ(getReg(12), 0xFFFFFFFFu);
}

TEST_F(CPUFixture, ORX_NoChange_WhenOrWithZero)
{
    GTEST_SKIP() << "ORX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0xDEADBEEFu);
    setReg(11, 0x00000000u);
    execute(encodeORX(12, 10, 11, false));
    EXPECT_EQ(getReg(12), 0xDEADBEEFu);
}

TEST_F(CPUFixture, ORX_CU_SetsZFlagWhenBothZero)
{
    GTEST_SKIP() << "ORX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0u);
    setReg(11, 0u);
    execute(encodeORX(12, 10, 11, true));
    EXPECT_EQ(getReg(12), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

TEST_F(CPUFixture, ORX_CU_ClearsZFlagWhenResultNonZero)
{
    GTEST_SKIP() << "ORX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0x1u);
    setReg(11, 0x0u);
    execute(encodeORX(12, 10, 11, true));
    EXPECT_EQ(getFlag(spg290::Z), 0);
}

// N flag — same inverted convention as ANDX: N set when bit 31 == 0
TEST_F(CPUFixture, ORX_CU_NFlagSetWhenMSBisClear)
{
    GTEST_SKIP() << "ORX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0x7FFFFFFFu);
    setReg(11, 0x00000000u);
    execute(encodeORX(12, 10, 11, true));
    EXPECT_EQ(getFlag(spg290::N), 1); // bit 31 == 0 → N set per current impl
}

TEST_F(CPUFixture, ORX_NoCU_FlagsUntouched)
{
    GTEST_SKIP() << "ORX opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0u);
    setReg(11, 0u);
    execute(encodeORX(12, 10, 11, false)); // result=0, CU=0
    EXPECT_EQ(getFlag(spg290::Z), 0);
}


// ============================================================
// BITTSTC  —  test bit BN of rA; always writes flags (.c suffix)
// Z is set if the tested bit is 0; N reflects the sign bit.
// ============================================================

TEST_F(CPUFixture, BITTSTC_TestedBitIsOne_ZFlagClear)
{
    GTEST_SKIP() << "BITTSTC opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0x1u); // bit 0 is set
    execute(encodeBITTSTC(10, 0, true));
    EXPECT_EQ(getFlag(spg290::Z), 0);
}

TEST_F(CPUFixture, BITTSTC_TestedBitIsZero_ZFlagSet)
{
    GTEST_SKIP() << "BITTSTC opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0x0u); // bit 0 is clear
    execute(encodeBITTSTC(10, 0, true));
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

TEST_F(CPUFixture, BITTSTC_TestBit31_NFlagSet)
{
    GTEST_SKIP() << "BITTSTC opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0x80000000u); // bit 31 set
    execute(encodeBITTSTC(10, 31, true));
    EXPECT_EQ(getFlag(spg290::Z), 0); // bit is 1 → Z clear
    EXPECT_EQ(getFlag(spg290::N), 1); // sign bit tested and set → N set
}

TEST_F(CPUFixture, BITTSTC_MidBit_BothFlagsCorrect)
{
    GTEST_SKIP() << "BITTSTC opcode not yet mapped — add opcode and dispatch to clock() first";
    setReg(10, 0x00001000u); // bit 12 set, bit 31 clear
    execute(encodeBITTSTC(10, 12, true));
    EXPECT_EQ(getFlag(spg290::Z), 0); // bit is 1 → Z clear
    EXPECT_EQ(getFlag(spg290::N), 0); // bit 31 not the tested bit → N per impl
}
