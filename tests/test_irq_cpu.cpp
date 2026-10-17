#include "test_helpers.h"

// ============================================================
// CPU-side interrupt handling: IRQ sampling at instruction
// boundaries, vector selection from EXCPvec (cr3), rte return,
// sleep, and the real GAS-assembled CR-form encodings.
// ============================================================

static constexpr uint32_t VEC_BASE = 0x1000;
// 16-byte spacing: irq(n) = base + 0x200 + 0x10*(n+1)
static constexpr uint32_t VEC53_MODE16 = VEC_BASE + 0x200 + 0x10 * 54;
// 4-byte spacing
static constexpr uint32_t VEC53_MODE4  = VEC_BASE + 0x200 + 0x04 * 54;

static uint32_t ecrVector(const spg290 &cpu)
{
    return (cpu.cr[2] & spg290::ECR_VECTOR_MASK) >> spg290::ECR_VECTOR_SHIFT;
}

TEST_F(CPUFixture, IRQ_IgnoredWhenInterruptsDisabled)
{
    bus.cpu.cr[3] = VEC_BASE | 1;
    bus.cpu.setIRQ(true, 53);
    execute(encodeADDIX(1, 5, false));
    EXPECT_EQ(getReg(1), 5u);
    EXPECT_EQ(bus.cpu.pc, 4u);
}

TEST_F(CPUFixture, IRQ_TakenWhenEnabled_Mode16)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    bus.cpu.cr[3] = VEC_BASE | 1;
    bus.cpu.setIRQ(true, 53);
    execute(encodeADDIX(1, 5, false));

    EXPECT_EQ(bus.cpu.pc, VEC53_MODE16);
    EXPECT_EQ(getReg(1), 0u);                       // interrupted instruction not run
    EXPECT_EQ(bus.cpu.cr[5], 0u);                   // EPC = next instruction to run
    EXPECT_EQ(ecrVector(bus.cpu), 53u);
    EXPECT_EQ(bus.cpu.cr[0] & spg290::PSR_IEC, 0u);
    EXPECT_NE(bus.cpu.cr[0] & spg290::PSR_IES, 0u);
}

TEST_F(CPUFixture, IRQ_TakenWhenEnabled_Mode4)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    bus.cpu.cr[3] = VEC_BASE;
    bus.cpu.setIRQ(true, 53);
    execute(encodeADDIX(1, 5, false));
    EXPECT_EQ(bus.cpu.pc, VEC53_MODE4);
}

TEST_F(CPUFixture, IRQ_RteResumesInterruptedInstructionExactlyOnce)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    bus.cpu.cr[3] = VEC_BASE | 1;
    bus.write32(0, encodeADDIX(1, 5, false));
    bus.write32(VEC53_MODE16, encodeRTE());

    bus.cpu.setIRQ(true, 53);
    bus.cpu.clock();                                // enter handler
    bus.cpu.setIRQ(false, 0);                       // handler acknowledged the source
    bus.cpu.clock();                                // rte
    EXPECT_EQ(bus.cpu.pc, 0u);
    EXPECT_NE(bus.cpu.cr[0] & spg290::PSR_IEC, 0u);

    bus.cpu.clock();                                // the interrupted addix
    EXPECT_EQ(getReg(1), 5u);
    EXPECT_EQ(bus.cpu.pc, 4u);
}

TEST_F(CPUFixture, IRQ_LevelSensitive_ReentersIfStillAsserted)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    bus.cpu.cr[3] = VEC_BASE | 1;
    bus.write32(VEC53_MODE16, encodeRTE());

    bus.cpu.setIRQ(true, 53);
    bus.cpu.clock();                                // enter
    bus.cpu.clock();                                // rte
    bus.cpu.clock();                                // line still high -> enter again
    EXPECT_EQ(bus.cpu.pc, VEC53_MODE16);
}

TEST_F(CPUFixture, IRQ_PendingLineTakenOnceEnabledByMtcr)
{
    bus.cpu.cr[3] = VEC_BASE | 1;
    bus.cpu.setIRQ(true, 53);
    setReg(5, spg290::PSR_IEC);
    execute(encodeMTCR(5, 0));                      // runs: IE was clear
    EXPECT_EQ(bus.cpu.pc, 4u);
    bus.cpu.clock();                                // IE now set -> taken
    EXPECT_EQ(bus.cpu.pc, VEC53_MODE16);
}

TEST_F(CPUFixture, Sleep_HaltsUntilIRQAsserted)
{
    execute(encodeSLEEP());
    EXPECT_TRUE(bus.cpu.isHalted());
    bus.write32(4, encodeADDIX(1, 5, false));

    for (int i = 0; i < 5; ++i) bus.cpu.clock();
    EXPECT_EQ(bus.cpu.pc, 4u);
    EXPECT_EQ(getReg(1), 0u);

    bus.cpu.setIRQ(true, 10);                       // IE clear: wakes, not taken
    bus.cpu.clock();
    EXPECT_FALSE(bus.cpu.isHalted());
    EXPECT_EQ(getReg(1), 5u);
}

TEST_F(CPUFixture, Sleep_WakesIntoHandlerWhenIEEnabled)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    bus.cpu.cr[3] = VEC_BASE | 1;
    execute(encodeSLEEP());
    bus.cpu.setIRQ(true, 53);
    bus.cpu.clock();
    EXPECT_EQ(bus.cpu.pc, VEC53_MODE16);
    EXPECT_EQ(bus.cpu.cr[5], 4u);                   // resumes after the sleep
}

// ---- Real GAS-assembled CR-form words (score-elf-as -SCORE7) ----
TEST_F(CPUFixture, CRForm_RealMfcrWord)
{
    bus.cpu.cr[0] = 0x5;
    execute(0x98808001u);                           // mfcr r4, cr0
    EXPECT_EQ(getReg(4), 0x5u);
}

TEST_F(CPUFixture, CRForm_RealMtcrWord)
{
    setReg(6, VEC_BASE | 1);
    execute(0x98c38000u);                           // mtcr r6, cr3
    EXPECT_EQ(bus.cpu.cr[3], VEC_BASE | 1);
}

TEST_F(CPUFixture, CRForm_RealRteWord)
{
    bus.cpu.cr[5] = 0x40;
    execute(0x98008084u);                           // rte
    EXPECT_EQ(bus.cpu.pc, 0x40u);
}

TEST_F(CPUFixture, VectorAddress_Helpers)
{
    bus.cpu.cr[3] = 0xA0000001u;
    EXPECT_EQ(bus.cpu.generalVectorAddr(), 0xA0000200u);
    EXPECT_EQ(bus.cpu.irqVectorAddr(53), 0xA0000200u + 0x10u * 54u);
}
