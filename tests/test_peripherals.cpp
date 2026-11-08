// Timer block, I2C master and controllers, UART capture, CD servo and edge interrupts
// (Phase 5, stories 23 and 24). Expected values come from MAME's spg290_timer,
// spg290_i2c and spg290_cdservo devices and the hyperscan controller.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include "test_helpers.h"
#include "CdServo.h"
#include "DiscImage.h"
#include "I2C.h"
#include "IRQController.h"
#include "System.h"
#include "Timer.h"

// ---- Timer ---------------------------------------------------------------
namespace {
constexpr uint32_t T0 = TimerBlock::BASE;                 // channel 0
constexpr uint32_t CKG0 = TimerBlock::CKG_TIMER_CTRL;     // clock generator word for channel 0
}

TEST(TimerBlock, RegistersReadBackAndEveryWriteClearsTheFlag)
{
	TimerBlock t;
	t.write32(T0 + 0x08, 0xF8F8);
	t.write32(T0 + 0x04, 0);
	t.write32(T0 + 0x0C, 0x55);
	t.write32(T0 + 0x10, 0x66);
	EXPECT_EQ(t.read32(T0 + 0x08), 0xF8F8u);
	EXPECT_EQ(t.read32(T0 + 0x0C), 0x55u);
	EXPECT_EQ(t.read32(T0 + 0x10), 0x66u);

	t.write32(T0 + 0x00, 0x88000000u);                    // enable + interrupt enable
	EXPECT_EQ(t.read32(T0 + 0x00), 0x88000000u);
}

TEST(TimerBlock, NothingCountsUntilTheClockGeneratorEnablesTheChannel)
{
	TimerBlock t;
	t.write32(T0 + 0x08, 0xFFF0);
	t.write32(T0 + 0x00, 0x88000000u);
	t.tick(100000);
	EXPECT_FALSE(t.irq());
	EXPECT_EQ(t.read32(T0 + 0x00) & TimerBlock::CTRL_FLAG, 0u);
}

TEST(TimerBlock, OverflowReloadsFromThePreloadAndRaisesTheFlag)
{
	TimerBlock t;
	t.write32(T0 + 0x08, 0xFFF0);                         // 16 counts to the top
	t.write32(CKG0, 3);                                   // clock on, counter = preload
	t.write32(T0 + 0x00, 0x88000000u);
	// default clock: 27 MHz, so 4 CPU cycles per count. 15 counts reach 0xFFFF.
	t.tick(15 * 4);
	EXPECT_FALSE(t.irq());
	t.tick(4);                                            // the reload step
	EXPECT_TRUE(t.irq());
	EXPECT_NE(t.read32(T0 + 0x00) & TimerBlock::CTRL_FLAG, 0u);

	t.write32(T0 + 0x00, 0x88000000u);                    // any write clears the flag
	EXPECT_FALSE(t.irq());

	// The period is 0x10000 - preload = 16 counts from here on.
	t.tick(15 * 4);
	EXPECT_FALSE(t.irq());
	t.tick(4);
	EXPECT_TRUE(t.irq());
}

TEST(TimerBlock, NoInterruptWithoutTheInterruptEnableBit)
{
	TimerBlock t;
	t.write32(T0 + 0x08, 0xFFF0);
	t.write32(CKG0, 3);
	t.write32(T0 + 0x00, 0x80000000u);                    // enabled, interrupt not enabled
	t.tick(1000);
	EXPECT_FALSE(t.irq());
}

TEST(TimerBlock, ClockSelectionDividesTheCrystalOrPicksTheSlowClock)
{
	TimerBlock t;
	EXPECT_EQ(t.cyclesPerTick(0), 4u);                    // 108 MHz / 27 MHz
	t.write32(TimerBlock::CKG_CLK_SEL, 3);                // 27 MHz / 4
	EXPECT_EQ(t.cyclesPerTick(0), 16u);
	EXPECT_EQ(t.read32(TimerBlock::CKG_CLK_SEL), 3u);
	t.write32(TimerBlock::CKG_CLK_SEL, 3 | 0x100);        // channel 0 on 32.768 kHz
	EXPECT_EQ(t.cyclesPerTick(0), TimerBlock::CPU_HZ / TimerBlock::SLOW_HZ);
	EXPECT_EQ(t.cyclesPerTick(1), 16u);
}

