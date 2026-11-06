// Fetch and instruction-size classification (plan 2.1): the aligned word's two
// P-bits (w[31], w[15]) pick 16+16, invalid, parallel pair or 32-bit.
#include "test_helpers.h"

namespace {
constexpr uint32_t kPC = 0x100;

// Parallel pair word: P31 = 1, P15 = 0, high and low 15-bit payloads.
uint32_t parallelWord(uint16_t high15, uint16_t low15)
{
    return 0x80000000u | ((uint32_t)(high15 & 0x7FFF) << 16) | (low15 & 0x7FFF);
}
} // namespace

// The register effect of a 16-bit instruction depends on decode16(), which is rewritten
// in story 6; these tests check what was fetched (cpu.instr) and where pc went.
class FetchTest : public CPUFixture, public ::testing::WithParamInterface<bool>
{
protected:
    void SetUp() override
    {
        bus.setLittleEndian(GetParam());
        bus.cpu.pc = kPC;
    }
    void clockOnce() { bus.cpu.clock(); }
};

INSTANTIATE_TEST_SUITE_P(BusByteOrder, FetchTest, ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<bool>& i) { return i.param ? "LittleEndian" : "BigEndian"; });

TEST_P(FetchTest, TwoSixteenBitInstructionsAdvanceByTwoEach)
{
    bus.write16(kPC,     encodeLDIU16(1, 5));
    bus.write16(kPC + 2, encodeLDIU16(2, 6));

    clockOnce();
    EXPECT_EQ(bus.cpu.pc, kPC + 2);
    EXPECT_EQ(bus.cpu.lastInstr(), (uint32_t)encodeLDIU16(1, 5));

    clockOnce();
    EXPECT_EQ(bus.cpu.pc, kPC + 4);
    EXPECT_EQ(bus.cpu.lastInstr(), (uint32_t)encodeLDIU16(2, 6));
}

TEST_P(FetchTest, ThirtyTwoBitInstructionAdvancesByFour)
{
    bus.write32(kPC, encodeLDI(3, 0x1234));
    clockOnce();
    EXPECT_EQ(bus.cpu.pc, kPC + 4);
    EXPECT_EQ(bus.cpu.lastInstrSize(), 4);
    EXPECT_EQ(getReg(3), 0x1234u);
}

TEST_P(FetchTest, SixteenBitInstructionAtHalfwordAddressIgnoresWordPBits)
{
    // The aligned word has P-bits that would otherwise classify it as 32-bit
    // (BE) or invalid (LE); at pc&2 the instruction is still the halfword.
    bus.write16(kPC,     0x8000);
    bus.write16(kPC + 2, encodeLDIU16(4, 9));
    bus.cpu.pc = kPC + 2;

    clockOnce();
    EXPECT_EQ(bus.cpu.pc, kPC + 4);
    EXPECT_EQ(bus.cpu.lastInstrSize(), 2);
    EXPECT_EQ(bus.cpu.lastInstr(), (uint32_t)encodeLDIU16(4, 9));
}

TEST_P(FetchTest, InvalidPBitPairRaisesPEL)
{
    // w[31] = 0, w[15] = 1; write32 composes the word in the bus byte order.
    bus.write32(kPC, 0x00008000u);

    clockOnce();
    EXPECT_EQ(bus.cpu.cr[5], kPC);   // EPC = address of the faulting word; 6 = P_EL
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 6u);
}

TEST_P(FetchTest, ParallelPairRunsLowHalfWhenTIsSet)
{
    bus.write32(kPC, parallelWord(encodeLDIU16(2, 0x22), encodeLDIU16(1, 0x11)));
    bus.cpu.status |= spg290::T;

    clockOnce();
    EXPECT_EQ(bus.cpu.pc, kPC + 4);
    EXPECT_EQ(bus.cpu.lastInstr(), (uint32_t)encodeLDIU16(1, 0x11));
}

TEST_P(FetchTest, ParallelPairRunsHighHalfWhenTIsClear)
{
    bus.write32(kPC, parallelWord(encodeLDIU16(2, 0x22), encodeLDIU16(1, 0x11)));
    bus.cpu.status &= ~spg290::T;

    clockOnce();
    EXPECT_EQ(bus.cpu.pc, kPC + 4);
    EXPECT_EQ(bus.cpu.lastInstr(), (uint32_t)encodeLDIU16(2, 0x22));
}
