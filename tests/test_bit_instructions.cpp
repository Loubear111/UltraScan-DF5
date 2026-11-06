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

// ============================================================
// ABS  —  absolute value   (func6=0x35)
// ============================================================

// ============================================================
// MIN / MAX  —  signed   (func6=0x36 / 0x37)
// ============================================================

// ============================================================
// BITREV  —  bit reverse then logical right shift   (func6=0x38)
// rB == r0 means shift amount 0.
// ============================================================