TEST(TimerBlock, ChannelsAreIndependentAndShareOneInterruptLine)
{
	TimerBlock t;
	const uint32_t T2 = TimerBlock::BASE + 2 * TimerBlock::STRIDE;
	t.write32(T2 + 0x08, 0xFFF0);
	t.write32(TimerBlock::CKG_TIMER_CTRL + 8, 3);
	t.write32(T2 + 0x00, 0x88000000u);
	t.tick(16 * 4);
	EXPECT_TRUE(t.irq());
	EXPECT_EQ(t.read32(T0 + 0x00), 0u);                   // channel 0 untouched
	EXPECT_NE(t.read32(T2 + 0x00) & TimerBlock::CTRL_FLAG, 0u);
}

TEST(TimerBlock, TheBusMapsTheTimersAndDrivesInterruptSource7)
{
	Bus b;
	b.irqc.write32(IRQController::P_I_MASKL, 0u);          // everything unmasked
	b.write32(T0 + 0x08, 0xFFF0);
	b.write32(CKG0, 3);
	b.write32(T0 + 0x00, 0x88000000u);
	EXPECT_EQ(b.read32(T0 + 0x08), 0xFFF0u);
	for (int i = 0; i < 20; ++i)
		b.tick(4);
	EXPECT_TRUE(b.irqc.irqActive());
	EXPECT_EQ(b.irqc.activeVector(), 56);                 // MAME: timers on line 56
	b.write32(T0 + 0x00, 0x88000000u);                     // ISR acknowledges
	b.tick(1);
	EXPECT_FALSE(b.irqc.irqActive());
}

// ---- I2C and the controllers ---------------------------------------------
namespace {
constexpr uint32_t I2C = I2CMaster::BASE;
void startRead8(I2CMaster& i, uint32_t portAddr)
{
	i.write32(I2C + 0x24, 0x02);                           // interrupt enable
	i.write32(I2C + 0x30, portAddr);
	i.write32(I2C + 0x20, 0x41);                           // read + start 8
}
}

TEST(I2CMaster, ATransferFinishesAfter38BusClocksAndRaisesTheInterrupt)
{
	I2CMaster i;
	startRead8(i, 0x02);                                   // port 0, index 2 (analog Y)
	i.tick(16 * 38 - 1);                                   // divider 0: 16 CPU cycles per bus clock
	EXPECT_FALSE(i.irq());
	i.tick(1);
	EXPECT_TRUE(i.irq());
	EXPECT_EQ(i.read32(I2C + 0x38), 0x7Fu);                // analog axes centred
	EXPECT_EQ(i.read32(I2C + 0x20) & 0x38u, 0x08u);        // start-8 copied into the ack bits

	i.write32(I2C + 0x24, 0x03);                           // acknowledge, keep enabled
	EXPECT_FALSE(i.irq());
}

TEST(I2CMaster, TheClockDividerStretchesTheTransfer)
{
	I2CMaster i;
	i.write32(I2C + 0x28, 3);                              // divider 3 + 1
	startRead8(i, 0);
	i.tick(16 * 4 * 38 - 1);
	EXPECT_FALSE(i.irq());
	i.tick(1);
	EXPECT_TRUE(i.irq());
}

TEST(I2CMaster, Start16TakesLongerAndNoInterruptWithoutTheEnableBit)
{
	I2CMaster i;
	i.write32(I2C + 0x30, 0);
	i.write32(I2C + 0x20, 0x42);                           // read + start 16, interrupt not enabled
	i.tick(16 * 47);
	EXPECT_FALSE(i.irq());
	EXPECT_EQ(i.read32(I2C + 0x20) & 0x38u, 0x10u);
}

TEST(I2CMaster, ControllerBytesChecksumAndMissingPorts)
{
	I2CMaster i;
	i.setControllerByte(0, 0, 0x02);                       // Start pressed
	i.setControllerByte(1, 1, 0x80);                       // port 1: green button
	startRead8(i, 0x00);                                   // port 0 index 0
	i.tick(16 * 38);
	EXPECT_EQ(i.read32(I2C + 0x38), 0x02u);

	startRead8(i, 0x14);                                   // port 1 index 4: checksum
	i.write32(I2C + 0x24, 0x01);
	i.tick(16 * 38);
	EXPECT_EQ(i.read32(I2C + 0x38), (uint32_t)((((0u + 0x80 + 0x7F + 0x7F)) << 2) | 1u));

	i.write32(I2C + 0x24, 0x01);
	startRead8(i, 0x20);                                   // port 2 does not exist
	i.tick(16 * 38);
	EXPECT_EQ(i.read32(I2C + 0x38), 0xFFFFu);
}

