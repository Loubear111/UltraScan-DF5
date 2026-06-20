#include "test_helpers.h"

// ============================================================
// Un-aligned load/store combine family: lcb/lcw/lce, scb/scw/sce
// (OP_SYS sub-ops). LCR = sr[1], SCR = sr[2].
//
// This emulator is word-addressed, so the architecture's byte-lane
// merge is modeled as a little-endian un-aligned access: rA is a
// byte address, the accessed word is slot rA>>2, rA post-increments
// by 4, and off = rA&3 selects how the word straddles two slots.
// lcb/lcw and scb/sce therefore round-trip the same value.
//
// Memory layout used by the tests: register indices (2,3,4,5) hold
// byte addresses; the data words live in high slots (50,51,52) to
// avoid colliding with the register-backed low slots.
// ============================================================

// ---- Load combine: un-aligned read at byte offset 1 ----
TEST_F(CPUFixture, LCB_LCW_ReconstructsUnalignedWord)
{
    setReg(2, 201);                 // base byte address, off = 1, slot = 50
    setReg(50, 0xAABBCCDDu);        // first aligned word
    setReg(51, 0x11223344u);        // second aligned word
    execute(encodeLCB(2));          // LCR = mem[50]; rA -> 205
    execute(encodeLCW(3, 2));       // r3 = unaligned word; LCR = mem[51]; rA -> 209
    EXPECT_EQ(getReg(3), 0x44AABBCCu);
    EXPECT_EQ(getReg(2), 209u);     // base advanced by 4 twice
    EXPECT_EQ(bus.cpu.sr[1], 0x11223344u);
}

TEST_F(CPUFixture, LCB_SetsLoadCombineRegister)
{
    setReg(2, 200);                 // aligned
    setReg(50, 0xDEADBEEFu);
    execute(encodeLCB(2));
    EXPECT_EQ(bus.cpu.sr[1], 0xDEADBEEFu);
    EXPECT_EQ(getReg(2), 204u);
}

// ---- lce: aligned case returns LCR untouched ----
TEST_F(CPUFixture, LCE_AlignedReturnsLCR)
{
    setReg(2, 200);                 // off = 0 throughout
    setReg(50, 0xCAFEBABEu);
    execute(encodeLCB(2));          // LCR = mem[50]; rA -> 204 (still aligned)
    execute(encodeLCE(3, 2));       // off==0 -> r3 = LCR, LCR unchanged
    EXPECT_EQ(getReg(3), 0xCAFEBABEu);
    EXPECT_EQ(bus.cpu.sr[1], 0xCAFEBABEu);
}

// ---- lce: un-aligned case behaves like lcw ----
TEST_F(CPUFixture, LCE_UnalignedCombines)
{
    setReg(2, 201);
    setReg(50, 0xAABBCCDDu);
    setReg(51, 0x11223344u);
    execute(encodeLCB(2));
    execute(encodeLCE(3, 2));
    EXPECT_EQ(getReg(3), 0x44AABBCCu);
}

// ---- Store combine: un-aligned write at byte offset 1 ----
TEST_F(CPUFixture, SCB_SCE_StoreUnalignedWordPreservingNeighbors)
{
    setReg(2, 201);                 // off = 1, slot = 50
    setReg(50, 0x000000DDu);        // low byte (addr 200) must be preserved
    setReg(51, 0x11000000u);        // high bytes (addr 205..207) must be preserved
    setReg(3, 0x44AABBCCu);         // value to store
    execute(encodeSCB(3, 2));       // writes slot 50 low part; SCR = value; rA -> 205
    execute(encodeSCE(2));          // writes slot 51 high part; rA -> 209
    EXPECT_EQ(getReg(50), 0xAABBCCDDu);
    EXPECT_EQ(getReg(51), 0x11000044u);
    EXPECT_EQ(bus.cpu.sr[2], 0x44AABBCCu);
    EXPECT_EQ(getReg(2), 209u);
}

TEST_F(CPUFixture, SCB_AlignedStoresFullWord)
{
    setReg(2, 200);
    setReg(3, 0xDEADBEEFu);
    execute(encodeSCB(3, 2));       // off==0 -> full word write; rA -> 204
    execute(encodeSCE(2));          // off==0 -> no write; rA -> 208
    EXPECT_EQ(getReg(50), 0xDEADBEEFu);
    EXPECT_EQ(getReg(2), 208u);
}

// ---- scw: middle-word combine of SCR and rD ----
TEST_F(CPUFixture, SCW_CombinesScrAndSource)
{
    setReg(2, 201);                 // off = 1, slot = 50
    bus.cpu.sr[2] = 0x44AABBCCu;    // SCR preset
    setReg(3, 0x8899AABBu);
    setReg(50, 0);
    execute(encodeSCW(3, 2));
    EXPECT_EQ(getReg(50), 0x99AABB44u);
    EXPECT_EQ(bus.cpu.sr[2], 0x8899AABBu);   // SCR updated to source
    EXPECT_EQ(getReg(2), 205u);
}

// ---- Full streaming round-trip: store two words un-aligned, read back ----
TEST_F(CPUFixture, StoreStream_ThenLoadStream_RoundTrips)
{
    const uint32_t W0 = 0x44332211u;
    const uint32_t W1 = 0x88776655u;

    // execute() writes each instruction to ram[pc] and advances pc, which would
    // eventually clobber the low register-backed slots. Keep every instruction
    // in ram[0] by resetting pc; the base/destination registers persist.
    auto step = [&](uint32_t word) { bus.cpu.pc = 0; execute(word); };

    // Store W0, W1 at byte offset 1 (slots 50,51,52) via scb/scw/sce.
    setReg(2, 201);
    setReg(50, 0x000000AAu);        // preserved low byte at addr 200
    setReg(52, 0xBBCCDD00u);        // preserved high bytes at addr 209..211
    setReg(3, W0);
    setReg(4, W1);
    step(encodeSCB(3, 2));          // slot 50
    step(encodeSCW(4, 2));          // slot 51
    step(encodeSCE(2));             // slot 52

    // Read the two words back from the same base.
    setReg(2, 201);
    step(encodeLCB(2));
    step(encodeLCW(5, 2));          // r5 = W0
    step(encodeLCW(6, 2));          // r6 = W1
    EXPECT_EQ(getReg(5), W0);
    EXPECT_EQ(getReg(6), W1);
}
