#include "test_helpers.h"

// ============================================================
// Pre/post-index loads and stores (opcode 0x1B).
//   post-index: access uses rA, then rA += sext(Imm12)
//   pre-index : access uses rA + sext(Imm12), then rA += sext(Imm12)
// In both cases the base register rA is updated afterwards.
// Registers and memory share the bus RAM array (word-indexed).
// ============================================================

// --- LW post / pre ---
TEST_F(CPUFixture, LW_PostIndex_LoadsThenUpdatesBase)
{
    setReg(2, 100u);
    setMem(100,  0xDEADBEEFu);
    execute(encodeLW_idx(4, 2, 8, /*pre=*/false));
    EXPECT_EQ(getReg(4), 0xDEADBEEFu);  // loaded from base (100)
    EXPECT_EQ(getReg(2), 108u);         // base updated by +8
}

TEST_F(CPUFixture, LW_PreIndex_LoadsFromUpdatedAddress)
{
    setReg(2, 100u);
    setMem(108,  0x12345678u);
    execute(encodeLW_idx(4, 2, 8, /*pre=*/true));
    EXPECT_EQ(getReg(4), 0x12345678u);  // loaded from base+8 (108)
    EXPECT_EQ(getReg(2), 108u);         // base updated by +8
}

TEST_F(CPUFixture, LW_PostIndex_NegativeImmediate)
{
    setReg(2, 100u);
    setMem(100,  0xCAFEF00Du);
    execute(encodeLW_idx(4, 2, -4, /*pre=*/false));
    EXPECT_EQ(getReg(4), 0xCAFEF00Du);
    EXPECT_EQ(getReg(2), 96u);          // 100 - 4
}

TEST_F(CPUFixture, LW_PostIndex_WritebackWinsWhenDestEqualsBase)
{
    // When rD == rA, the spec updates rA last, so the writeback value wins.
    setReg(2, 100u);
    setMem(100,  0xDEADBEEFu);
    execute(encodeLW_idx(2, 2, 8, /*pre=*/false));
    EXPECT_EQ(getReg(2), 108u);
}

// --- SW post / pre ---
TEST_F(CPUFixture, SW_PostIndex_StoresThenUpdatesBase)
{
    setReg(4, 0x99887766u);
    setReg(2, 200u);
    execute(encodeSW_idx(4, 2, 4, /*pre=*/false));
    EXPECT_EQ(getMem(200), 0x99887766u); // stored at base (200)
    EXPECT_EQ(getReg(2), 204u);
}

TEST_F(CPUFixture, SW_PreIndex_StoresAtUpdatedAddress)
{
    setReg(4, 0x99887766u);
    setReg(2, 200u);
    execute(encodeSW_idx(4, 2, 4, /*pre=*/true));
    EXPECT_EQ(getMem(204), 0x99887766u); // stored at base+4 (204)
    EXPECT_EQ(getReg(2), 204u);
}

// --- Byte loads ---
// Byte-within-word ordering is big-endian: offset 0 is the most-
// significant byte of the word that was written there.
TEST_F(CPUFixture, LBU_PostIndex_ZeroExtends)
{
    setReg(2, 100u);
    setMem(100,  0xDEADBEEFu);
    execute(encodeLBU_idx(4, 2, 1, /*pre=*/false));
    EXPECT_EQ(getReg(4), 0x000000DEu);
    EXPECT_EQ(getReg(2), 101u);
}

TEST_F(CPUFixture, LB_PostIndex_SignExtends)
{
    setReg(2, 100u);
    setMem(100,  0xFF000000u);
    execute(encodeLB_idx(4, 2, 1, /*pre=*/false));
    EXPECT_EQ(getReg(4), 0xFFFFFFFFu);
    EXPECT_EQ(getReg(2), 101u);
}

// --- Half-word loads ---
TEST_F(CPUFixture, LHU_PreIndex_ZeroExtends)
{
    setReg(2, 100u);
    setMem(102,  0xDEADBEEFu);
    execute(encodeLHU_idx(4, 2, 2, /*pre=*/true));
    EXPECT_EQ(getReg(4), 0x0000DEADu);
    EXPECT_EQ(getReg(2), 102u);
}

TEST_F(CPUFixture, LH_PreIndex_SignExtends)
{
    setReg(2, 100u);
    setMem(102,  0xFFFF0000u);
    execute(encodeLH_idx(4, 2, 2, /*pre=*/true));
    EXPECT_EQ(getReg(4), 0xFFFFFFFFu);
    EXPECT_EQ(getReg(2), 102u);
}

// --- Byte / half-word stores (now genuine single-byte/half accesses) ---
TEST_F(CPUFixture, SB_PostIndex_PreservesUpperBytes)
{
    setReg(4, 0x000000ABu);
    setReg(2, 200u);
    setMem(200,  0x11223344u);
    execute(encodeSB_idx(4, 2, 1, /*pre=*/false));
    EXPECT_EQ(getMem(200), 0xAB223344u);
    EXPECT_EQ(getReg(2), 201u);
}

TEST_F(CPUFixture, SH_PostIndex_PreservesUpperHalf)
{
    setReg(4, 0x0000ABCDu);
    setReg(2, 200u);
    setMem(200,  0x11223344u);
    execute(encodeSH_idx(4, 2, 2, /*pre=*/false));
    EXPECT_EQ(getMem(200), 0xABCD3344u);
    EXPECT_EQ(getReg(2), 202u);
}

// --- Walking a buffer with post-increment (one full word = 4 bytes) ---
TEST_F(CPUFixture, LW_PostIndex_WalksBuffer)
{
    setReg(2, 100u);
    setMem(100,  0xAAAAAAAAu);
    setMem(104,  0xBBBBBBBBu);
    execute(encodeLW_idx(4, 2, 4, /*pre=*/false)); // load 100, base -> 104
    EXPECT_EQ(getReg(4), 0xAAAAAAAAu);
    execute(encodeLW_idx(5, 2, 4, /*pre=*/false)); // load 104, base -> 108
    EXPECT_EQ(getReg(5), 0xBBBBBBBBu);
    EXPECT_EQ(getReg(2), 108u);
}