TEST(I2CMaster, TheBusMapsTheWholeBlockAndUsesInterruptSource24)
{
	Bus b;
	b.irqc.write32(IRQController::P_I_MASKL, 0u);
	b.write32(I2C + 0x24, 0x02);
	b.write32(I2C + 0x20, 0x41);
	for (int n = 0; n < 40; ++n)
		b.tick(16);
	EXPECT_TRUE(b.irqc.irqActive());
	EXPECT_EQ(b.irqc.activeVector(), 39);                  // MAME: I2C on line 39
}

// ---- UART ----------------------------------------------------------------
TEST(Uart, WritesToTheTransmitRegisterAreCaptured)
{
	Bus b;
	for (char c : std::string("Hi!"))
		b.write32(Bus::UART_TX, (uint8_t)c);
	EXPECT_EQ(b.uartOutput, "Hi!");
	EXPECT_EQ(b.read32(Bus::UART_TX), 0u);
}

// ---- Edge interrupts -----------------------------------------------------
TEST(EdgeIrq, APulseStaysUntilTheCpuTakesItsVector)
{
	IRQController c;
	c.write32(IRQController::P_I_MASKL, 0u);
	c.pulse(IRQController::SRC_CDSERVO);
	EXPECT_TRUE(c.irqActive());
	EXPECT_EQ(c.activeVector(), 60);
	c.acknowledge(59);                                     // some other vector: no effect
	EXPECT_TRUE(c.irqActive());
	c.acknowledge(60);
	EXPECT_FALSE(c.irqActive());
}

TEST(EdgeIrq, ALevelSourceIsNotClearedByAcknowledge)
{
	IRQController c;
	c.write32(IRQController::P_I_MASKL, 0u);
	c.setSource(IRQController::SRC_TIMER, true);
	c.acknowledge(56);
	EXPECT_TRUE(c.irqActive());
	c.setSource(IRQController::SRC_TIMER, false);
	EXPECT_FALSE(c.irqActive());
}

TEST(EdgeIrq, TheBusClearsAPulsedSourceWhenTheCpuTakesTheInterrupt)
{
	Bus b;
	b.irqc.write32(IRQController::P_I_MASKL, 0u);
	b.irqc.pulse(IRQController::SRC_CDSERVO);
	b.updateIRQ();
	ASSERT_TRUE(b.irqc.irqActive());
	ASSERT_TRUE(static_cast<bool>(b.cpu.onIrqTaken));
	b.cpu.onIrqTaken(60);
	EXPECT_FALSE(b.irqc.irqActive());
}

// ---- Disc image raw sectors ----------------------------------------------
namespace {
// A temporary 2048-byte-sector image of `sectors` sectors; sector n starts with the bytes n, n+1, n+2...
struct TempIso
{
	std::filesystem::path path;
	explicit TempIso(uint32_t sectors)
	{
		path = std::filesystem::temp_directory_path() / "ultrascan_peripherals_test.iso";
		std::ofstream f(path, std::ios::binary);
		std::vector<uint8_t> sec(2048);
		for (uint32_t n = 0; n < sectors; ++n)
		{
			for (size_t i = 0; i < sec.size(); ++i)
				sec[i] = (uint8_t)(n + i);
			f.write((const char*)sec.data(), (std::streamsize)sec.size());
		}
	}
	~TempIso() { std::error_code ec; std::filesystem::remove(path, ec); }
};
}

TEST(DiscImageRaw, ACookedImageGetsSyncHeaderAndModeSynthesized)
{
	TempIso iso(300);
	DiscImage d;
	ASSERT_TRUE(d.open(iso.path.string()));
	EXPECT_EQ(d.sectorCount(), 300u);
	uint8_t raw[DiscImage::RAW_SECTOR];
	ASSERT_TRUE(d.readRawSector(100, raw));
	EXPECT_EQ(raw[0], 0x00);
	EXPECT_EQ(raw[1], 0xFF);
	EXPECT_EQ(raw[10], 0xFF);
	EXPECT_EQ(raw[11], 0x00);
	EXPECT_EQ(raw[12], 0x00);                             // minute
	EXPECT_EQ(raw[13], 0x03);                             // (100 + 150) / 75 = 3 s
	EXPECT_EQ(raw[14], 0x25);                             // frame 25 in BCD
	EXPECT_EQ(raw[15], 1);
	EXPECT_EQ(raw[16], 100);                              // user data
	EXPECT_EQ(raw[17], 101);
	EXPECT_EQ(raw[16 + 2047], (uint8_t)(100 + 2047));
	EXPECT_EQ(raw[16 + 2048], 0);
	EXPECT_FALSE(d.readRawSector(300, raw));
}

