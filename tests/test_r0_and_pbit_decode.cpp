#include "test_helpers.h"

// ============================================================
// Phase 4.0 prerequisites.
//
// P1: r0 is the stack pointer, a normal register.
// P2: the J-form displacement, the b{cond} Disp[18:9] and the load/store
//     Imm15 exclude the P-bit at bit 15. Raw words below are real GAS
//     output / hand-assembled from the documented field layout, so they do
//     not depend on the test encoders.
// ============================================================

TEST_F(CPUFixture, R0_IsWritable)
{
    execute(encodeADDIX(0, 24, false));      // addi r0, 24
    EXPECT_EQ(getReg(0), 24u);
}

TEST_F(CPUFixture, R0_PreIndexStoreWithWriteback_ActsAsStackPointer)
{
    setReg(0, 0x1000u);
    setReg(2, 0xABCD1234u);
    execute(encodeSW_idx(2, 0, -4, /*pre=*/true));   // sw r2,[r0,-4]+
    EXPECT_EQ(getReg(0), 0xFFCu);
    EXPECT_EQ(getMem(0xFFC), 0xABCD1234u);
}

TEST_F(CPUFixture, R0_PostIndexLoadWithWriteback_ActsAsStackPointer)
{
    setReg(0, 0xFFCu);
    setMem(0xFFC, 0x55AA55AAu);
    execute(encodeLW_idx(2, 0, 4, /*pre=*/false));   // lw r2,[r0]+,4
    EXPECT_EQ(getReg(2), 0x55AA55AAu);
    EXPECT_EQ(getReg(0), 0x1000u);
}

TEST_F(CPUFixture, LoadImm15_RealGasWord)
{
    // GAS: lw r21,[r20,4]
    setReg(20, 0x100u);
    setMem(0x104, 0x1234ABCDu);
    execute(0xC2B48004u);
    EXPECT_EQ(getReg(21), 0x1234ABCDu);
}

TEST_F(CPUFixture, StoreImm15_NegativeOffset)
{
    // sw r21,[r20,-4] built from fields: opcode 0x14, P-bits set, Imm15 = 0x7FFC
    setReg(20, 0x104u);
    setReg(21, 0xCAFEBABEu);
    execute(0x80000000u | (0x14u << 26) | (21u << 21) | (20u << 16) | 0x8000u | 0x7FFCu);
    EXPECT_EQ(getMem(0x100), 0xCAFEBABEu);
}

TEST_F(CPUFixture, J_RealHyperExeVectorWord)
{
    // First word of every HYPER.EXE demo (little-endian bytes 00 83 12 88).
    // Disp24 = {bits 25:16, bits 14:1} = 0x48180, target = Disp24 << 1.
    bus.cpu.pc = 0x200;
    execute(0x88128300u);
    EXPECT_EQ(bus.cpu.pc, 0x90300u);
}

TEST_F(CPUFixture, BCond_ForwardDisplacementUsesBits25to16)
{
    // b (always, bc=15) with Disp = 0x200 halfwords: Disp[18:9] = 1 -> bit 16.
    bus.cpu.pc = 0;
    execute(0x80000000u | (0x04u << 26) | (1u << 16) | 0x8000u | (15u << 10));
    EXPECT_EQ(bus.cpu.pc, 0x400u);
}
