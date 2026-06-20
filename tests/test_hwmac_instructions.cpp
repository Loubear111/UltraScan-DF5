#include "test_helpers.h"

// ============================================================
// Half-word multiply-accumulate family (opcode 0x1C).
//   Lower forms use rA[15:0] * rB[15:0] and target CEL.
//   Higher forms use rA[31:16] * rB[31:16] and target CEH.
// Control bits: Z = zero start, S = subtract, F = fractional (<<1),
// with saturation on the .fs forms (F && !Z). Operands are treated
// as signed 16-bit 2's-complement values.
// CEL / CEH are read directly from the CPU for verification.
// ============================================================

// ---- Lower-half base forms ----
TEST_F(CPUFixture, MAZL_MultiplyToZero)
{
    setReg(2, 0x00000003u);
    setReg(3, 0x00000004u);
    execute(encodeMAZL(2, 3));
    EXPECT_EQ(bus.cpu.cel, 12u);          // 0 + 3*4
}

TEST_F(CPUFixture, MAZL_SignedOperands)
{
    setReg(2, 0x0000FFFFu);               // low16 = -1
    setReg(3, 0x00000002u);               // low16 = 2
    execute(encodeMAZL(2, 3));
    EXPECT_EQ(bus.cpu.cel, 0xFFFFFFFEu);  // -2
}

TEST_F(CPUFixture, MAZL_IgnoresHighHalf)
{
    setReg(2, 0xFFFF0003u);               // high half must be ignored
    setReg(3, 0xFFFF0004u);
    execute(encodeMAZL(2, 3));
    EXPECT_EQ(bus.cpu.cel, 12u);
}

TEST_F(CPUFixture, MADL_Accumulates)
{
    bus.cpu.cel = 10u;
    setReg(2, 0x00000003u);
    setReg(3, 0x00000004u);
    execute(encodeMADL(2, 3));
    EXPECT_EQ(bus.cpu.cel, 22u);          // 10 + 12
}

TEST_F(CPUFixture, MSZL_NegatesProduct)
{
    setReg(2, 0x00000003u);
    setReg(3, 0x00000004u);
    execute(encodeMSZL(2, 3));
    EXPECT_EQ(bus.cpu.cel, 0xFFFFFFF4u);  // 0 - 12
}

TEST_F(CPUFixture, MSBL_SubtractsFromAccumulator)
{
    bus.cpu.cel = 100u;
    setReg(2, 0x00000003u);
    setReg(3, 0x00000004u);
    execute(encodeMSBL(2, 3));
    EXPECT_EQ(bus.cpu.cel, 88u);          // 100 - 12
}

// ---- Lower-half fractional / saturating forms ----
TEST_F(CPUFixture, MAZLF_ShiftsProductLeft)
{
    setReg(2, 0x00000003u);
    setReg(3, 0x00000004u);
    execute(encodeMAZLF(2, 3));
    EXPECT_EQ(bus.cpu.cel, 24u);          // (3*4) << 1
}

TEST_F(CPUFixture, MSZLF_NegatesShiftedProduct)
{
    setReg(2, 0x00000003u);
    setReg(3, 0x00000004u);
    execute(encodeMSZLF(2, 3));
    EXPECT_EQ(bus.cpu.cel, 0xFFFFFFE8u);  // -(24)
}

TEST_F(CPUFixture, MADLFS_NoOverflowAccumulates)
{
    bus.cpu.cel = 10u;
    setReg(2, 0x00000003u);
    setReg(3, 0x00000004u);
    execute(encodeMADLFS(2, 3));
    EXPECT_EQ(bus.cpu.cel, 34u);          // 10 + (12<<1)
}

TEST_F(CPUFixture, MADLFS_SaturatesOnOverflow)
{
    bus.cpu.cel = 0x7FFFFFFFu;
    setReg(2, 0x00000001u);
    setReg(3, 0x00000001u);
    execute(encodeMADLFS(2, 3));          // INT_MAX + 2 -> clamp
    EXPECT_EQ(bus.cpu.cel, 0x7FFFFFFFu);
}