// ---- CD servo ------------------------------------------------------------
namespace {
constexpr uint32_t CD = CdServo::BASE;

struct CdRig
{
	TempIso iso{ 300 };
	DiscImage disc;
	CdServo cd;
	std::vector<uint8_t> mem = std::vector<uint8_t>(0x10000, 0xEE);

	CdRig()
	{
		EXPECT_TRUE(disc.open(iso.path.string()));
		cd.setMemoryWriter([this](uint32_t a, uint8_t v) { mem[a & 0xFFFF] = v; });
		cd.attachDisc(&disc);
	}
	void dsp(uint32_t addr, uint32_t data, bool read = false)
	{
		cd.write32(CD + 0x04, addr);
		cd.write32(CD + 0x08, data);
		cd.write32(CD + 0x0C, read ? 1 : 0);
	}
	uint32_t dspRead(uint32_t addr) { dsp(addr, 0, true); return cd.read32(CD + 0x08); }
	void ringAndSeek(uint32_t start, uint32_t end, uint32_t size, uint32_t m, uint32_t s, uint32_t f)
	{
		cd.write32(CD + 0x60, start);
		cd.write32(CD + 0x64, end);
		cd.write32(CD + 0x68, start);
		cd.write32(CD + 0x6C, size);
		cd.write32(CD + 0x48, m);
		cd.write32(CD + 0x4C, s);
		cd.write32(CD + 0x50, f);
		cd.write32(CD + 0x44, 0);                          // seek
	}
};
constexpr uint32_t kSectorCycles = CdServo::CPU_HZ / 75;   // one sector at 1x
}

TEST(CdServo, DspIdentifiesItselfAndTheDisc)
{
	CdRig r;
	r.dsp(0x032, 0);                                       // DSP version request
	EXPECT_EQ(r.dspRead(0x07C), 0x01u);
	EXPECT_EQ(r.dspRead(0x07D) & 0xFFu, 0x02u);            // MAME returns the whole 0x0102 here; the BIOS masks
	r.dsp(0x030, 0);                                       // disc id request
	EXPECT_EQ(r.dspRead(0x07D), 100u);                     // a CD-ROM (>= 97)
	EXPECT_EQ(r.dspRead(0x079), 0xE3u);                    // servo status
	EXPECT_EQ(r.dspRead(0x506), 0x04u);                    // DSP ready
	EXPECT_EQ(r.dspRead(0x41F), 0xFFu);

	CdServo empty;
	empty.write32(CD + 0x04, 0x030);
	empty.write32(CD + 0x08, 0);
	empty.write32(CD + 0x0C, 0);
	empty.write32(CD + 0x04, 0x07D);
	empty.write32(CD + 0x0C, 1);
	EXPECT_EQ(empty.read32(CD + 0x08), 0u);                // no disc
}

TEST(CdServo, TheDspMemoryStoresThreeByteChunks)
{
	CdRig r;
	r.dsp(0x500, 0x12);   // address high
	r.dsp(0x501, 0x34);   // address low
	r.dsp(0x502, 0xAA);
	r.dsp(0x503, 0xBB);
	r.dsp(0x504, 0xCC);
	r.dsp(0x505, 0x02);                                    // write
	r.dsp(0x502, 0);
	r.dsp(0x503, 0);
	r.dsp(0x504, 0);
	r.dsp(0x505, 0x03);                                    // read back into regs 7..9
	EXPECT_EQ(r.dspRead(0x507), 0xAAu);
	EXPECT_EQ(r.dspRead(0x508), 0xBBu);
	EXPECT_EQ(r.dspRead(0x509), 0xCCu);
}

