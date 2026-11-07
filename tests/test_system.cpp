// Bus::tick, BusMemory, CPU reset and the System run loop (plan story 13).
#include <vector>

#include "test_helpers.h"
#include "System.h"
#include "roms/generated/test_irq.h"

namespace {
constexpr uint32_t kLine = VideoTiming::CPU_CYCLES_PER_LINE;

// A one-instruction spin loop at address 0 on a big-endian system.
void loadSpin(System& s)
{
    s.bus.write32(0, encodeJX(0, false));
    s.reset(0);
}
} // namespace

// ---- BusMemory -----------------------------------------------------------
TEST(BusMemoryAdapter, ComposesLittleEndianWhateverTheBusByteOrder)
{
    for (bool le : { false, true })
    {
        Bus b;
        b.setLittleEndian(le);
        const uint8_t bytes[4] = { 0x34, 0x12, 0x78, 0x56 };
        for (int i = 0; i < 4; ++i)
            b.write8(0x100 + i, bytes[i]);
        BusMemory m(b);
        EXPECT_EQ(m.readByte(0x101), 0x12u);
        EXPECT_EQ(m.readHalf(0x100), 0x1234u) << "le=" << le;
        EXPECT_EQ(m.readWord(0x100), 0x56781234u) << "le=" << le;
    }
}

// ---- Bus::tick -----------------------------------------------------------
TEST(BusTick, VblankStartRaisesFrameReadyStatusAndIrq)
{
    Bus b;
    b.ppu.write32(PPU::BASE + PPU::P_IRQ_Control, PPU::IRQ_BLK_ST);
    b.irqc.write32(IRQController::P_I_MASKL, 0xFFFFFBFFu);          // unmask source 10
    b.cpu.cr[0] |= spg290::PSR_IEC;

    for (uint32_t i = 0; i < VideoTiming::ACTIVE_LINES - 1; ++i)
        b.tick(kLine);
    EXPECT_FALSE(b.takeFrameReady());
    EXPECT_FALSE(b.ppu.blkStartLine());

    b.tick(kLine);
    EXPECT_TRUE(b.ppu.blkStartLine());
    EXPECT_TRUE(b.irqc.irqActive());
    EXPECT_EQ(b.irqc.activeVector(), IRQController::vectorOf(IRQController::SRC_PPU_VBLK_START));
    EXPECT_TRUE(b.takeFrameReady());
    EXPECT_FALSE(b.takeFrameReady());                                // reading clears it
}

TEST(BusTick, FrameReadyEvenWhenTheVblankInterruptIsNotEnabled)
{
    Bus b;
    for (uint32_t i = 0; i < VideoTiming::ACTIVE_LINES; ++i)
        b.tick(kLine);
    EXPECT_TRUE(b.takeFrameReady());
    EXPECT_FALSE(b.ppu.blkStartLine());
}

// ---- CPU reset -----------------------------------------------------------
TEST(CpuReset, ClearsStateAndLoadsThePc)
{
    spg290 cpu;
    cpu.regs[0] = 5;  cpu.regs[5] = 7;
    cpu.cr[0] = 1;    cpu.cr[3] = 0xA0000000u;
    cpu.sr[1] = 9;    cpu.status = 0x1F;
    cpu.ceh = 3;      cpu.cel = 4;
    cpu.pc = 0x500;
    cpu.setIRQ(true, 12);
    cpu.reset(0x1234);

    EXPECT_EQ(cpu.pc, 0x1234u);
    EXPECT_EQ(cpu.regs[0], 0u);
    EXPECT_EQ(cpu.regs[5], 0u);
    EXPECT_EQ(cpu.cr[0], 0u);
    EXPECT_EQ(cpu.cr[3], 0u);
    EXPECT_EQ(cpu.sr[1], 0u);
    EXPECT_EQ(cpu.status, 0u);
    EXPECT_EQ(cpu.ceh, 0u);
    EXPECT_EQ(cpu.cel, 0u);
    EXPECT_FALSE(cpu.isHalted());
    EXPECT_TRUE(cpu.faultEvents().empty());
}

// ---- System --------------------------------------------------------------
TEST(SystemRun, FramesArriveAtTheVideoCadence)
{
    System s(1u << 20);
    loadSpin(s);
    int callbacks = 0;
    const System::RunStats st = s.run(3, 0, [&](const std::vector<PPU::Color>& fb, int w, int h, uint64_t n) {
        EXPECT_EQ(fb.size(), (size_t)w * (size_t)h);
        EXPECT_EQ(n, (uint64_t)callbacks);
        ++callbacks;
    });
    EXPECT_EQ(callbacks, 3);
    EXPECT_EQ(st.frames, 3u);
    EXPECT_EQ(st.reason, System::StopReason::FrameLimit);
    // The third vblank starts at line 480 of the third frame.
    const uint64_t expected = (2ull * VideoTiming::LINES_PER_FRAME + VideoTiming::ACTIVE_LINES) * kLine
                              / System::CYCLES_PER_INSTRUCTION;
    EXPECT_EQ(st.instructions, expected);
    EXPECT_EQ(st.cpuCycles, st.instructions * System::CYCLES_PER_INSTRUCTION);
}

