#include "test_helpers.h"

// ============================================================
// EXTSBX  —  sign-extend low byte   (func6=0x30)
// ============================================================

TEST_F(CPUFixture, EXTSBX_NegativeByte)
{
    setReg(5, 0x000000FFu);
    execute(encodeEXTSBX(7, 5, false));
    EXPECT_EQ(getReg(7), 0xFFFFFFFFu);
}

TEST_F(CPUFixture, EXTSBX_PositiveByte)
{
    setReg(5, 0x0000007Fu);
    execute(encodeEXTSBX(7, 5, false));
    EXPECT_EQ(getReg(7), 0x0000007Fu);
}

TEST_F(CPUFixture, EXTSBX_IgnoresUpperBits)
{
    setReg(5, 0xDEADBE12u);
    execute(encodeEXTSBX(7, 5, false));
    EXPECT_EQ(getReg(7), 0x00000012u);
}

TEST_F(CPUFixture, EXTSBX_NFlagSet_CUSet)
{
    setReg(5, 0x00000080u); // sign bit of byte set
    execute(encodeEXTSBX(7, 5, true));
    EXPECT_EQ(getFlag(spg290::N), 1);
}

// ============================================================
// EXTSHX  —  sign-extend low half-word   (func6=0x31)
// ============================================================

TEST_F(CPUFixture, EXTSHX_NegativeHalf)
{
    setReg(5, 0x0000FFFFu);
    execute(encodeEXTSHX(7, 5, false));
    EXPECT_EQ(getReg(7), 0xFFFFFFFFu);
}

TEST_F(CPUFixture, EXTSHX_PositiveHalf)
{
    setReg(5, 0xDEAD7FFFu);
    execute(encodeEXTSHX(7, 5, false));
    EXPECT_EQ(getReg(7), 0x00007FFFu);
}

// ============================================================
// EXTZBX / EXTZHX  —  zero-extend   (func6=0x32 / 0x33)
// ============================================================

TEST_F(CPUFixture, EXTZBX_Basic)
{
    setReg(5, 0xFFFFFFFFu);
    execute(encodeEXTZBX(7, 5, false));
    EXPECT_EQ(getReg(7), 0x000000FFu);
}

TEST_F(CPUFixture, EXTZHX_Basic)
{
    setReg(5, 0xFFFFFFFFu);
    execute(encodeEXTZHX(7, 5, false));
    EXPECT_EQ(getReg(7), 0x0000FFFFu);
}

// ============================================================
// CLZ  —  count leading zeros   (func6=0x34)
// ============================================================

TEST_F(CPUFixture, CLZ_One)
{
    setReg(5, 0x00000001u);
    execute(encodeCLZ(7, 5));
    EXPECT_EQ(getReg(7), 31u);
}

TEST_F(CPUFixture, CLZ_HighBit)
{
    setReg(5, 0x80000000u);
    execute(encodeCLZ(7, 5));
    EXPECT_EQ(getReg(7), 0u);
}

TEST_F(CPUFixture, CLZ_Zero)
{
    setReg(5, 0x00000000u);
    execute(encodeCLZ(7, 5));
    EXPECT_EQ(getReg(7), 32u);
}

TEST_F(CPUFixture, CLZ_SixteenBits)
{
    setReg(5, 0x0000FFFFu);
    execute(encodeCLZ(7, 5));
    EXPECT_EQ(getReg(7), 16u);
}

// ============================================================
// ABS  —  absolute value   (func6=0x35)
// ============================================================

TEST_F(CPUFixture, ABS_Positive)
{
    setReg(5, 5u);
    execute(encodeABS(7, 5));
    EXPECT_EQ(getReg(7), 5u);
}

TEST_F(CPUFixture, ABS_Negative)
{
    setReg(5, (uint32_t)(-5));
    execute(encodeABS(7, 5));
    EXPECT_EQ(getReg(7), 5u);
}

TEST_F(CPUFixture, ABS_IntMinStaysIntMin)
{
    setReg(5, 0x80000000u);
    execute(encodeABS(7, 5));
    EXPECT_EQ(getReg(7), 0x80000000u); // -INT_MIN overflows back to itself
}

// ============================================================
// MIN / MAX  —  signed   (func6=0x36 / 0x37)
// ============================================================

TEST_F(CPUFixture, MIN_Positive)
{
    setReg(5, 3u);
    setReg(6, 5u);
    execute(encodeMIN(7, 5, 6));
    EXPECT_EQ(getReg(7), 3u);
}

TEST_F(CPUFixture, MIN_Signed)
{
    setReg(5, (uint32_t)(-1));
    setReg(6, 5u);
    execute(encodeMIN(7, 5, 6));
    EXPECT_EQ(getReg(7), (uint32_t)(-1)); // -1 < 5
}

TEST_F(CPUFixture, MAX_Positive)
{
    setReg(5, 3u);
    setReg(6, 5u);
    execute(encodeMAX(7, 5, 6));
    EXPECT_EQ(getReg(7), 5u);
}

TEST_F(CPUFixture, MAX_Signed)
{
    setReg(5, (uint32_t)(-1));
    setReg(6, 5u);
    execute(encodeMAX(7, 5, 6));
    EXPECT_EQ(getReg(7), 5u); // 5 > -1
}

// ============================================================
// BITREV  —  bit reverse then logical right shift   (func6=0x38)
// rB == r0 means shift amount 0.
// ============================================================

TEST_F(CPUFixture, BITREV_NoShift)
{
    setReg(5, 0x00000001u);
    execute(encodeBITREV(7, 5, 0)); // rB index 0 -> SA = 0
    EXPECT_EQ(getReg(7), 0x80000000u);
}

TEST_F(CPUFixture, BITREV_WithShift)
{
    setReg(5, 0x00000001u);
    setReg(6, 4u);
    execute(encodeBITREV(7, 5, 6)); // SA = 4
    EXPECT_EQ(getReg(7), 0x08000000u);
}

TEST_F(CPUFixture, BITREV_FullPattern)
{
    setReg(5, 0x0000000Fu);
    execute(encodeBITREV(7, 5, 0));
    EXPECT_EQ(getReg(7), 0xF0000000u);
}