TEST(CdServo, ASeekThenOneSectorPerPeriodFillsTheRingWithPaddedSectors)
{
	CdRig r;
	r.ringAndSeek(0x1000, 0x1FFF, 2368, 0x00, 0x02, 0x00);   // 00:02:00 = LBA 0 on the disc
	EXPECT_EQ(r.cd.seekLba(), 150u);

	r.cd.tick(kSectorCycles - 1);
	EXPECT_FALSE(r.cd.takeIrq());
	r.cd.tick(1);
	EXPECT_TRUE(r.cd.takeIrq());
	EXPECT_FALSE(r.cd.takeIrq());                          // an edge, once

	// Raw sector 0 at 0x1000: sync, MSF 00:02:00, mode 1, user data, zero padding to 2368.
	EXPECT_EQ(r.mem[0x1000], 0x00);
	EXPECT_EQ(r.mem[0x1001], 0xFF);
	EXPECT_EQ(r.mem[0x100C], 0x00);
	EXPECT_EQ(r.mem[0x100D], 0x02);
	EXPECT_EQ(r.mem[0x100E], 0x00);
	EXPECT_EQ(r.mem[0x100F], 0x01);
	EXPECT_EQ(r.mem[0x1010], 0x00);                        // sector 0, byte 0 of user data
	EXPECT_EQ(r.mem[0x1011], 0x01);
	EXPECT_EQ(r.mem[0x1000 + 2352], 0x00);                 // padding
	EXPECT_EQ(r.mem[0x1000 + 2367], 0x00);
	EXPECT_EQ(r.mem[0x1000 + 2368], 0xEE);                 // nothing beyond one sector yet
	EXPECT_EQ(r.cd.bufferPointer(), 0x1000u + 2368u);
	EXPECT_EQ(r.cd.seekLba(), 151u);
	EXPECT_EQ(r.dspRead(0x307), 1u);                       // frame found

	r.dsp(0x307, 0);                                       // clear it
	EXPECT_EQ(r.dspRead(0x307), 0u);

	r.cd.tick(kSectorCycles);
	EXPECT_TRUE(r.cd.takeIrq());
	EXPECT_EQ(r.mem[0x1000 + 2368 + 14], 0x01);            // second sector: frame 01
	EXPECT_EQ(r.mem[0x1000 + 2368 + 16], 0x01);            // user data starts with n = 1
}

TEST(CdServo, TheRingWrapsByteByByteAtTheEndAddress)
{
	CdRig r;
	r.ringAndSeek(0x1000, 0x17FF, 2368, 0x00, 0x02, 0x00); // 2048-byte ring, smaller than a sector
	r.cd.tick(kSectorCycles);
	EXPECT_EQ(r.cd.bufferPointer(), 0x1000u + (2368u % 2048u));
	// Bytes 2048..2367 of the sector wrapped to the start of the ring.
	EXPECT_EQ(r.mem[0x1000], 0xF0);                        // sector byte 2048 = user byte 2032 = (0 + 2032) & 0xFF
}

TEST(CdServo, PauseBitStopsDeliveryAndSpeedSelectChangesThePeriod)
{
	CdRig r;
	r.ringAndSeek(0x1000, 0x1FFF, 2368, 0x00, 0x02, 0x00);
	r.cd.write32(CD + 0x44, 0x04);                         // pause (also re-seeks)
	r.cd.tick(kSectorCycles * 3);
	EXPECT_FALSE(r.cd.takeIrq());

	r.cd.write32(CD + 0x44, 0);
	r.dsp(0x020, 1);                                       // speed = 1 << 1 = 2x
	r.cd.tick(kSectorCycles / 2);
	EXPECT_TRUE(r.cd.takeIrq());                           // a sector every half period
}

TEST(CdServo, QSubchannelTableHasTheLeadInTocAndTheProgramArea)
{
	CdRig r;
	// 7500 lead-in sectors, then the 150-frame pregap, the 300 data sectors, 6750 lead-out.
	EXPECT_EQ(r.cd.totalSectors(), 7500u + 150u + 300u + 6750u);
	EXPECT_EQ(r.cd.qsubByte(0, 0), 0x41);                  // adr/control 0x14 with the nibbles swapped
	EXPECT_EQ(r.cd.qsubByte(0, 2), 0xA0);                  // first track number entry
	EXPECT_EQ(r.cd.qsubByte(1, 2), 0xA1);                  // last track
	EXPECT_EQ(r.cd.qsubByte(2, 2), 0xA2);                  // start of the lead-out
	EXPECT_EQ(r.cd.qsubByte(3, 2), 0x01);                  // track 1 entry (BCD)
	// First program sector: track 1, index 1, absolute 00:02:00.
	EXPECT_EQ(r.cd.qsubByte(7500 + 150, 1), 0x01);
	EXPECT_EQ(r.cd.qsubByte(7500 + 150, 2), 0x01);
	EXPECT_EQ(r.cd.qsubByte(7500 + 150, 8), 0x02);
	EXPECT_EQ(r.cd.qsubByte(7500 + 150 + 300, 1), 0xAA);   // lead-out track number
}

