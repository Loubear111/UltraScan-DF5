// BIOS-call tracking in System::run (Phase 5, stories 19 and 20).
#include <vector>

#include "test_helpers.h"
#include "System.h"

namespace {
constexpr uint32_t kStub = 0x900;      // inside System::CALL_REGION_LO..HI

// Writes the words from address 0 on a big-endian bus (the test default), then resets.
void load(System& s, const std::vector<uint32_t>& words, uint32_t entry = 0)
{
    uint32_t a = 0;
    for (uint32_t w : words)
    {
        s.bus.write32(a, w);
        a += 4;
    }
    s.reset(entry);
}

// 0: ldi r4,5   4: ldi r5,9   8: jl kStub   C: ldi r6,7   10: spin
std::vector<uint32_t> callOnce()
{
    return { encodeLDI(4, 5), encodeLDI(5, 9), encodeJX(kStub >> 1, true), encodeLDI(6, 7), encodeJX(0x10 >> 1, false) };
}
} // namespace

TEST(BiosCalls, NothingIsTrackedUnlessAskedFor)
{
    System s(1u << 20);
    load(s, callOnce());
    s.bus.write32(kStub, encodeJX(kStub >> 1, false));          // spin if the call is made
    s.run(0, 50);
    EXPECT_EQ(s.totalCalls(), 0u);
    EXPECT_TRUE(s.callStats().empty());
}

TEST(BiosCalls, StopOnCallReportsTargetCallerAndArguments)
{
    System s(1u << 20);
    load(s, callOnce());
    s.stopOnCall = true;
    const System::RunStats st = s.run(0, 100);

    EXPECT_EQ(st.reason, System::StopReason::BiosCall);
    EXPECT_EQ(st.instructions, 3u);                              // ldi, ldi, jl
    ASSERT_EQ(s.callStats().size(), 1u);
    const System::CallStat& c = s.callStats()[0];
    EXPECT_EQ(c.target, kStub);
    EXPECT_EQ(c.count, 1u);
    EXPECT_EQ(c.first.caller, 0xCu);                             // address after the jl
    EXPECT_EQ(c.first.args[0], 5u);
    EXPECT_EQ(c.first.args[1], 9u);
    EXPECT_EQ(s.bus.cpu.pc, kStub);                              // stopped before running the stub
    ASSERT_EQ(s.callSequence().size(), 1u);
}

TEST(BiosCalls, LogOnlyKeepsRunningAndCountsRepeats)
{
    System s(1u << 20);
    // Two calls to the same stub, which is a bare `br r3` (return), then a spin.
    load(s, { encodeJX(kStub >> 1, true), encodeJX(kStub >> 1, true), encodeJX(8 >> 1, false) });
    s.bus.write32(kStub, encodeBR(15, 3));                       // br r3
    s.logCalls = true;
    s.run(0, 60);
    EXPECT_EQ(s.totalCalls(), 2u);
    ASSERT_EQ(s.callStats().size(), 1u);
    EXPECT_EQ(s.callStats()[0].count, 2u);
    EXPECT_EQ(s.callSequence().size(), 2u);
    EXPECT_EQ(s.callSequence()[1].caller, 8u);
}

TEST(BiosCalls, StubReturnsZeroInR4AndSkipsTheStubBody)
{
    System s(1u << 20);
    load(s, callOnce());
    s.bus.write32(kStub, encodeLDI(6, 99));                      // would clobber r6 if it ran
    s.stubCalls = true;
    const System::RunStats st = s.run(0, 40);

    EXPECT_EQ(st.reason, System::StopReason::InstructionLimit);
    EXPECT_EQ(s.totalCalls(), 1u);
    EXPECT_EQ(s.bus.cpu.regs[4], 0u);                            // was 5 before the call
    EXPECT_EQ(s.bus.cpu.regs[6], 7u);                            // the instruction after the call ran
}

TEST(BiosCalls, StubRampGivesGrowingResultsPerTarget)
{
    System s(1u << 20);
    load(s, { encodeJX(kStub >> 1, true), encodeJX(kStub >> 1, true), encodeJX(8 >> 1, false) });
    s.stubCalls = true;
    s.stubRamp = 7;
    s.run(0, 30);
    EXPECT_EQ(s.totalCalls(), 2u);
    EXPECT_EQ(s.bus.cpu.regs[4], 7u);                            // second call: 1 * 7
}

TEST(BiosCalls, StubWithNoUsableReturnAddressStops)
{
    System s(1u << 20);
    load(s, { encodeJX(kStub >> 1, false) });                    // j, so r3 is still 0
    s.stubCalls = true;
    const System::RunStats st = s.run(0, 100);
    EXPECT_EQ(st.reason, System::StopReason::BadCallReturn);
    EXPECT_EQ(s.totalCalls(), 1u);
}

TEST(BiosCalls, ExceptionVectorAreaIsNotACall)
{
    System s(1u << 20);
    load(s, { encodeJX(0x600 >> 1, false) });                    // 0x600 is below CALL_REGION_LO
    s.bus.write32(0x600, encodeJX(0x600 >> 1, false));
    s.stopOnCall = true;
    const System::RunStats st = s.run(0, 50);
    EXPECT_EQ(st.reason, System::StopReason::InstructionLimit);
    EXPECT_EQ(s.totalCalls(), 0u);
}

TEST(BiosCalls, TheUncachedWindowBitsAreIgnored)
{
    System s(1u << 20);
    // Running at 0xA0000000 (physical 0): the jl lands on 0xA0000900.
    load(s, { encodeJX(kStub >> 1, true) }, 0xA0000000u);
    s.stopOnCall = true;
    s.run(0, 10);
    ASSERT_EQ(s.callStats().size(), 1u);
    EXPECT_EQ(s.callStats()[0].target, 0xA0000900u);
}
