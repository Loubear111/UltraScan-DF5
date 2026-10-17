#include "test_helpers.h"

// ============================================================
// PPU interrupt sources, bus address aliasing, and the full
// PPU -> controller -> CPU path through the bus.
// ============================================================

static constexpr uint32_t VEC_BASE     = 0x1000;
static constexpr uint32_t VEC53_MODE16 = VEC_BASE + 0x200 + 0x10 * 54;

static constexpr uint32_t PPU_CTRL   = PPU::BASE + PPU::P_IRQ_Control;
static constexpr uint32_t PPU_STATUS = PPU::BASE + PPU::P_IRQ_Status;
static constexpr uint32_t IRQ_MASKL  = IRQController::BASE + IRQController::P_I_MASKL;

// ---- Bus address aliasing ----
TEST_F(CPUFixture, Bus_UncachedAliasReachesRam)
{
    bus.write32(0x100, 0xCAFEF00Du);
    EXPECT_EQ(bus.read32(0xA0000100u), 0xCAFEF00Du);
    bus.write32(0xA0000104u, 0x12345678u);
    EXPECT_EQ(bus.read32(0x104), 0x12345678u);
}

TEST_F(CPUFixture, Bus_PPUReachableAtBothAliases)
{
    bus.write32(0x88010000u + PPU::P_SP_Max, 0x55u);
    EXPECT_EQ(bus.read32(0x08010000u + PPU::P_SP_Max), 0x55u);
}

// ---- PPU IRQ sources ----
TEST(PPUIrq, EventWithEnableClearLatchesNothing)
{
    PPU ppu;
    ppu.signalVBlankStart();
    EXPECT_EQ(ppu.read32(PPU_STATUS), 0u);
    EXPECT_FALSE(ppu.blkStartLine());
}

TEST(PPUIrq, EventLatchesOnlyItsOwnBit)
{
    PPU ppu;
    ppu.write32(PPU_CTRL, PPU::IRQ_MASK);
    ppu.signalVBlankEnd();
    EXPECT_EQ(ppu.read32(PPU_STATUS), PPU::IRQ_BLK_END);
    EXPECT_TRUE(ppu.blkEndLine());
    EXPECT_FALSE(ppu.blkStartLine());
    EXPECT_FALSE(ppu.hvLine());
}

TEST(PPUIrq, StatusIsWriteOneToClear)
{
    PPU ppu;
    ppu.write32(PPU_CTRL, PPU::IRQ_MASK);
    ppu.signalVBlankStart();
    ppu.signalHVHit();
    ppu.write32(PPU_STATUS, 0);                     // writing 0 changes nothing
    EXPECT_EQ(ppu.read32(PPU_STATUS), PPU::IRQ_BLK_ST | PPU::IRQ_HV);
    ppu.write32(PPU_STATUS, PPU::IRQ_BLK_ST);
    EXPECT_EQ(ppu.read32(PPU_STATUS), PPU::IRQ_HV);
    EXPECT_FALSE(ppu.blkStartLine());
    EXPECT_TRUE(ppu.hvLine());
}

TEST(PPUIrq, DisablingEnableDropsLine)
{
    PPU ppu;
    ppu.write32(PPU_CTRL, PPU::IRQ_BLK_ST);
    ppu.signalVBlankStart();
    EXPECT_TRUE(ppu.blkStartLine());
    ppu.write32(PPU_CTRL, 0);
    EXPECT_FALSE(ppu.blkStartLine());
}

TEST_F(CPUFixture, PPUIrq_ByteWriteDoesNotClearOtherLanesOrBits)
{
    bus.write32(PPU_CTRL, PPU::IRQ_MASK);
    bus.ppu.signalVBlankStart();
    bus.ppu.signalVBlankEnd();
    // Big-endian: byte offset 3 is the least-significant lane.
    bus.write8(PPU_STATUS + 3, (uint8_t)PPU::IRQ_BLK_ST);
    EXPECT_EQ(bus.read32(PPU_STATUS), PPU::IRQ_BLK_END);
}

// ---- Full path ----
class IrqPathFixture : public CPUFixture
{
protected:
    void SetUp() override
    {
        bus.cpu.cr[3] = VEC_BASE | 1;
        bus.write32(PPU_CTRL, PPU::IRQ_BLK_ST);
        bus.write32(IRQ_MASKL, ~(1u << IRQController::SRC_PPU_VBLK_START));
        bus.write32(0, encodeADDIX(1, 5, false));
    }

    void raiseVBlank()
    {
        bus.ppu.signalVBlankStart();
        bus.updateIRQ();
    }
};

TEST_F(IrqPathFixture, VBlankEntersHandlerAtVector53)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    raiseVBlank();
    bus.cpu.clock();
    EXPECT_EQ(bus.cpu.pc, VEC53_MODE16);
    EXPECT_EQ((bus.cpu.cr[2] & spg290::ECR_VECTOR_MASK) >> spg290::ECR_VECTOR_SHIFT, 53u);
}

TEST_F(IrqPathFixture, MaskedSourceDoesNotInterrupt)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    bus.write32(IRQ_MASKL, 0xFFFFFFFFu);
    raiseVBlank();
    bus.cpu.clock();
    EXPECT_EQ(getReg(1), 5u);
    EXPECT_EQ(bus.cpu.pc, 4u);
}

TEST_F(IrqPathFixture, AcknowledgeDropsLine)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    raiseVBlank();
    bus.write32(PPU_STATUS, PPU::IRQ_BLK_ST);       // MMIO write re-evaluates the line
    bus.cpu.clock();
    EXPECT_EQ(getReg(1), 5u);
}

TEST_F(IrqPathFixture, SoftwareInterruptThroughController)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    bus.write32(IRQController::BASE + IRQController::P_I_GPUSWI, 1u);
    bus.cpu.clock();
    EXPECT_EQ(bus.cpu.pc, VEC53_MODE16);
}

TEST_F(IrqPathFixture, HandlerRoundTripThroughRte)
{
    bus.cpu.cr[0] = spg290::PSR_IEC;
    bus.write32(VEC53_MODE16, encodeRTE());
    raiseVBlank();
    bus.cpu.clock();                                // enter
    bus.write32(PPU_STATUS, PPU::IRQ_BLK_ST);       // ack
    bus.cpu.clock();                                // rte
    bus.cpu.clock();                                // addix, exactly once
    EXPECT_EQ(getReg(1), 5u);
    EXPECT_EQ(bus.cpu.pc, 4u);
}