TEST(CdServo, TheBusDeliversSectorsIntoRamAndRaisesInterruptSource3)
{
	TempIso iso(300);
	DiscImage disc;
	ASSERT_TRUE(disc.open(iso.path.string()));
	Bus b;
	b.cdservo.attachDisc(&disc);
	b.irqc.write32(IRQController::P_I_MASKL, 0u);
	b.write32(CD + 0x60, 0x2000);
	b.write32(CD + 0x64, 0x2FFF);
	b.write32(CD + 0x68, 0x2000);
	b.write32(CD + 0x6C, 2368);
	b.write32(CD + 0x4C, 0x02);
	b.write32(CD + 0x44, 0);
	b.tick(kSectorCycles);
	EXPECT_EQ(b.read8(0x2001), 0xFF);                      // sync bytes landed in RAM
	EXPECT_TRUE(b.irqc.irqActive());
	EXPECT_EQ(b.irqc.activeVector(), 60);                  // MAME: CD servo on line 60
	b.cpu.onIrqTaken(60);
	EXPECT_FALSE(b.irqc.irqActive());
}

// ---- Scripted controller input (story 30) -----------------------------------
TEST(ScriptedInput, ButtonHeldForTheRequestedFrames)
{
	System sys(1u << 20);
	std::string err;
	ASSERT_TRUE(sys.addPress("start@3+2", &err)) << err;
	ASSERT_TRUE(sys.addPress("red:1@4", &err)) << err;
	sys.applyInput(2);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 0), 0x00);
	sys.applyInput(3);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 0), 0x02);   // start
	sys.applyInput(4);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 0), 0x02);
	EXPECT_EQ(sys.bus.i2c.controllerByte(1, 1), 0x40);   // red on port 1 only
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 1), 0x00);
	sys.applyInput(5);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 0), 0x00);   // start released after 2 frames
	EXPECT_EQ(sys.bus.i2c.controllerByte(1, 1), 0x40);
	sys.applyInput(10);
	EXPECT_EQ(sys.bus.i2c.controllerByte(1, 1), 0x00);   // default 6 frames: 4..9
}

TEST(ScriptedInput, AxesMoveAndRecentre)
{
	System sys(1u << 20);
	ASSERT_TRUE(sys.addPress("up@1+1"));
	ASSERT_TRUE(sys.addPress("right@1+1"));
	sys.applyInput(1);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 2), 0x00);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 3), 0xFF);
	sys.applyInput(2);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 2), 0x7F);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 3), 0x7F);
}

TEST(ScriptedInput, BadSpecsAreRejected)
{
	System sys(1u << 20);
	std::string err;
	EXPECT_FALSE(sys.addPress("start", &err));
	EXPECT_FALSE(sys.addPress("bogus@10", &err));
	EXPECT_FALSE(sys.addPress("start:2@10", &err));
	EXPECT_FALSE(sys.addPress("start@x", &err));
	EXPECT_FALSE(sys.addPress("start@10+0", &err));
	EXPECT_FALSE(sys.addPress("start@10+3z", &err));
	EXPECT_EQ(sys.pressCount(), 0u);
}

TEST(ScriptedInput, NoPressesLeavesManualBytesAlone)
{
	System sys(1u << 20);
	sys.bus.i2c.setControllerByte(0, 0, 0x55);
	sys.applyInput(7);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 0), 0x55);
}

// ---- Live display support (story 39), headless parts ---------------------------
#include "FrameWriter.h"

namespace {
// Reassemble the pixel data of a PNG made of stored deflate blocks, checking every CRC.
bool decodeStoredPng(const std::vector<uint8_t>& png, int& w, int& h, std::vector<uint8_t>& scan)
{
	if (png.size() < 8 || png[1] != 'P' || png[2] != 'N' || png[3] != 'G') return false;
	size_t pos = 8;
	std::vector<uint8_t> z;
	bool sawEnd = false;
	while (pos + 12 <= png.size())
	{
		const uint32_t len = (uint32_t)png[pos] << 24 | (uint32_t)png[pos + 1] << 16 | (uint32_t)png[pos + 2] << 8 | png[pos + 3];
		const std::string type(png.begin() + (std::ptrdiff_t)pos + 4, png.begin() + (std::ptrdiff_t)pos + 8);
		if (pos + 12 + len > png.size()) return false;
		const uint32_t crc = frame_io::detail::crc32(&png[pos + 4], 4 + len);
		const size_t cp = pos + 8 + len;
		const uint32_t want = (uint32_t)png[cp] << 24 | (uint32_t)png[cp + 1] << 16 | (uint32_t)png[cp + 2] << 8 | png[cp + 3];
		if (crc != want) return false;
		if (type == "IHDR")
		{
			w = (int)((uint32_t)png[pos + 8] << 24 | (uint32_t)png[pos + 9] << 16 | (uint32_t)png[pos + 10] << 8 | png[pos + 11]);
			h = (int)((uint32_t)png[pos + 12] << 24 | (uint32_t)png[pos + 13] << 16 | (uint32_t)png[pos + 14] << 8 | png[pos + 15]);
		}
		else if (type == "IDAT")
			z.insert(z.end(), png.begin() + (std::ptrdiff_t)pos + 8, png.begin() + (std::ptrdiff_t)cp);
		else if (type == "IEND")
			sawEnd = true;
		pos = cp + 4;
	}
	if (!sawEnd || z.size() < 6) return false;
	size_t i = 2;                                   // skip the zlib header
	for (;;)
	{
		if (i + 5 > z.size()) return false;
		const bool last = z[i] & 1;
		const uint32_t n = z[i + 1] | (uint32_t)z[i + 2] << 8;
		if ((uint16_t)(z[i + 3] | z[i + 4] << 8) != (uint16_t)~n) return false;
		scan.insert(scan.end(), z.begin() + (std::ptrdiff_t)i + 5, z.begin() + (std::ptrdiff_t)(i + 5 + n));
		i += 5 + n;
		if (last) break;
	}
	return true;
}
} // namespace

