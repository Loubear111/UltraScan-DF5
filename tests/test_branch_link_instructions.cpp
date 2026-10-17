#include "test_helpers.h"

// ============================================================
// Branch-and-link instructions. The link register r3 always receives
// the address of the following instruction (pc, already advanced by
// instrSize=4 for these 32-bit forms), regardless of whether the branch
// condition is taken. Displacements are halfword-granular (disp<<1).
// ============================================================

// --- B{cond}L : conditional relative branch and link  (opcode 0x1D) ---
TEST_F(CPUFixture, BCONDL_TakenWritesLink)
{
    bus.cpu.status |= spg290::Z;          // EQ holds
    execute(encodeBCONDL(4, 5));          // beql +5 halfwords at pc=0
    EXPECT_EQ(bus.cpu.pc, 10u);
    EXPECT_EQ(getReg(3), 4u);             // link = following instruction
}

TEST_F(CPUFixture, BCONDL_NotTakenStillWritesLink)
{
    // Z clear -> EQ false -> branch not taken, but link is still written.
    execute(encodeBCONDL(4, 5));
    EXPECT_EQ(bus.cpu.pc, 4u);
    EXPECT_EQ(getReg(3), 4u);
}

TEST_F(CPUFixture, BCONDL_AlwaysBranches)
{
    execute(encodeBCONDL(15, 10));        // bl (always)
    EXPECT_EQ(bus.cpu.pc, 20u);
    EXPECT_EQ(getReg(3), 4u);
}

TEST_F(CPUFixture, BCONDL_NegativeDisplacement)
{
    bus.cpu.pc = 10;
    bus.cpu.status |= spg290::Z;
    execute(encodeBCONDL(4, -5));         // beql -5 halfwords -> 10 - 10
    EXPECT_EQ(bus.cpu.pc, 0u);
    EXPECT_EQ(getReg(3), 14u);            // following instruction was at byte 14
}

// --- BR{cond}L : conditional branch to register and link  (opcode 0x1E) ---
TEST_F(CPUFixture, BRL_TakenWritesLink)
{
    setReg(5, 42u);
    bus.cpu.status |= spg290::Z;
    execute(encodeBRL(4, 5));
    EXPECT_EQ(bus.cpu.pc, 42u);
    EXPECT_EQ(getReg(3), 4u);
}

TEST_F(CPUFixture, BRL_NotTakenStillWritesLink)
{
    setReg(5, 42u);
    execute(encodeBRL(4, 5));             // Z clear -> not taken
    EXPECT_EQ(bus.cpu.pc, 4u);
    EXPECT_EQ(getReg(3), 4u);
}
