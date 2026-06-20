#include "test_helpers.h"

// ============================================================
// Loads / stores. Effective address = GPRrA + sext(Imm15), used
// as a word index. rA holds the base address; data lives at the
// addressed slot (registers and memory share the bus RAM array).
// ============================================================

// --- LW : rD = Mem32[rA + imm] ---
TEST_F(CPUFixture, LW_Basic)
{
    setReg(2, 100u);          // base address
    setReg(100, 0xDEADBEEFu); // memory contents
    execute(encodeLW(4, 2, 0));
    EXPECT_EQ(getReg(4), 0xDEADBEEFu);
}

TEST_F(CPUFixture, LW_PositiveOffset)
{
    setReg(2, 100u);
    setReg(105, 0x12345678u);
    execute(encodeLW(4, 2, 5));
    EXPECT_EQ(getReg(4), 0x12345678u);
}

TEST_F(CPUFixture, LW_NegativeOffset)
{
    setReg(2, 100u);
    setReg(99, 0xCAFEF00Du);
    execute(encodeLW(4, 2, 0x7FFF)); // imm15 == -1
    EXPECT_EQ(getReg(4), 0xCAFEF00Du);
}

// --- SW : Mem32[rA + imm] = rD ---
TEST_F(CPUFixture, SW_Basic)
{
    setReg(4, 0x0000CAFEu); // value to store
    setReg(2, 200u);        // base address
    execute(encodeSW(4, 2, 0));
    EXPECT_EQ(getReg(200), 0x0000CAFEu);
}

// --- LBU / LB : load byte ---
TEST_F(CPUFixture, LBU_ZeroExtends)
{
    setReg(2, 100u);
    setReg(100, 0xDEADBEEFu);
    execute(encodeLBU(4, 2, 0));
    EXPECT_EQ(getReg(4), 0x000000EFu);
}

TEST_F(CPUFixture, LB_SignExtends)
{
    setReg(2, 100u);
    setReg(100, 0x000000FFu);
    execute(encodeLB(4, 2, 0));
    EXPECT_EQ(getReg(4), 0xFFFFFFFFu);
}

// --- LHU / LH : load half-word ---
TEST_F(CPUFixture, LHU_ZeroExtends)
{
    setReg(2, 100u);
    setReg(100, 0xDEADBEEFu);
    execute(encodeLHU(4, 2, 0));
    EXPECT_EQ(getReg(4), 0x0000BEEFu);
}

TEST_F(CPUFixture, LH_SignExtends)
{
    setReg(2, 100u);
    setReg(100, 0x0000FFFFu);
    execute(encodeLH(4, 2, 0));
    EXPECT_EQ(getReg(4), 0xFFFFFFFFu);
}

// --- SB / SH : store byte / half-word (read-modify-write) ---
TEST_F(CPUFixture, SB_PreservesUpperBytes)
{
    setReg(4, 0x000000ABu);
    setReg(2, 200u);
    setReg(200, 0x11223344u);
    execute(encodeSB(4, 2, 0));
    EXPECT_EQ(getReg(200), 0x112233ABu);
}

TEST_F(CPUFixture, SH_PreservesUpperHalf)
{
    setReg(4, 0x0000ABCDu);
    setReg(2, 200u);
    setReg(200, 0x11223344u);
    execute(encodeSH(4, 2, 0));
    EXPECT_EQ(getReg(200), 0x1122ABCDu);
}

// --- Round trip ---
TEST_F(CPUFixture, SW_then_LW_RoundTrip)
{
    setReg(4, 0x99887766u);
    setReg(2, 300u);
    execute(encodeSW(4, 2, 4));   // store to 304
    execute(encodeLW(5, 2, 4));   // load from 304
    EXPECT_EQ(getReg(5), 0x99887766u);
}