TEST(FramePng, RoundTripsPixelsAcrossStoredBlocks)
{
	const int w = 300, h = 120;                     // 108,120 raw bytes: needs two stored blocks
	std::vector<PPU::Color> fb((size_t)w * h);
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x)
			fb[(size_t)y * w + x] = PPU::Color{ (uint8_t)x, (uint8_t)y, (uint8_t)(x ^ y) };
	const std::vector<uint8_t> png = frame_io::encodePNG(fb, w, h);
	int rw = 0, rh = 0;
	std::vector<uint8_t> scan;
	ASSERT_TRUE(decodeStoredPng(png, rw, rh, scan));
	EXPECT_EQ(rw, w);
	EXPECT_EQ(rh, h);
	ASSERT_EQ(scan.size(), (size_t)h * (w * 3 + 1));
	for (int y = 0; y < h; ++y)
	{
		ASSERT_EQ(scan[(size_t)y * (w * 3 + 1)], 0);   // filter type 0
		for (int x = 0; x < w; ++x)
		{
			const uint8_t* px = &scan[(size_t)y * (w * 3 + 1) + 1 + (size_t)x * 3];
			ASSERT_EQ(px[0], fb[(size_t)y * w + x].r);
			ASSERT_EQ(px[1], fb[(size_t)y * w + x].g);
			ASSERT_EQ(px[2], fb[(size_t)y * w + x].b);
		}
	}
}

TEST(FramePng, KnownCrcAndBadSizes)
{
	const uint8_t s[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
	EXPECT_EQ(frame_io::detail::crc32(s, sizeof(s)), 0xCBF43926u);   // the standard CRC-32 check value
	EXPECT_TRUE(frame_io::encodePNG({}, 4, 4).empty());
	EXPECT_TRUE(frame_io::encodePNG(std::vector<PPU::Color>(4), 0, 4).empty());
}

TEST(LiveInput, ButtonsAndAxesHoldUntilReleased)
{
	System sys(1u << 20);
	EXPECT_TRUE(sys.setLiveButton(0, "start", true));
	EXPECT_TRUE(sys.setLiveButton(0, "blue", true));
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 0), 0x03);
	EXPECT_TRUE(sys.setLiveButton(0, "start", false));
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 0), 0x01);
	EXPECT_TRUE(sys.setLiveButton(1, "green", true));
	EXPECT_EQ(sys.bus.i2c.controllerByte(1, 1), 0x80);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 1), 0x00);
	EXPECT_TRUE(sys.setLiveButton(0, "left", true));
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 3), 0x00);
	EXPECT_TRUE(sys.setLiveButton(0, "left", false));
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 3), 0x7F);
	EXPECT_FALSE(sys.setLiveButton(0, "bogus", true));
	EXPECT_FALSE(sys.setLiveButton(2, "start", true));
}

TEST(LiveInput, CombinesWithScriptedPresses)
{
	System sys(1u << 20);
	ASSERT_TRUE(sys.addPress("select@2+1"));
	sys.setLiveButton(0, "start", true);
	sys.applyInput(2);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 0), 0x06);   // start (live) + select (scripted)
	sys.applyInput(3);
	EXPECT_EQ(sys.bus.i2c.controllerByte(0, 0), 0x02);
}