TEST_F(CPUFixture, MSBLFS_SaturatesOnUnderflow)
{
    bus.cpu.cel = 0x80000000u;
    setReg(2, 0x00000001u);
    setReg(3, 0x00000001u);
    execute(encodeMSBLFS(2, 3));          // INT_MIN - 2 -> clamp
    EXPECT_EQ(bus.cpu.cel, 0x80000000u);
}

// ---- Higher-half forms ----
TEST_F(CPUFixture, MAZH_UsesHighHalfAndTargetsCEH)
{
    setReg(2, 0x00030000u);               // high16 = 3
    setReg(3, 0x00040000u);               // high16 = 4
    execute(encodeMAZH(2, 3));
    EXPECT_EQ(bus.cpu.ceh, 12u);
}

TEST_F(CPUFixture, MAZH_SignedHighHalf)
{
    setReg(2, 0xFFFF0000u);               // high16 = -1
    setReg(3, 0x00020000u);               // high16 = 2
    execute(encodeMAZH(2, 3));
    EXPECT_EQ(bus.cpu.ceh, 0xFFFFFFFEu);  // -2
}

TEST_F(CPUFixture, MADH_AccumulatesIntoCEH)
{
    bus.cpu.ceh = 5u;
    setReg(2, 0x00030000u);
    setReg(3, 0x00040000u);
    execute(encodeMADH(2, 3));
    EXPECT_EQ(bus.cpu.ceh, 17u);          // 5 + 12
}

TEST_F(CPUFixture, MSZH_NegatesProductIntoCEH)
{
    setReg(2, 0x00030000u);
    setReg(3, 0x00040000u);
    execute(encodeMSZH(2, 3));
    EXPECT_EQ(bus.cpu.ceh, 0xFFFFFFF4u);
}

TEST_F(CPUFixture, MSBH_SubtractsIntoCEH)
{
    bus.cpu.ceh = 100u;
    setReg(2, 0x00030000u);
    setReg(3, 0x00040000u);
    execute(encodeMSBH(2, 3));
    EXPECT_EQ(bus.cpu.ceh, 88u);
}

TEST_F(CPUFixture, MAZHF_ShiftsHighProduct)
{
    setReg(2, 0x00030000u);
    setReg(3, 0x00040000u);
    execute(encodeMAZHF(2, 3));
    EXPECT_EQ(bus.cpu.ceh, 24u);
}

TEST_F(CPUFixture, MADHFS_SaturatesOnOverflow)
{
    bus.cpu.ceh = 0x7FFFFFFFu;
    setReg(2, 0x00010000u);
    setReg(3, 0x00010000u);
    execute(encodeMADHFS(2, 3));
    EXPECT_EQ(bus.cpu.ceh, 0x7FFFFFFFu);
}

TEST_F(CPUFixture, MSBHFS_SaturatesOnUnderflow)
{
    bus.cpu.ceh = 0x80000000u;
    setReg(2, 0x00010000u);
    setReg(3, 0x00010000u);
    execute(encodeMSBHFS(2, 3));
    EXPECT_EQ(bus.cpu.ceh, 0x80000000u);
}

TEST_F(CPUFixture, MSZHF_NegatesShiftedHighProduct)
{
    setReg(2, 0x00030000u);
    setReg(3, 0x00040000u);
    execute(encodeMSZHF(2, 3));
    EXPECT_EQ(bus.cpu.ceh, 0xFFFFFFE8u);
}

// ---- 32-bit fractional multiply-subtract (msb.f, func6=0x1F) ----
TEST_F(CPUFixture, MSBF_SubtractsShiftedFullProduct)
{
    bus.cpu.ceh = 0u;
    bus.cpu.cel = 0u;
    setReg(2, 2u);
    setReg(3, 3u);
    execute(encodeMSBF(2, 3));            // {CEH,CEL} -= (2*3)<<1 = 12
    EXPECT_EQ(bus.cpu.cel, 0xFFFFFFF4u);  // low word of -12
    EXPECT_EQ(bus.cpu.ceh, 0xFFFFFFFFu);  // sign-extended high word
}
