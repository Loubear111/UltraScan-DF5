#include "test_helpers.h"

// ============================================================
// Saturating arithmetic. The true (wider) result is clamped to the
// signed 32-bit range [INT32_MIN, INT32_MAX] = [0x80000000, 0x7FFFFFFF].
// These instructions do not update the condition flags.
// ============================================================

// --- ADD.S : signed saturating add  (func6=0x1B) ---
TEST_F(CPUFixture, ADDS_NoOverflow)
{
    setReg(5, 5u);
    setReg(6, 3u);
    execute(encodeADDS(7, 5, 6));
    EXPECT_EQ(getReg(7), 8u);
}

TEST_F(CPUFixture, ADDS_PositiveOverflowClamps)
{
    setReg(5, 0x7FFFFFFFu);   // INT_MAX
    setReg(6, 1u);
    execute(encodeADDS(7, 5, 6));
    EXPECT_EQ(getReg(7), 0x7FFFFFFFu);
}

TEST_F(CPUFixture, ADDS_NegativeOverflowClamps)
{
    setReg(5, 0x80000000u);   // INT_MIN
    setReg(6, 0xFFFFFFFFu);   // -1
    execute(encodeADDS(7, 5, 6));
    EXPECT_EQ(getReg(7), 0x80000000u);
}

// --- SUB.S : signed saturating subtract  (func6=0x1C) ---
TEST_F(CPUFixture, SUBS_NoOverflow)
{
    setReg(5, 10u);
    setReg(6, 4u);
    execute(encodeSUBS(7, 5, 6));
    EXPECT_EQ(getReg(7), 6u);
}

TEST_F(CPUFixture, SUBS_PositiveOverflowClamps)
{
    setReg(5, 0x7FFFFFFFu);   // INT_MAX
    setReg(6, 0xFFFFFFFFu);   // -1  -> INT_MAX - (-1) overflows
    execute(encodeSUBS(7, 5, 6));
    EXPECT_EQ(getReg(7), 0x7FFFFFFFu);
}

TEST_F(CPUFixture, SUBS_NegativeOverflowClamps)
{
    setReg(5, 0x80000000u);   // INT_MIN
    setReg(6, 1u);
    execute(encodeSUBS(7, 5, 6));
    EXPECT_EQ(getReg(7), 0x80000000u);
}

// --- ABS.S : signed saturating absolute value  (func6=0x1D) ---
TEST_F(CPUFixture, ABSS_Positive)
{
    setReg(5, 5u);
    execute(encodeABSS(7, 5));
    EXPECT_EQ(getReg(7), 5u);
}

TEST_F(CPUFixture, ABSS_Negative)
{
    setReg(5, 0xFFFFFFFFu);   // -1
    execute(encodeABSS(7, 5));
    EXPECT_EQ(getReg(7), 1u);
}

TEST_F(CPUFixture, ABSS_IntMinClamps)
{
    setReg(5, 0x80000000u);   // INT_MIN -> abs overflows -> INT_MAX
    execute(encodeABSS(7, 5));
    EXPECT_EQ(getReg(7), 0x7FFFFFFFu);
}

// --- SLL.S : signed saturating shift left  (func6=0x1E) ---
TEST_F(CPUFixture, SLLS_NoOverflow)
{
    setReg(5, 1u);
    setReg(6, 4u);
    execute(encodeSLLS(7, 5, 6));
    EXPECT_EQ(getReg(7), 0x10u);
}

TEST_F(CPUFixture, SLLS_PositiveOverflowClamps)
{
    setReg(5, 0x40000000u);
    setReg(6, 1u);            // 0x40000000 << 1 = 0x80000000 -> overflow
    execute(encodeSLLS(7, 5, 6));
    EXPECT_EQ(getReg(7), 0x7FFFFFFFu);
}

TEST_F(CPUFixture, SLLS_NegativeShiftKeepsSign)
{
    setReg(5, 0xFFFFFFFFu);   // -1
    setReg(6, 1u);            // -1 << 1 = -2, fits
    execute(encodeSLLS(7, 5, 6));
    EXPECT_EQ(getReg(7), 0xFFFFFFFEu);
}

TEST_F(CPUFixture, SLLS_NegativeOverflowClamps)
{
    setReg(5, 0x80000000u);   // INT_MIN
    setReg(6, 1u);            // shifts below INT_MIN -> clamp INT_MIN
    execute(encodeSLLS(7, 5, 6));
    EXPECT_EQ(getReg(7), 0x80000000u);
}