TEST(Viewer, RequestStopEndsTheRunAfterTheFrame)
{
	System sys(1u << 20);
	// A tight loop at the reset PC: "j ." is not needed, the default run just executes
	// whatever is in RAM (zeros = nop); we only need frames to elapse.
	sys.reset(0);
	uint64_t seen = 0;
	const System::RunStats s = sys.run(0, 50000000, [&](const std::vector<PPU::Color>&, int, int, uint64_t n) {
		seen = n;
		if (n == 2) sys.requestStop();
	});
	EXPECT_EQ(s.reason, System::StopReason::UserStop);
	EXPECT_EQ(seen, 2u);
	EXPECT_EQ(s.frames, 3u);
}

TEST(Viewer, DebugHideRemovesTextLayersAndSprites)
{
	FlatMemory mem(0, 0x40000);
	PPU ppu;
	ppu.write32(PPU::P_PPU_Control, PPU::PPUEN_BIT | PPU::RES_QVGA);
	// text layer 0 in bitmap RGB565 mode, every pixel pure red
	ppu.write32(PPU::TEXT_BASE + PPU::TX_CTRL, PPU::TXC_TXE | PPU::TXC_LINR | PPU::TXC_RGB565);
	ppu.write32(PPU::BUF_SA_BASE, 0x1000);
	for (uint32_t i = 0; i < 512 * 256; ++i)
		mem.writeHalf(0x1000 + i * 2, 0xF800);
	EXPECT_EQ(ppu.samplePixel(mem, 5, 5, PPU::Color{ 1, 2, 3 }), (PPU::Color{ 255, 0, 0 }));
	ppu.debugHide = 1;
	EXPECT_EQ(ppu.samplePixel(mem, 5, 5, PPU::Color{ 1, 2, 3 }), (PPU::Color{ 1, 2, 3 }));
	ppu.debugHide = 0;
	EXPECT_EQ(ppu.samplePixel(mem, 5, 5, PPU::Color{ 1, 2, 3 }), (PPU::Color{ 255, 0, 0 }));
}

// ---- Lazy device time must behave exactly like per-instruction ticking -----------
namespace {
// A timer that wraps every 64 CPU cycles, acknowledged by a control write after each
// interrupt like an ISR would. Returns the step at which each interrupt became active.
std::vector<int> timerIrqTimeline(bool lazy, int steps)
{
	Bus bus(1u << 20);
	bus.write32(0x880A0020u, 0);             // unmask every source
	bus.write32(0x08160008u, 0xFFF0u);       // preload: 16 clocks per wrap
	bus.write32(0x082100E4u, 0);             // divider 1: 4 CPU cycles per timer clock
	bus.write32(0x0821006Cu, 3);             // clock on, load the counter from the preload
	bus.write32(0x08160000u, 0x88000000u);   // enable + interrupt enable
	std::vector<int> events;
	for (int step = 0; step < steps; ++step)
	{
		if (lazy) bus.advance(6); else bus.tick(6);
		if (bus.irqc.activeSource() == (int)IRQController::SRC_TIMER)
		{
			events.push_back(step);
			bus.write32(0x08160000u, 0x88000000u);   // acknowledge (clears the flag)
		}
	}
	return events;
}
} // namespace

TEST(LazyTime, TimerInterruptsMatchEagerTickingAcrossAcknowledges)
{
	const std::vector<int> eager = timerIrqTimeline(false, 4000);
	const std::vector<int> lazy  = timerIrqTimeline(true, 4000);
	ASSERT_GT(eager.size(), 100u);          // many wrap/acknowledge rounds, not just the first
	EXPECT_EQ(eager, lazy);
}

TEST(LazyTime, SyncBringsDevicesUpToDateAndRegisterReadsDoThemselves)
{
	Bus bus(1u << 20);
	bus.write32(0x08160008u, 0xFFF0u);
	bus.write32(0x082100E4u, 0);
	bus.write32(0x0821006Cu, 3);
	bus.write32(0x08160000u, 0x88000000u);
	for (int i = 0; i < 5; ++i)
		bus.advance(6);                         // 30 cycles: no wrap yet (64 needed)
	EXPECT_FALSE(bus.timers.irq());
	for (int i = 0; i < 10; ++i)
		bus.advance(6);                         // 90 cycles in all: the wrap has to have happened
	// advance() synced at the deadline, so the flag is already visible...
	EXPECT_TRUE(bus.timers.irq());
	// ...and a register read reports it too.
	EXPECT_NE(bus.read32(0x08160000u) & (1u << 26), 0u);
}
