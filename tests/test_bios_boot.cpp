// Booting from a BIOS image (Phase 5, story 22).
#include <vector>

#include "test_helpers.h"
#include "System.h"

namespace {
// A little-endian image of `size` bytes made of the words w0 (at 0) and w1 (at 4), the rest 0xAB.
std::vector<uint8_t> makeImage(size_t size, uint32_t w0, uint32_t w1 = 0)
{
	std::vector<uint8_t> img(size, 0xAB);
	for (int i = 0; i < 4; ++i)
	{
		img[i]     = (uint8_t)(w0 >> (8 * i));
		img[4 + i] = (uint8_t)(w1 >> (8 * i));
	}
	return img;
}
} // namespace

TEST(BiosBoot, MapsTheImageAtTheRomWindowAndStartsAtItsFirstWord)
{
	System s(1u << 20);
	const std::vector<uint8_t> img = makeImage(4096, encodeJX(0, false), 0x11223344u);
	ASSERT_TRUE(s.bootBios(img.data(), img.size()));

	EXPECT_EQ(s.bus.cpu.pc, System::BIOS_RESET_PC);
	EXPECT_EQ(System::BIOS_RESET_PC, 0x9F000000u);                 // MAME: PC = EXCPVEC = 0x9F000000
	EXPECT_EQ(s.bus.cpu.cr[3], 0x9F000000u);
	EXPECT_EQ(s.bus.read32(0x10000004u), 0x11223344u);        // little-endian bus
	EXPECT_EQ(s.bus.read8(0x10000000u + 5), 0x33u);
}

TEST(BiosBoot, TheImageRepeatsThroughTheRomRegion)
{
	System s(1u << 20);
	const std::vector<uint8_t> img = makeImage(4096, 0x01020304u, 0x11223344u);
	ASSERT_TRUE(s.bootBios(img.data(), img.size()));

	// The real BIOS reads its own header through 0x1F000040 and copies its code
	// from 0x1E001000, so the 1 MiB image repeats; here the image is 4 KiB.
	EXPECT_EQ(s.bus.read32(0x10000000u + 4096u * 3 + 4), 0x11223344u);
	EXPECT_EQ(s.bus.read32(0x1F000004u), 0x11223344u);        // 0x1F000000 is a multiple of the image size
	EXPECT_EQ(s.bus.read32(0x1FFFF000u + 4), 0x11223344u);    // last page of the region, offset 4
	EXPECT_EQ(s.bus.read8(0x1E000000u + 100), 0xABu);         // plain image byte
}

TEST(BiosBoot, RomStaysReadOnly)
{
	System s(1u << 20);
	const std::vector<uint8_t> img = makeImage(4096, 0x01020304u);
	ASSERT_TRUE(s.bootBios(img.data(), img.size()));
	s.bus.write32(0x10000000u, 0xDEADBEEFu);
	EXPECT_EQ(s.bus.read32(0x10000000u), 0x01020304u);
}

TEST(BiosBoot, ARunWithoutABiosDoesNotMirrorTheRomWindow)
{
	Bus b;
	const uint8_t data[4] = { 1, 2, 3, 4 };
	ASSERT_TRUE(b.loadImage(data, 4, Bus::ROM_PHYS));
	EXPECT_EQ(b.romMirror, 0u);
	EXPECT_EQ(b.read8(Bus::ROM_PHYS + 4), 0u);                // beyond the image: 0, as before
	EXPECT_EQ(b.read8(0x1E000000u), 0u);                      // outside the 16 MiB window: unmapped
}

TEST(BiosBoot, ResetPcOverrideAndTheMachineIsReset)
{
	System s(1u << 20);
	s.bus.cpu.regs[5] = 99;
	const std::vector<uint8_t> img = makeImage(4096, encodeJX(0, false));
	ASSERT_TRUE(s.bootBios(img.data(), img.size(), 0x10000100u));
	EXPECT_EQ(s.bus.cpu.pc, 0x10000100u);
	EXPECT_EQ(s.bus.cpu.regs[5], 0u);
}

TEST(BiosBoot, AnImageThatDoesNotFitLeavesTheMachineAlone)
{
	System s(1u << 20);
	s.bus.cpu.regs[5] = 99;
	s.bus.cpu.pc = 0x44;
	const std::vector<uint8_t> img(Bus::ROM_WINDOW + 4, 1);
	EXPECT_FALSE(s.bootBios(img.data(), img.size()));
	EXPECT_EQ(s.bus.cpu.regs[5], 99u);
	EXPECT_EQ(s.bus.cpu.pc, 0x44u);
	EXPECT_EQ(s.bus.romMirror, 0u);
}

TEST(BiosBoot, BootingSetsDregLikeMamesDriverDoes)
{
	System s(1u << 20);
	const std::vector<uint8_t> img = makeImage(4096, encodeJX(0, false));
	ASSERT_TRUE(s.bootBios(img.data(), img.size()));
	// MAME's driver sets cr29 to 0x20000000 ("disable JTAG"); the BIOS spins forever
	// at 0x100001B0 without it.
	EXPECT_EQ(s.bus.cpu.cr[29], System::DREG_JTAG_DISABLED);

	spg290 cpu;                                              // a bare CPU reset leaves it alone
	cpu.reset(0);
	EXPECT_EQ(cpu.cr[29], 0u);
}

TEST(BiosBoot, ABiosThatSpinsStopsAsAHang)
{
	System s(1u << 20);
	s.bus.cpu.setStopOnHang(true);
	s.bus.cpu.setHangThreshold(8);
	const std::vector<uint8_t> img = makeImage(4096, 0x9000BC00u);   // b . (little-endian image bytes)
	ASSERT_TRUE(s.bootBios(img.data(), img.size()));
	const System::RunStats st = s.run(5, 0);
	EXPECT_EQ(st.reason, System::StopReason::Hang);
}
