// Condition register layout, exception entry/rte flag shadowing and the
// vector formulas (plan 2.5, story 8). Positions follow MAME's S+core7 core.
#include "test_helpers.h"

TEST(Cr1Layout, FlagBitPositions)
{
    EXPECT_EQ((uint32_t)spg290::V, 1u << 0);
    EXPECT_EQ((uint32_t)spg290::C, 1u << 1);
    EXPECT_EQ((uint32_t)spg290::Z, 1u << 2);
    EXPECT_EQ((uint32_t)spg290::N, 1u << 3);
    EXPECT_EQ((uint32_t)spg290::T, 1u << 4);
}

TEST_F(CPUFixture, Cmp_WritesFlagsIntoCr1LowBits)
{
    setReg(1, 5u);
    setReg(2, 5u);
    execute(encodeCMP(1, 2, 3));            // equal: Z = 1, C = 1 (no borrow)
    execute(encodeMFCR(4, 1));
    EXPECT_EQ(getReg(4) & 0x1Fu, 0x06u);    // Z (bit 2) | C (bit 1)
}

TEST_F(CPUFixture, ExceptionEntry_CopiesFlagsToCr1Bits9To5)
{
    bus.cpu.status = spg290::T | spg290::Z | spg290::C;   // 0b10110
    execute(encodeSYSCALL());
    EXPECT_EQ((bus.cpu.status >> 5) & 0x1Fu, 0x16u);
    EXPECT_EQ(bus.cpu.status & 0x1Fu, 0x16u);             // live flags are left as they were
}

TEST_F(CPUFixture, Rte_RestoresLiveFlagsFromCr1Bits9To5)
{
    bus.cpu.status = spg290::N | spg290::V;               // 0b01001
    execute(encodeSYSCALL());
    bus.cpu.status = (bus.cpu.status & ~0x1Fu) | spg290::Z;   // handler changes the live flags
    execute(encodeRTE());
    EXPECT_EQ(bus.cpu.status & 0x1Fu, 0x09u);
}

TEST_F(CPUFixture, VectorBaseIsCr3UpperHalf)
{
    bus.cpu.cr[3] = 0x12345601u;
    EXPECT_EQ(bus.cpu.generalVectorAddr(), 0x12340200u);
    EXPECT_EQ(bus.cpu.irqVectorAddr(53), 0x12340200u + (53u << 4));   // 16-byte spacing: base + 0x550
    bus.cpu.cr[3] = 0x12345600u;
    EXPECT_EQ(bus.cpu.irqVectorAddr(53), 0x12340200u + (53u << 2));   // 4-byte spacing: base + 0x2D4
    EXPECT_EQ(bus.cpu.irqVectorAddr(0), bus.cpu.generalVectorAddr());
}

TEST_F(CPUFixture, Irq_UsesCauseCode20WithSourceInEcr)
{
    bus.cpu.cr[3] = 0x10000u;
    bus.cpu.cr[0] = spg290::PSR_IEC;
    bus.cpu.setIRQ(true, 53);
    execute(encodeNOP());
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 20u);
    EXPECT_EQ((bus.cpu.cr[2] & spg290::ECR_VECTOR_MASK) >> spg290::ECR_VECTOR_SHIFT, 53u);
    EXPECT_EQ(bus.cpu.pc, 0x10200u + (53u << 2));
}
