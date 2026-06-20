#include "test_helpers.h"

// ============================================================
// ORIX  —  d = d | zext(imm16)   (I-type-1, func3=1)
// ============================================================

TEST_F(CPUFixture, ORIX_Basic)
{
    setReg(4, 0x00001200u);
    execute(encodeORIX(4, 0x0034u, false));
    EXPECT_EQ(getReg(4), 0x00001234u);
}

TEST_F(CPUFixture, ORIX_CU_SetsZFlagWhenZero)
{
    setReg(4, 0u);
    execute(encodeORIX(4, 0u, true));
    EXPECT_EQ(getReg(4), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

TEST_F(CPUFixture, ORIX_ZeroExtendsImmediate)
{
    setReg(4, 0u);
    execute(encodeORIX(4, 0xFFFFu, false));
    EXPECT_EQ(getReg(4), 0x0000FFFFu); // upper 16 bits stay zero
}

// ============================================================
// ORISX  —  d = d | (imm16 << 16)   (I-type-1, func3=7)
// ============================================================

TEST_F(CPUFixture, ORISX_Basic)
{
    setReg(4, 0x00001234u);
    execute(encodeORISX(4, 0xABCDu, false));
    EXPECT_EQ(getReg(4), 0xABCD1234u);
}

// ============================================================
// ORRIX  —  d = a | zext(imm14)   (RIX-type, opcode 0x0E)
// ============================================================

TEST_F(CPUFixture, ORRIX_Basic)
{
    setReg(5, 0x00000100u);
    execute(encodeORRIX(4, 5, 0x0023u, false));
    EXPECT_EQ(getReg(4), 0x00000123u);
}

// ============================================================
// SUBIX  —  d = d - sext(imm16)   (I-type-2, func3=1)
// ============================================================

TEST_F(CPUFixture, SUBIX_Basic)
{
    setReg(4, 100u);
    execute(encodeSUBIX(4, 30u, false));
    EXPECT_EQ(getReg(4), 70u);
}

TEST_F(CPUFixture, SUBIX_NegativeImmediateAddsBack)
{
    setReg(4, 100u);
    execute(encodeSUBIX(4, 0xFFFFu, false)); // imm = -1 -> 100 - (-1) = 101
    EXPECT_EQ(getReg(4), 101u);
}

TEST_F(CPUFixture, SUBIX_Zero_CUSet)
{
    setReg(4, 50u);
    execute(encodeSUBIX(4, 50u, true));
    EXPECT_EQ(getReg(4), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

// ============================================================
// SUBISX  —  d = d - (sext(imm16) << 16)   (I-type-2, func3=2)
// ============================================================

TEST_F(CPUFixture, SUBISX_Basic)
{
    setReg(4, 0x00050000u);
    execute(encodeSUBISX(4, 0x0001u, false));
    EXPECT_EQ(getReg(4), 0x00040000u);
}

// ============================================================
// SUBRIX  —  d = a - sext(imm14)   (RIX-type, opcode 0x0F)
// ============================================================

TEST_F(CPUFixture, SUBRIX_Basic)
{
    setReg(5, 100u);
    execute(encodeSUBRIX(4, 5, 10u, false));
    EXPECT_EQ(getReg(4), 90u);
}

TEST_F(CPUFixture, SUBRIX_NegativeImmediate)
{
    setReg(5, 100u);
    execute(encodeSUBRIX(4, 5, 0x3FFFu, false)); // imm14 = -1 -> 100 - (-1) = 101
    EXPECT_EQ(getReg(4), 101u);
}

// ============================================================
// CMPI  —  compare d - sext(imm16); sets N/Z/C/V/T  (I-type-1, func3=2)
// ============================================================

TEST_F(CPUFixture, CMPI_Equal_SetsZAndT)
{
    setReg(4, 50u);
    execute(encodeCMPI(4, 50u));
    EXPECT_EQ(getReg(4), 50u);          // operand unchanged
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::T), 1);   // TCS=00 -> T=Z
    EXPECT_EQ(getFlag(spg290::C), 1);
}

TEST_F(CPUFixture, CMPI_Less_SetsNClearsC)
{
    setReg(4, 50u);
    execute(encodeCMPI(4, 100u));
    EXPECT_EQ(getFlag(spg290::N), 1);
    EXPECT_EQ(getFlag(spg290::Z), 0);
    EXPECT_EQ(getFlag(spg290::C), 0); // 50 < 100 unsigned -> borrow
}

// ============================================================
// LDI  —  d = sext(imm16)   (I-type-1, func3=3)
// ============================================================

TEST_F(CPUFixture, LDI_Positive)
{
    execute(encodeLDI(4, 0x1234u));
    EXPECT_EQ(getReg(4), 0x00001234u);
}

TEST_F(CPUFixture, LDI_NegativeSignExtended)
{
    execute(encodeLDI(4, 0xFFFFu));
    EXPECT_EQ(getReg(4), 0xFFFFFFFFu);
}

// ============================================================
// LDIS  —  d = imm16 << 16   (I-type-2, func3=0)
// ============================================================

TEST_F(CPUFixture, LDIS_Basic)
{
    execute(encodeLDIS(4, 0x1234u));
    EXPECT_EQ(getReg(4), 0x12340000u);
}
