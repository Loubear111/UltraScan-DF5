// Fault diagnostics (plan story 10): the unimplemented funnel, hang detection
// and the instruction trace, plus their text reports.
#include "test_helpers.h"
#include "FaultReport.h"

static constexpr uint32_t kBadFunc6 = 0x80008000u | (0x06u << 1);   // func6 6 is undefined

TEST_F(CPUFixture, Unimplemented_RecordsEventAndRaisesReservedInstruction)
{
    bus.cpu.pc = 0x40;
    execute(kBadFunc6);

    auto ev = bus.cpu.faultEvents();
    ASSERT_EQ(ev.size(), 1u);
    EXPECT_EQ(ev[0].kind, spg290::FaultEvent::Unimplemented);
    EXPECT_EQ(ev[0].pc, 0x40u);
    EXPECT_EQ(ev[0].word, kBadFunc6);
    EXPECT_EQ(ev[0].what, "special func6");
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 9u);
    EXPECT_EQ(bus.cpu.cr[5], 0x40u);
    EXPECT_FALSE(bus.cpu.stopRequested());
}

TEST_F(CPUFixture, StopOnUnimplemented_SetsFlagInsteadOfRaising)
{
    bus.cpu.setStopOnUnimplemented(true);
    execute(kBadFunc6);
    EXPECT_TRUE(bus.cpu.stopRequested());
    EXPECT_EQ(bus.cpu.cr[2] & spg290::ECR_CODE_MASK, 0u);   // no exception taken
    EXPECT_EQ(bus.cpu.faultEvents().size(), 1u);

    bus.cpu.clearStop();
    EXPECT_FALSE(bus.cpu.stopRequested());
}

TEST_F(CPUFixture, UndefinedOpcodes_AndSixteenBitForms_GoThroughTheFunnel)
{
    execute(0x80008000u | (0x1Cu << 26));      // custom engine opcode
    bus.cpu.pc = 0;
    bus.write32(0, 0);                         // clear the 32-bit word first so the P-bits are 0/0
    execute16(0x1000);                         // 16-bit OP 1
    auto ev = bus.cpu.faultEvents();
    ASSERT_EQ(ev.size(), 2u);
    EXPECT_EQ(ev[0].what, "opcode");
    EXPECT_EQ(ev[1].what, "16-bit OP 1");
    EXPECT_EQ(ev[1].word, 0x1000u);
}

TEST_F(CPUFixture, UndefinedIFormFunc3_RaisesInBothIFormOpcodes)
{
    // opcode 1 func3 1 and opcode 5 func3 1 are undefined
    execute(0x80008000u | (0x01u << 26) | (1u << 18));
    bus.cpu.pc = 0;
    execute(0x80008000u | (0x05u << 26) | (1u << 18));
    EXPECT_EQ(bus.cpu.faultEvents().size(), 2u);
}

TEST_F(CPUFixture, FaultEventRingKeepsTheLastSixtyFour)
{
    for (int i = 0; i < 70; ++i)
    {
        bus.cpu.pc = (uint32_t)(i * 4);
        execute(kBadFunc6);
    }
    auto ev = bus.cpu.faultEvents();
    ASSERT_EQ(ev.size(), spg290::FAULT_EVENT_CAP);
    EXPECT_EQ(ev.front().pc, 6u * 4u);          // events 0-5 were dropped
    EXPECT_EQ(ev.back().pc, 69u * 4u);
}

TEST_F(CPUFixture, BranchToSelf_IsReportedAsHangWithLastCause)
{
    bus.cpu.cr[2] = 9;                          // last exception: RI
    bus.cpu.pc = 0x100;
    bus.write16(0x100, encodeB16(15, 0));       // b! to itself
    for (int i = 0; i < 15; ++i) bus.cpu.clock();
    EXPECT_FALSE(bus.cpu.hangDetected());
    bus.cpu.clock();                            // 16th consecutive self-branch
    EXPECT_TRUE(bus.cpu.hangDetected());

    auto ev = bus.cpu.faultEvents();
    ASSERT_EQ(ev.size(), 1u);
    EXPECT_EQ(ev[0].kind, spg290::FaultEvent::Hang);
    EXPECT_EQ(ev[0].pc, 0x100u);
    EXPECT_EQ(ev[0].cause & spg290::ECR_CODE_MASK, 9u);

    for (int i = 0; i < 50; ++i) bus.cpu.clock();   // reported once per loop
    EXPECT_EQ(bus.cpu.faultEvents().size(), 1u);
}

