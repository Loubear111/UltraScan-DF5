#include "test_helpers.h"

// ============================================================
// Control-flow instructions. pc is a real byte address; a 32-bit
// instruction is 4 bytes (instrSize=4). By the time an instruction
// executes, pc has already advanced past it, so PCcurrent == (pc - 4).
// Displacements are halfword-granular (disp<<1), per s_core7.pdf.
// Branch condition (BC) encoding matches the EC table (EQ=4,
// NE=5, MI=10, AL=15, ...).
// ============================================================

// --- BCOND : conditional relative branch  (opcode 0x18) ---
TEST_F(CPUFixture, BCOND_TakenWhenEQ)
{
    bus.cpu.status |= spg290::Z;          // EQ condition holds
    execute(encodeBCOND(4, 5));           // beq +5 halfwords, executed at pc=0
    EXPECT_EQ(bus.cpu.pc, 10u);
}

TEST_F(CPUFixture, BCOND_NotTakenFallsThrough)
{
    // Z clear -> EQ false -> pc just advances to the next instruction
    execute(encodeBCOND(4, 5));
    EXPECT_EQ(bus.cpu.pc, 4u);
}

TEST_F(CPUFixture, BCOND_AlwaysBranches)
{
    execute(encodeBCOND(15, 10));         // b (always)
    EXPECT_EQ(bus.cpu.pc, 20u);
}

TEST_F(CPUFixture, BCOND_NegativeDisplacement)
{
    bus.cpu.pc = 10;                      // place the branch at byte 10
    bus.cpu.status |= spg290::Z;
    execute(encodeBCOND(4, -5));          // beq -5 halfwords -> 10 - 10
    EXPECT_EQ(bus.cpu.pc, 0u);
}

// --- JX : absolute jump (and link)  (opcode 0x19) ---
TEST_F(CPUFixture, JX_Jump)
{
    execute(encodeJX(20, false));
    EXPECT_EQ(bus.cpu.pc, 40u);
}

TEST_F(CPUFixture, JX_LinkWritesR3)
{
    execute(encodeJX(20, true));          // jl: r3 = address after the jump
    EXPECT_EQ(bus.cpu.pc, 40u);
    EXPECT_EQ(getReg(3), 4u);             // following instruction was at byte 4
}

// --- BR : conditional branch to register  (opcode 0x1A) ---
TEST_F(CPUFixture, BR_TakenWhenEQ)
{
    setReg(5, 42u);
    bus.cpu.status |= spg290::Z;
    execute(encodeBR(4, 5));
    EXPECT_EQ(bus.cpu.pc, 42u);
}

TEST_F(CPUFixture, BR_NotTaken)
{
    setReg(5, 42u);
    execute(encodeBR(4, 5));              // Z clear -> not taken
    EXPECT_EQ(bus.cpu.pc, 4u);
}
