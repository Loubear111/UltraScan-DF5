#include "test_helpers.h"

// ============================================================
// Multiply-accumulate family. Results accumulate into {CEH,CEL}.
// ============================================================

// --- MAD : {CEH,CEL} += a * b (signed)  (func6=0x0C) ---
TEST_F(CPUFixture, MAD_FromZero)
{
    setReg(5, 3u);
    setReg(6, 4u);
    execute(encodeMAD(5, 6)); // CE starts at 0
    EXPECT_EQ(bus.cpu.cel, 12u);
    EXPECT_EQ(bus.cpu.ceh, 0u);
}

TEST_F(CPUFixture, MAD_Accumulates)
{
    setReg(5, 100u);
    execute(encodeMTCEL(5)); // CEL = 100
    setReg(5, 3u);
    setReg(6, 4u);
    execute(encodeMAD(5, 6)); // 100 + 12
    EXPECT_EQ(bus.cpu.cel, 112u);
}

TEST_F(CPUFixture, MAD_CarriesIntoHighWord)
{
    setReg(5, 0x00010000u);
    setReg(6, 0x00010000u);
    execute(encodeMAD(5, 6)); // 2^32
    EXPECT_EQ(bus.cpu.cel, 0u);
    EXPECT_EQ(bus.cpu.ceh, 1u);
}

// --- MADU : unsigned  (func6=0x0D) ---
TEST_F(CPUFixture, MADU_Unsigned)
{
    setReg(5, 0xFFFFFFFFu);
    setReg(6, 1u);
    execute(encodeMADU(5, 6));
    EXPECT_EQ(bus.cpu.cel, 0xFFFFFFFFu);
    EXPECT_EQ(bus.cpu.ceh, 0u);
}

// --- MSB : {CEH,CEL} -= a * b (signed)  (func6=0x0E) ---
TEST_F(CPUFixture, MSB_Subtracts)
{
    setReg(5, 100u);
    execute(encodeMTCEL(5)); // CEL = 100
    setReg(5, 3u);
    setReg(6, 4u);
    execute(encodeMSB(5, 6)); // 100 - 12
    EXPECT_EQ(bus.cpu.cel, 88u);
}

TEST_F(CPUFixture, MSB_FromZeroGoesNegative)
{
    setReg(5, 1u);
    setReg(6, 1u);
    execute(encodeMSB(5, 6)); // 0 - 1
    EXPECT_EQ(bus.cpu.cel, 0xFFFFFFFFu);
    EXPECT_EQ(bus.cpu.ceh, 0xFFFFFFFFu);
}

// --- MSBU : unsigned  (func6=0x0F) ---
TEST_F(CPUFixture, MSBU_Subtracts)
{
    setReg(5, 50u);
    execute(encodeMTCEL(5));
    setReg(5, 5u);
    setReg(6, 6u);
    execute(encodeMSBU(5, 6)); // 50 - 30
    EXPECT_EQ(bus.cpu.cel, 20u);
}

// --- MULF : signed fractional, {CEH,CEL} = (a*b) << 1  (func6=0x15) ---
TEST_F(CPUFixture, MULF_HalfTimesHalf)
{
    // 0.5 * 0.5 = 0.25 in Q31  ->  CEH = 0x20000000
    setReg(5, 0x40000000u);
    setReg(6, 0x40000000u);
    execute(encodeMULF(5, 6));
    EXPECT_EQ(bus.cpu.ceh, 0x20000000u);
    EXPECT_EQ(bus.cpu.cel, 0u);
}

// --- MADF : fractional multiply-add  (func6=0x16) ---
TEST_F(CPUFixture, MADF_FromZero)
{
    setReg(5, 0x40000000u);
    setReg(6, 0x40000000u);
    execute(encodeMADF(5, 6));
    EXPECT_EQ(bus.cpu.ceh, 0x20000000u);
}
