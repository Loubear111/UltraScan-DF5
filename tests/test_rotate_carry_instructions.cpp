#include "test_helpers.h"

// ============================================================
// Rotate-through-carry instructions. The 33-bit rotate chain is
// {rA, C}: C participates as an extra bit between bit 0 and bit 31.
// Rotate amount is the low 5 bits of rB (register) or SA (immediate).
// N is always updated; C is updated only when the amount is non-zero.
// ============================================================

// --- ROLC : rotate left through carry (register)  (func6=0x17) ---
TEST_F(CPUFixture, ROLC_ByOne_CarryClear)
{
    setReg(5, 0x80000001u);
    setReg(6, 1u);
    // C starts clear. rD = {rA[30:0], C} = (0x00000001<<1)|0 = 2; new C = rA[31] = 1.
    execute(encodeROLC(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0x00000002u);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

TEST_F(CPUFixture, ROLC_ByOne_CarrySet)
{
    bus.cpu.status |= spg290::C;     // C = 1
    setReg(5, 0x00000000u);
    setReg(6, 1u);
    // rD = {0[30:0], C=1} = 1; new C = rA[31] = 0.
    execute(encodeROLC(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0x00000001u);
    EXPECT_EQ(getFlag(spg290::C), 0);
}

TEST_F(CPUFixture, ROLC_ZeroAmountIsCopy_CarryUnchanged)
{
    bus.cpu.status |= spg290::C;     // C = 1 and must stay 1
    setReg(5, 0xDEADBEEFu);
    setReg(6, 0u);
    execute(encodeROLC(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0xDEADBEEFu);
    EXPECT_EQ(getFlag(spg290::C), 1);     // C unchanged when amount == 0
    EXPECT_EQ(getFlag(spg290::N), 1);     // bit 31 set
}

TEST_F(CPUFixture, ROLC_MultiBit)
{
    bus.cpu.status |= spg290::C;          // C = 1
    setReg(5, 0x40000000u);
    setReg(6, 2u);
    // chain W = {rA,C}; rotate left 2. Expected via spec:
    // rD = {rA[29:0], C, rA[31:31]} = (0x40000000<<2)|(C<<1)|rA[31]
    //    = 0x00000000 | 0x2 | 0x0 = 0x00000002; newC = rA[30] = 1.
    execute(encodeROLC(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0x00000002u);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

// --- ROLIC : rotate left through carry (immediate)  (func6=0x18) ---
TEST_F(CPUFixture, ROLIC_ByOne)
{
    setReg(5, 0x80000000u);
    // C=0; rD = {rA[30:0]=0, C=0} = 0; newC = rA[31] = 1.
    execute(encodeROLIC(7, 5, 1u, true));
    EXPECT_EQ(getReg(7), 0u);
    EXPECT_EQ(getFlag(spg290::C), 1);
    EXPECT_EQ(getFlag(spg290::Z), 0); // ROLC does not update Z
}

// --- RORC : rotate right through carry (register)  (func6=0x19) ---
TEST_F(CPUFixture, RORC_ByOne_CarryClear)
{
    setReg(5, 0x00000001u);
    setReg(6, 1u);
    // rD = {C=0, rA[31:1]=0} = 0; newC = rA[0] = 1.
    execute(encodeRORC(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0u);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

TEST_F(CPUFixture, RORC_ByOne_CarrySet)
{
    bus.cpu.status |= spg290::C;     // C = 1
    setReg(5, 0x00000002u);
    setReg(6, 1u);
    // rD = {C=1, rA[31:1]=0x00000001} = 0x80000001; newC = rA[0] = 0.
    execute(encodeRORC(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0x80000001u);
    EXPECT_EQ(getFlag(spg290::C), 0);
    EXPECT_EQ(getFlag(spg290::N), 1);
}

// --- RORIC : rotate right through carry (immediate)  (func6=0x1A) ---
TEST_F(CPUFixture, RORIC_ByOne)
{
    bus.cpu.status |= spg290::C;     // C = 1
    setReg(5, 0x00000003u);
    // rD = {C=1, rA[31:1]=0x00000001} = 0x80000001; newC = rA[0] = 1.
    execute(encodeRORIC(7, 5, 1u, true));
    EXPECT_EQ(getReg(7), 0x80000001u);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

TEST_F(CPUFixture, RORIC_ZeroAmountIsCopy)
{
    setReg(5, 0x12345678u);
    execute(encodeRORIC(7, 5, 0u, true));
    EXPECT_EQ(getReg(7), 0x12345678u);
}
