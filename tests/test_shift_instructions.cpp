#include "test_helpers.h"

// ============================================================
// Register-operand shifts/rotates. Shift amount = low 5 bits of rB.
// When CU is set: N=bit31, Z (logical/arith shifts), C=last bit shifted/rotated out.
// ============================================================

// --- SLLX : d = a << sa  (func6=0x20) ---
TEST_F(CPUFixture, SLLX_Basic)
{
    setReg(5, 0x00000001u);
    setReg(6, 4u);
    execute(encodeSLLX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 0x00000010u);
}

TEST_F(CPUFixture, SLLX_CarryAndZero_CUSet)
{
    setReg(5, 0x80000000u);
    setReg(6, 1u);
    execute(encodeSLLX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1); // bit shifted out of MSB
}

TEST_F(CPUFixture, SLLX_NegativeResult_CUSet)
{
    setReg(5, 0x40000000u);
    setReg(6, 1u);
    execute(encodeSLLX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0x80000000u);
    EXPECT_EQ(getFlag(spg290::N), 1);
}

// --- SRLX : d = a >> sa (logical)  (func6=0x21) ---
TEST_F(CPUFixture, SRLX_Basic)
{
    setReg(5, 0x00000010u);
    setReg(6, 4u);
    execute(encodeSRLX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 0x00000001u);
}

TEST_F(CPUFixture, SRLX_HighBitNotPreserved)
{
    setReg(5, 0x80000000u);
    setReg(6, 4u);
    execute(encodeSRLX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 0x08000000u); // zero filled
}

TEST_F(CPUFixture, SRLX_Carry_CUSet)
{
    setReg(5, 0x00000001u);
    setReg(6, 1u);
    execute(encodeSRLX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
    EXPECT_EQ(getFlag(spg290::C), 1); // bit 0 shifted out
}

// --- SRAX : d = a >> sa (arithmetic)  (func6=0x22) ---
TEST_F(CPUFixture, SRAX_SignExtends)
{
    setReg(5, 0x80000000u);
    setReg(6, 4u);
    execute(encodeSRAX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 0xF8000000u); // sign filled
}

TEST_F(CPUFixture, SRAX_PositiveSameAsLogical)
{
    setReg(5, 0x40000000u);
    setReg(6, 4u);
    execute(encodeSRAX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 0x04000000u);
}

// --- ROLX : rotate left  (func6=0x23) ---
TEST_F(CPUFixture, ROLX_Basic)
{
    setReg(5, 0x80000001u);
    setReg(6, 1u);
    execute(encodeROLX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0x00000003u);
    EXPECT_EQ(getFlag(spg290::C), 1); // old bit31 rotated out
}

TEST_F(CPUFixture, ROLX_ZeroAmountIsCopy)
{
    setReg(5, 0xDEADBEEFu);
    setReg(6, 0u);
    execute(encodeROLX(7, 5, 6, false));
    EXPECT_EQ(getReg(7), 0xDEADBEEFu);
}

// --- RORX : rotate right  (func6=0x24) ---
TEST_F(CPUFixture, RORX_Basic)
{
    setReg(5, 0x80000001u);
    setReg(6, 1u);
    execute(encodeRORX(7, 5, 6, true));
    EXPECT_EQ(getReg(7), 0xC0000000u);
    EXPECT_EQ(getFlag(spg290::C), 1); // old bit0 rotated out
}

// ============================================================
// Immediate-operand shifts/rotates. SA = bits 14:10.
// ============================================================

TEST_F(CPUFixture, SLLIX_Basic)
{
    setReg(5, 1u);
    execute(encodeSLLIX(7, 5, 4u, false));
    EXPECT_EQ(getReg(7), 0x00000010u);
}

TEST_F(CPUFixture, SRLIX_Basic)
{
    setReg(5, 0x00000010u);
    execute(encodeSRLIX(7, 5, 4u, false));
    EXPECT_EQ(getReg(7), 0x00000001u);
}

TEST_F(CPUFixture, SRAIX_SignExtends)
{
    setReg(5, 0x80000000u);
    execute(encodeSRAIX(7, 5, 4u, false));
    EXPECT_EQ(getReg(7), 0xF8000000u);
}

TEST_F(CPUFixture, ROLIX_Basic)
{
    setReg(5, 0x80000001u);
    execute(encodeROLIX(7, 5, 1u, true));
    EXPECT_EQ(getReg(7), 0x00000003u);
    EXPECT_EQ(getFlag(spg290::C), 1);
}

TEST_F(CPUFixture, RORIX_Basic)
{
    setReg(5, 0x80000001u);
    execute(encodeRORIX(7, 5, 1u, true));
    EXPECT_EQ(getReg(7), 0xC0000000u);
    EXPECT_EQ(getFlag(spg290::C), 1);
}
