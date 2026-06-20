#include "test_helpers.h"

// ============================================================
// Custom-engine multiply / divide and CEH/CEL moves.
// mul/mad/div place results in {CEH,CEL}; mfce*/mtce* shuttle
// those values to and from general-purpose registers.
// ============================================================

// --- MUL : signed, {CEH,CEL} = a * b   (func6=0x39) ---
TEST_F(CPUFixture, MUL_Basic)
{
    setReg(5, 3u);
    setReg(6, 4u);
    execute(encodeMUL(5, 6));
    EXPECT_EQ(bus.cpu.cel, 12u);
    EXPECT_EQ(bus.cpu.ceh, 0u);
}

TEST_F(CPUFixture, MUL_Signed)
{
    setReg(5, (uint32_t)(-1));
    setReg(6, 1u);
    execute(encodeMUL(5, 6));
    EXPECT_EQ(bus.cpu.cel, 0xFFFFFFFFu);
    EXPECT_EQ(bus.cpu.ceh, 0xFFFFFFFFu); // sign-extended high word
}

TEST_F(CPUFixture, MUL_HighWord)
{
    setReg(5, 0x00010000u);
    setReg(6, 0x00010000u);
    execute(encodeMUL(5, 6)); // 2^32
    EXPECT_EQ(bus.cpu.cel, 0u);
    EXPECT_EQ(bus.cpu.ceh, 1u);
}

// --- MULU : unsigned   (func6=0x3A) ---
TEST_F(CPUFixture, MULU_HighWord)
{
    setReg(5, 0xFFFFFFFFu);
    setReg(6, 2u);
    execute(encodeMULU(5, 6)); // 0x1FFFFFFFE
    EXPECT_EQ(bus.cpu.cel, 0xFFFFFFFEu);
    EXPECT_EQ(bus.cpu.ceh, 1u);
}

// --- DIV : signed, CEL=quotient CEH=remainder   (func6=0x3B) ---
TEST_F(CPUFixture, DIV_Basic)
{
    setReg(5, 17u);
    setReg(6, 5u);
    execute(encodeDIV(5, 6));
    EXPECT_EQ(bus.cpu.cel, 3u); // quotient
    EXPECT_EQ(bus.cpu.ceh, 2u); // remainder
}

TEST_F(CPUFixture, DIV_Signed)
{
    setReg(5, (uint32_t)(-17));
    setReg(6, 5u);
    execute(encodeDIV(5, 6));
    EXPECT_EQ(bus.cpu.cel, (uint32_t)(-3));
    EXPECT_EQ(bus.cpu.ceh, (uint32_t)(-2));
}

TEST_F(CPUFixture, DIV_ByZeroLeavesCERegsUnchanged)
{
    setReg(5, 100u);
    execute(encodeMTCEL(5)); // CEL = 100 (reuse a known value)
    setReg(6, 0u);
    setReg(5, 42u);
    execute(encodeDIV(5, 6)); // divisor 0 -> undefined, we leave CE regs alone
    EXPECT_EQ(bus.cpu.cel, 100u);
}

// --- DIVU : unsigned   (func6=0x3C) ---
TEST_F(CPUFixture, DIVU_Basic)
{
    setReg(5, 17u);
    setReg(6, 5u);
    execute(encodeDIVU(5, 6));
    EXPECT_EQ(bus.cpu.cel, 3u);
    EXPECT_EQ(bus.cpu.ceh, 2u);
}

// --- REM / REMU : rD = remainder   (func6=0x0A / 0x0B) ---
TEST_F(CPUFixture, REM_Basic)
{
    setReg(5, 17u);
    setReg(6, 5u);
    execute(encodeREM(7, 5, 6));
    EXPECT_EQ(getReg(7), 2u);
}

TEST_F(CPUFixture, REMU_Basic)
{
    setReg(5, 17u);
    setReg(6, 5u);
    execute(encodeREMU(7, 5, 6));
    EXPECT_EQ(getReg(7), 2u);
}

// --- MFCEL / MFCEH : move CE register to GPR ---
TEST_F(CPUFixture, MFCEL_MovesQuotient)
{
    setReg(5, 17u);
    setReg(6, 5u);
    execute(encodeDIV(5, 6)); // CEL=3, CEH=2
    execute(encodeMFCEL(7));
    EXPECT_EQ(getReg(7), 3u);
}

TEST_F(CPUFixture, MFCEH_MovesRemainder)
{
    setReg(5, 17u);
    setReg(6, 5u);
    execute(encodeDIV(5, 6));
    execute(encodeMFCEH(8));
    EXPECT_EQ(getReg(8), 2u);
}

// --- MTCEL / MTCEH : move GPR to CE register ---
TEST_F(CPUFixture, MTCEL_Basic)
{
    setReg(5, 0x12345678u);
    execute(encodeMTCEL(5));
    EXPECT_EQ(bus.cpu.cel, 0x12345678u);
}

TEST_F(CPUFixture, MTCEH_Basic)
{
    setReg(5, 0xCAFEBABEu);
    execute(encodeMTCEH(5));
    EXPECT_EQ(bus.cpu.ceh, 0xCAFEBABEu);
}

TEST_F(CPUFixture, MTCEL_ThenMFCEL_RoundTrip)
{
    setReg(5, 0xABCD1234u);
    execute(encodeMTCEL(5));
    execute(encodeMFCEL(9));
    EXPECT_EQ(getReg(9), 0xABCD1234u);
}