TEST_F(CPUFixture, JumpToSelf_32Bit_IsAlsoAHang)
{
    bus.cpu.setHangThreshold(4);
    bus.cpu.pc = 0x100;
    bus.write32(0x100, encodeJX(0x80, false));
    for (int i = 0; i < 4; ++i) bus.cpu.clock();
    EXPECT_TRUE(bus.cpu.hangDetected());
}

TEST_F(CPUFixture, HangThresholdZeroDisablesDetection_AndMovingLoopsAreNotHangs)
{
    bus.cpu.setHangThreshold(0);
    bus.cpu.pc = 0x100;
    bus.write16(0x100, encodeB16(15, 0));
    for (int i = 0; i < 100; ++i) bus.cpu.clock();
    EXPECT_FALSE(bus.cpu.hangDetected());

    bus.cpu.setHangThreshold(4);
    bus.cpu.pc = 0x200;
    bus.write16(0x200, encodeNOP16());
    bus.write16(0x202, encodeB16(15, -1));      // loops back over two instructions
    for (int i = 0; i < 100; ++i) bus.cpu.clock();
    EXPECT_FALSE(bus.cpu.hangDetected());
}

TEST_F(CPUFixture, StopOnHang_RequestsStop)
{
    bus.cpu.setStopOnHang(true);
    bus.cpu.setHangThreshold(3);
    bus.cpu.pc = 0x100;
    bus.write16(0x100, encodeB16(15, 0));
    for (int i = 0; i < 3; ++i) bus.cpu.clock();
    EXPECT_TRUE(bus.cpu.stopRequested());
}

TEST_F(CPUFixture, Trace_KeepsTheLastNInstructions)
{
    bus.cpu.setTraceDepth(3);
    for (int i = 0; i < 5; ++i)
        execute(encodeNOP());
    auto t = bus.cpu.traceEntries();
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].pc, 8u);
    EXPECT_EQ(t[2].pc, 16u);
    EXPECT_EQ(t[0].word, 0x80008000u);
    EXPECT_EQ(t[0].size, 4);

    bus.cpu.setTraceDepth(0);
    EXPECT_TRUE(bus.cpu.traceEntries().empty());
    execute(encodeNOP());
    EXPECT_TRUE(bus.cpu.traceEntries().empty());
}

TEST_F(CPUFixture, TraceReport_ShowsDisassembly)
{
    bus.cpu.setTraceDepth(4);
    execute(encodeNOP());
    execute16(encodeLDIU16(1, 5));
    const std::string s = formatTrace(bus.cpu);
    EXPECT_NE(s.find("00000000:  80008000  nop"), std::string::npos) << s;
    EXPECT_NE(s.find("00000004:  5105      ldiu! r1, 0x05"), std::string::npos) << s;
}

TEST_F(CPUFixture, FaultReport_ShowsPcWordDisassemblyAndCause)
{
    bus.cpu.pc = 0x90;
    execute(kBadFunc6);
    const std::string s = formatFaultEvents(bus.cpu);
    EXPECT_NE(s.find("Unimplemented at PC 0x00000090"), std::string::npos) << s;
    EXPECT_NE(s.find("word 0x8000800C"), std::string::npos) << s;
    EXPECT_NE(s.find("special func6"), std::string::npos) << s;
    EXPECT_NE(s.find("undefined"), std::string::npos) << s;   // the disassembler agrees it is undefined
}

TEST_F(CPUFixture, HangReport_NamesTheLastExceptionCause)
{
    bus.cpu.cr[2] = 7;                          // syscall
    bus.cpu.setHangThreshold(2);
    bus.cpu.pc = 0x100;
    bus.write16(0x100, encodeB16(15, 0));
    bus.cpu.clock();
    bus.cpu.clock();
    const std::string s = formatFaultEvents(bus.cpu);
    EXPECT_NE(s.find("Hang at PC 0x00000100"), std::string::npos) << s;
    EXPECT_NE(s.find("syscall (7)"), std::string::npos) << s;
}

TEST(ExceptionNames, KnownAndUnknownCodes)
{
    EXPECT_STREQ(exceptionName(9), "RI");
    EXPECT_STREQ(exceptionName(20), "interrupt");
    EXPECT_STREQ(exceptionName(31), "unknown");
}
