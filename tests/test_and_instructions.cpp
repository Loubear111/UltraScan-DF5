#include "test_helpers.h"

// ============================================================
// ANDX  —  d = a & b
// Opcode 0, func6 = 0x10. Fully dispatched in clock().
// ============================================================

TEST_F(CPUFixture, ANDX_BasicResult)
{
    setReg(10, 0xF0F0F0F0u); // rA
    setReg(11, 0xFF00FF00u); // rB
    execute(encodeANDX(12, 10, 11, false));
    EXPECT_EQ(getReg(12), 0xF000F000u);
}

TEST_F(CPUFixture, ANDX_ZeroResult)
{
    setReg(10, 0xAAAAAAAAu);
    setReg(11, 0x55555555u);
    execute(encodeANDX(12, 10, 11, false));
    EXPECT_EQ(getReg(12), 0x00000000u);
}

// CU=1, result==0 → Z flag set
TEST_F(CPUFixture, ANDX_CU_SetsZFlagWhenResultIsZero)
{
    setReg(10, 0xAAAAAAAAu);
    setReg(11, 0x55555555u);
    execute(encodeANDX(12, 10, 11, true));
    EXPECT_EQ(getReg(12), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

// CU=1, result!=0 → Z flag clear
TEST_F(CPUFixture, ANDX_CU_ClearsZFlagWhenResultIsNonZero)
{
    setReg(10, 0xFFFFFFFFu);
    setReg(11, 0xFFFFFFFFu);
    execute(encodeANDX(12, 10, 11, true));
    EXPECT_NE(getReg(12), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 0);
}

// CU=0 → flags must NOT be updated even when result is zero
TEST_F(CPUFixture, ANDX_NoCU_FlagsUntouched)
{
    setReg(10, 0xAAAAAAAAu);
    setReg(11, 0x55555555u);
    execute(encodeANDX(12, 10, 11, false)); // result=0, CU=0
    EXPECT_EQ(getFlag(spg290::Z), 0);       // Z must still be clear
    EXPECT_EQ(getFlag(spg290::N), 0);       // N must still be clear
}

// N flag — NOTE: the current implementation sets N when bit 31 == 0
// (i.e. N is set for non-negative results). This is inverted from
// typical CPU convention and may be a bug, but the test documents
// the actual behaviour.
TEST_F(CPUFixture, ANDX_CU_NFlagSetWhenMSBisClear)
{
    setReg(10, 0x7FFFFFFFu); // bit 31 clear
    setReg(11, 0x7FFFFFFFu);
    execute(encodeANDX(12, 10, 11, true));
    EXPECT_EQ(getFlag(spg290::N), 1); // N set because (result >> 31) == 0
}

TEST_F(CPUFixture, ANDX_CU_NFlagClearWhenMSBisSet)
{
    setReg(10, 0xFFFFFFFFu); // bit 31 set
    setReg(11, 0xFFFFFFFFu);
    execute(encodeANDX(12, 10, 11, true));
    EXPECT_EQ(getFlag(spg290::N), 0); // N clear because (result >> 31) == 1
}

// Verifies the exact instruction word from main.cpp decodes identically
TEST_F(CPUFixture, ANDX_KnownGoodInstructionWord)
{
    setReg(10, 0xFFFF45E6u); // A register (from main.cpp)
    setReg(11, 0b0010u);     // B register
    execute(0x818A2C20u);    // ANDX D=12, A=10, B=11, CU=0
    EXPECT_EQ(getReg(12), 0xFFFF45E6u & 0b0010u);
}


// ============================================================
// ANDIX  —  d = d & imm16
// Opcode 1, func3=4. Fully dispatched in clock().
// ============================================================

TEST_F(CPUFixture, ANDIX_BasicResult)
{
    setReg(12, 0xDEADBEEFu);
    execute(encodeANDIX(12, 0x00FFu, false));
    EXPECT_EQ(getReg(12), 0x000000EFu);
}

// Mask with 0xFFFF preserves only the lower 16 bits
TEST_F(CPUFixture, ANDIX_MaskLower16Bits)
{
    setReg(12, 0xDEADBEEFu);
    execute(encodeANDIX(12, 0xFFFFu, false));
    EXPECT_EQ(getReg(12), 0x0000BEEFu);
}

// CU=1, result==0 → Z flag set
TEST_F(CPUFixture, ANDIX_CU_SetsZFlag)
{
    setReg(12, 0x00000000u);
    execute(encodeANDIX(12, 0xFFFFu, true));
    EXPECT_EQ(getReg(12), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

// CU=0 → flags untouched
TEST_F(CPUFixture, ANDIX_NoCU_FlagsUntouched)
{
    setReg(12, 0x00000000u);
    execute(encodeANDIX(12, 0xFFFFu, false)); // result=0, CU=0
    EXPECT_EQ(getFlag(spg290::Z), 0);
}

// Verifies the known-good instruction word from main.cpp
TEST_F(CPUFixture, ANDIX_KnownGoodInstructionWord)
{
    setReg(12, 0xFFFFFFFFu);
    execute(0x85937FFEu); // ANDIX D=12, imm=0xFFFF, CU=0
    EXPECT_EQ(getReg(12), 0x0000FFFFu);
}


// ============================================================
// ANDRIX  —  d = a & imm14
// Opcode 12 (0b01100). Fully dispatched in clock().
// ============================================================

TEST_F(CPUFixture, ANDRIX_BasicResult)
{
    setReg(10, 0xFFFFFFFFu); // rA
    execute(encodeANDRIX(12, 10, 0x234Eu, false));
    EXPECT_EQ(getReg(12), 0x234Eu);
}

TEST_F(CPUFixture, ANDRIX_ZeroResult)
{
    setReg(10, 0x00000000u);
    execute(encodeANDRIX(12, 10, 0x3FFFu, false));
    EXPECT_EQ(getReg(12), 0u);
}

// CU=1, result==0 → Z flag set
TEST_F(CPUFixture, ANDRIX_CU_SetsZFlag)
{
    setReg(10, 0x00000000u);
    execute(encodeANDRIX(12, 10, 0x3FFFu, true));
    EXPECT_EQ(getFlag(spg290::Z), 1);
}

// CU=0 → flags untouched
TEST_F(CPUFixture, ANDRIX_NoCU_FlagsUntouched)
{
    setReg(10, 0x00000000u);
    execute(encodeANDRIX(12, 10, 0x3FFFu, false));
    EXPECT_EQ(getFlag(spg290::Z), 0);
}

// Verifies the known-good instruction word from main.cpp
TEST_F(CPUFixture, ANDRIX_KnownGoodInstructionWord)
{
    setReg(10, 0xFFFF45E6u); // A register (from main.cpp)
    execute(0xB18A469Cu);    // ANDRIX D=12, A=10, imm=0x234E, CU=0
    EXPECT_EQ(getReg(12), 0xFFFF45E6u & 0x234Eu);
}


// ============================================================
// ANDISX  —  d = d & (imm16 << 16)
// Opcode not yet mapped in clock() dispatch.
// Fill in OPCODE_UNKNOWN in test_helpers.h and add a dispatch
// case in clock() before enabling these tests.
// ============================================================

TEST_F(CPUFixture, ANDISX_BasicResult)
{
    setReg(12, 0xDEADBEEFu);
    execute(encodeANDISX(12, 0xFFFFu, false)); // mask = 0xFFFF0000
    EXPECT_EQ(getReg(12), 0xDEAD0000u);
}

TEST_F(CPUFixture, ANDISX_CU_SetsZFlag)
{
    setReg(12, 0x0000FFFFu); // upper 16 bits already zero
    execute(encodeANDISX(12, 0xFFFFu, true));
    EXPECT_EQ(getReg(12), 0u);
    EXPECT_EQ(getFlag(spg290::Z), 1);
}
