#include "test_helpers.h"

// ============================================================
// CMP  —  compute rA - rB, set N/Z/C/V (+T per TCS), no GPR write.
// func6=0x06, TCS in low bits of rD field. TCS=00 -> T=Z, 01 -> T=N.
// ============================================================

TEST_F(CPUFixture, CMP_Equal)
{
    setReg(5, 10u);
    setReg(6, 10u);
    execute(encodeCMP(5, 6, 0));
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1); // a >= b
    EXPECT_EQ(getFlag(spg290::T), 1); // TCS=00 -> T=Z
}

TEST_F(CPUFixture, CMP_Less)
{
    setReg(5, 5u);
    setReg(6, 10u);
    execute(encodeCMP(5, 6, 0));
    EXPECT_EQ(getFlag(spg290::N), 1);
    EXPECT_EQ(getFlag(spg290::Z), 0);
    EXPECT_EQ(getFlag(spg290::C), 0); // borrow
    EXPECT_EQ(getFlag(spg290::T), 0);
}

TEST_F(CPUFixture, CMP_Greater)
{
    setReg(5, 10u);
    setReg(6, 5u);
    execute(encodeCMP(5, 6, 0));
    EXPECT_EQ(getFlag(spg290::Z), 0);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

TEST_F(CPUFixture, CMP_TCS1_TtracksN)
{
    setReg(5, 5u);
    setReg(6, 10u);
    execute(encodeCMP(5, 6, 1)); // TCS=01 -> T=N
    EXPECT_EQ(getFlag(spg290::N), 1);
    EXPECT_EQ(getFlag(spg290::T), 1);
}

TEST_F(CPUFixture, CMP_DoesNotModifyRegisters)
{
    setReg(5, 10u);
    setReg(6, 5u);
    execute(encodeCMP(5, 6, 0));
    EXPECT_EQ(getReg(5), 10u);
    EXPECT_EQ(getReg(6), 5u);
}

// ============================================================
// CMPZ  —  compare rA with 0   (func6=0x07)
// ============================================================

TEST_F(CPUFixture, CMPZ_Zero)
{
    setReg(5, 0u);
    execute(encodeCMPZ(5, 0));
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::T), 1);
    EXPECT_EQ(getFlag(spg290::C), 1);
    EXPECT_EQ(getFlag(spg290::V), 0);
}

TEST_F(CPUFixture, CMPZ_Negative)
{
    setReg(5, 0x80000000u);
    execute(encodeCMPZ(5, 0));
    EXPECT_EQ(getFlag(spg290::N), 1);
    EXPECT_EQ(getFlag(spg290::Z), 0);
}

TEST_F(CPUFixture, CMPZ_TCS1_TtracksN)
{
    setReg(5, 0x80000000u);
    execute(encodeCMPZ(5, 1));
    EXPECT_EQ(getFlag(spg290::T), 1);
}

// ============================================================
// MVCOND  —  conditional move   (func6=0x08, EC in low bits of rB field)
// EC: EQ=4, NE=5, MI=10, AL=15.
// ============================================================

TEST_F(CPUFixture, MVCOND_EQ_True)
{
    bus.cpu.status |= spg290::Z; // condition EQ holds
    setReg(5, 0xAAAAAAAAu);
    setReg(7, 0x11111111u);
    execute(encodeMVCOND(7, 5, 4)); // mveq r7, r5
    EXPECT_EQ(getReg(7), 0xAAAAAAAAu);
}

TEST_F(CPUFixture, MVCOND_EQ_False)
{
    // Z is clear -> EQ false -> destination unchanged
    setReg(5, 0xAAAAAAAAu);
    setReg(7, 0x11111111u);
    execute(encodeMVCOND(7, 5, 4));
    EXPECT_EQ(getReg(7), 0x11111111u);
}

TEST_F(CPUFixture, MVCOND_NE_True)
{
    // Z clear -> NE true
    setReg(5, 0xAAAAAAAAu);
    setReg(7, 0x11111111u);
    execute(encodeMVCOND(7, 5, 5)); // mvne
    EXPECT_EQ(getReg(7), 0xAAAAAAAAu);
}

TEST_F(CPUFixture, MVCOND_MI_TrueWhenNegative)
{
    bus.cpu.status |= spg290::N;
    setReg(5, 0xAAAAAAAAu);
    setReg(7, 0x11111111u);
    execute(encodeMVCOND(7, 5, 10)); // mvmi
    EXPECT_EQ(getReg(7), 0xAAAAAAAAu);
}

TEST_F(CPUFixture, MVCOND_AL_AlwaysMoves)
{
    setReg(5, 0xAAAAAAAAu);
    setReg(7, 0x11111111u);
    execute(encodeMVCOND(7, 5, 15)); // mv (always)
    EXPECT_EQ(getReg(7), 0xAAAAAAAAu);
}
