#include "test_helpers.h"

// ============================================================
// Control / special register moves. Control registers (cr) and special
// registers (sr) are CPU members and are inspected directly. None of
// these ops touch the condition flags. The coprocessor forms are not
// implemented (they raise a reserved-instruction exception).
// ============================================================

// ---- Control registers (mfcr / mtcr) ----
TEST_F(CPUFixture, MFCR_ReadsControlRegister)
{
    bus.cpu.cr[2] = 0x12345678u;
    execute(encodeMFCR(4, 2));
    EXPECT_EQ(getReg(4), 0x12345678u);
}

TEST_F(CPUFixture, MTCR_WritesControlRegister)
{
    setReg(4, 0xABCDEF01u);
    execute(encodeMTCR(4, 2));
    EXPECT_EQ(bus.cpu.cr[2], 0xABCDEF01u);
}

TEST_F(CPUFixture, MTCR_then_MFCR_RoundTrip)
{
    setReg(4, 0xCAFEBABEu);
    execute(encodeMTCR(4, 5));      // cr5 = r4
    execute(encodeMFCR(6, 5));      // r6  = cr5
    EXPECT_EQ(getReg(6), 0xCAFEBABEu);
}

// ---- Special registers (mfsr / mtsr) ----
TEST_F(CPUFixture, MFSR_ReadsSpecialRegister)
{
    bus.cpu.sr[1] = 0x0BADF00Du;
    execute(encodeMFSR(4, 1));
    EXPECT_EQ(getReg(4), 0x0BADF00Du);
}

TEST_F(CPUFixture, MTSR_WritesSpecialRegister)
{
    setReg(2, 0x55AA55AAu);
    execute(encodeMTSR(2, 1));
    EXPECT_EQ(bus.cpu.sr[1], 0x55AA55AAu);
}

