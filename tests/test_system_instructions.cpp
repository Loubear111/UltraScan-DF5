#include "test_helpers.h"

// ============================================================
// System / coprocessor register moves (opcode 0x1F = OP_SYS).
// Control registers (cr), special registers (sr), and the per-CP
// data/control register files are CPU members and are inspected
// directly. None of these ops touch the condition flags.
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

// ---- Coprocessor data registers (mfcx / mtcx) ----
TEST_F(CPUFixture, MFCX_ReadsCoprocessorDataRegister)
{
    bus.cpu.copData[2][8] = 0xDEADBEEFu;
    execute(encodeMFCX(4, 8, 2));   // mfc2 r4, cr8
    EXPECT_EQ(getReg(4), 0xDEADBEEFu);
}

TEST_F(CPUFixture, MTCX_WritesCoprocessorDataRegister)
{
    setReg(4, 0x99887766u);
    execute(encodeMTCX(4, 8, 2));
    EXPECT_EQ(bus.cpu.copData[2][8], 0x99887766u);
}

TEST_F(CPUFixture, MTCX_PerCoprocessorSeparation)
{
    setReg(4, 0x11112222u);
    execute(encodeMTCX(4, 8, 1));   // write CP1
    EXPECT_EQ(bus.cpu.copData[1][8], 0x11112222u);
    EXPECT_EQ(bus.cpu.copData[2][8], 0u);   // CP2 untouched
}

// ---- Coprocessor control registers (mfccx / mtccx) ----
TEST_F(CPUFixture, MFCCX_ReadsCoprocessorControlRegister)
{
    bus.cpu.copCtrl[3][5] = 0x0000F00Du;
    execute(encodeMFCCX(4, 5, 3));
    EXPECT_EQ(getReg(4), 0x0000F00Du);
}

TEST_F(CPUFixture, MTCCX_WritesCoprocessorControlRegister)
{
    setReg(4, 0xFEEDFACEu);
    execute(encodeMTCCX(4, 5, 3));
    EXPECT_EQ(bus.cpu.copCtrl[3][5], 0xFEEDFACEu);
}

// ---- Custom-engine moves (mfcex / mtcex) ----
TEST_F(CPUFixture, MFCEX_LowOnly)
{
    bus.cpu.cel = 0x0000AAAAu;
    bus.cpu.ceh = 0x0000BBBBu;
    execute(encodeMFCEX(4, 5, 0x1));   // {HI,LO}=01 -> mfcel
    EXPECT_EQ(getReg(4), 0x0000AAAAu);
}

TEST_F(CPUFixture, MFCEX_HighOnly)
{
    bus.cpu.cel = 0x0000AAAAu;
    bus.cpu.ceh = 0x0000BBBBu;
    execute(encodeMFCEX(4, 5, 0x2));   // {HI,LO}=10 -> mfceh
    EXPECT_EQ(getReg(4), 0x0000BBBBu);
}

TEST_F(CPUFixture, MFCEX_BothHalves)
{
    bus.cpu.cel = 0x0000AAAAu;
    bus.cpu.ceh = 0x0000BBBBu;
    execute(encodeMFCEX(4, 5, 0x3));   // mfcehl: rD=CEH, rA=CEL
    EXPECT_EQ(getReg(4), 0x0000BBBBu);
    EXPECT_EQ(getReg(5), 0x0000AAAAu);
}

TEST_F(CPUFixture, MTCEX_BothHalves)
{
    setReg(4, 0x12340000u);
    setReg(5, 0x00005678u);
    execute(encodeMTCEX(4, 5, 0x3));   // mtcehl: CEH=rD, CEL=rA
    EXPECT_EQ(bus.cpu.ceh, 0x12340000u);
    EXPECT_EQ(bus.cpu.cel, 0x00005678u);
}

TEST_F(CPUFixture, MTCEX_LowOnly)
{
    setReg(4, 0x0000CCCCu);
    execute(encodeMTCEX(4, 5, 0x1));   // mtcel
    EXPECT_EQ(bus.cpu.cel, 0x0000CCCCu);
}

// ---- Coprocessor user-defined op (copx) : modeled as a no-op ----
TEST_F(CPUFixture, COPX_HasNoArchitecturalEffect)
{
    setReg(4, 0x12345678u);
    execute(encodeCOPX());
    EXPECT_EQ(getReg(4), 0x12345678u);
    EXPECT_EQ(bus.cpu.pc, 1u);
}