TEST(SystemRun, InstructionCapStopsTheRun)
{
    System s(1u << 20);
    loadSpin(s);
    const System::RunStats st = s.run(0, 1000);
    EXPECT_EQ(st.reason, System::StopReason::InstructionLimit);
    EXPECT_EQ(st.instructions, 1000u);
    EXPECT_EQ(st.frames, 0u);
}

TEST(SystemRun, RealVblankInterruptEndToEnd)
{
    System s(1u << 20);
    s.bus.loadROM(rom_test_irq, rom_test_irq_len);
    s.reset(0);
    s.run(4, 0);
    EXPECT_GE(s.bus.read32(0x2000), 3u);                              // IRQ counter
    EXPECT_EQ(s.bus.read32(ROM_RESULT_ADDR), PASS_VALUE);
    EXPECT_EQ(s.bus.read32(0x2004) & spg290::ECR_CODE_MASK, 7u);      // saved ECR: syscall
}

TEST(SystemRun, FrameCallbackGetsTheTveComposedFrame)
{
    System s(1u << 20);
    loadSpin(s);
    s.bus.write8(0x10000, 0x00);
    s.bus.write8(0x10001, 0xF8);                                      // RGB565 0xF800: red
    s.bus.tve.write32(TVE::START0, 0x10000);
    s.bus.tve.write32(TVE::BUFSEL, 0);

    int calls = 0, fw = 0, fh = 0;
    PPU::Color first;
    s.run(1, 0, [&](const std::vector<PPU::Color>& fb, int w, int h, uint64_t) {
        ++calls; fw = w; fh = h; first = fb[0];
    });
    int w = 0, h = 0;
    const auto direct = s.bus.tve.compose(s.bus, nullptr, w, h);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(fw, 320);
    EXPECT_EQ(fh, 240);
    EXPECT_EQ(first, direct[0]);
    EXPECT_EQ(first.r, 255);
}

TEST(SystemRun, UnimplementedInstructionStopsWithCpuFault)
{
    System s(1u << 20);
    s.bus.cpu.setStopOnUnimplemented(true);
    s.bus.write32(0, 0x80008000u | (0x06u << 1));                     // undefined func6
    s.reset(0);
    const System::RunStats st = s.run(5, 0);
    EXPECT_EQ(st.reason, System::StopReason::CpuFault);
    EXPECT_EQ(st.instructions, 1u);
    EXPECT_EQ(s.bus.cpu.faultEvents().size(), 1u);
}

TEST(SystemRun, SelfBranchStopsWithHangWhenAsked)
{
    System s(1u << 20);
    loadSpin(s);
    s.bus.cpu.setStopOnHang(true);
    s.bus.cpu.setHangThreshold(8);
    const System::RunStats st = s.run(5, 0);
    EXPECT_EQ(st.reason, System::StopReason::Hang);
    EXPECT_EQ(st.instructions, 8u);
}

TEST(SystemRun, AnUnboundedRequestGetsTheDefaultCap)
{
    EXPECT_EQ(System::DEFAULT_INSTRUCTION_CAP, 100000000ull);
}

TEST(SystemBoot, ResetsTheMachineLoadsTheImageAndAppliesOverrides)
{
    System s(Bus::SYSTEM_RAM_SIZE);
    s.bus.cpu.regs[5] = 99;
    s.bus.cpu.cr[0] = 1;

    std::vector<uint8_t> img(0x1000, 0xAB);
    BootParams p;
    p.cr3 = 0xA0000000u;
    ASSERT_TRUE(s.boot(img.data(), img.size(), p));
    EXPECT_EQ(s.bus.cpu.regs[5], 0u);
    EXPECT_EQ(s.bus.cpu.cr[0], 0u);
    EXPECT_EQ(s.bus.cpu.cr[3], 0xA0000000u);
    EXPECT_EQ(s.bus.cpu.pc, 0xA0091000u);
    EXPECT_EQ(s.bus.read8(0xA00901FC), 0xABu);
}

TEST(SystemBoot, ImageThatDoesNotFitLeavesTheMachineAlone)
{
    System s(1024u);
    s.bus.cpu.regs[5] = 99;
    s.bus.cpu.pc = 0x44;
    std::vector<uint8_t> img(4096, 1);
    EXPECT_FALSE(s.boot(img.data(), img.size()));
    EXPECT_EQ(s.bus.cpu.regs[5], 99u);
    EXPECT_EQ(s.bus.cpu.pc, 0x44u);
}

TEST(SystemRun, UnmappedAccessesAreCollectedForTheSummary)
{
    System s(1u << 20);
    s.bus.write32(0, encodeLW(4, 5, 0));                              // lw r4, [r5]
    s.reset(0);
    s.bus.cpu.regs[5] = 0x08210030u;                                  // an unmodelled I/O page
    s.run(0, 1);
    const std::string summary = s.bus.unmappedSummary();
    EXPECT_NE(summary.find("0x08210000"), std::string::npos) << summary;
}
