// Bus memory map (plan story 9): mirrored RAM, SRAM, MIU status, the dummy
// window and the unmapped-access log. Addresses follow MAME's SPG29x map.
#include <gtest/gtest.h>
#include "Bus.h"

TEST(BusMap, SystemRamMirrorsThroughTheLowRegion)
{
    Bus b(Bus::SYSTEM_RAM_SIZE);
    b.write32(0x07FFFFFC, 0xCAFEF00Du);
    EXPECT_EQ(b.read32(0x00FFFFFC), 0xCAFEF00Du);        // 16 MiB mirror
    EXPECT_EQ(b.read32(0xA7FFFFFC), 0xCAFEF00Du);        // uncached alias of the same word
    EXPECT_EQ(b.read32(0x01FFFFFC), 0xCAFEF00Du);
    EXPECT_TRUE(b.topUnmappedPages().empty());
}

TEST(BusMap, ReadsBelowTheRamLimitNeverCountAsUnmapped)
{
    Bus b;   // 256 KiB
    b.read32(0x07FFFFFC);
    b.write8(0x01234567, 1);
    EXPECT_TRUE(b.topUnmappedPages().empty());
}

TEST(BusMap, SramRoundTripsAt0A000000)
{
    Bus b;
    b.write32(0x0A000000, 0x11223344u);
    b.write8(0x0A003FFF, 0x5A);
    EXPECT_EQ(b.read32(0x0A000000), 0x11223344u);
    EXPECT_EQ(b.read8(0x0A003FFF), 0x5Au);
    EXPECT_EQ(b.read32(0xAA000000), 0x11223344u);        // alias
    EXPECT_EQ(b.read8(0x00000000), 0u);                  // not RAM
    b.read32(0x0A004000);                                // first byte past SRAM
    EXPECT_EQ(b.topUnmappedPages().size(), 1u);
}

TEST(BusMap, MiuStatusReadsOneAndIgnoresWrites)
{
    Bus b;
    EXPECT_EQ(b.read32(0x0807006C), 1u);
    b.write32(0x0807006C, 0u);
    EXPECT_EQ(b.read32(0x0807006C), 1u);
    EXPECT_EQ(b.read32(0x8807006C), 1u);                 // alias
    EXPECT_TRUE(b.topUnmappedPages().empty());
}

TEST(BusMap, DummyWindowReadsZeroAndIgnoresWrites)
{
    Bus b;
    b.write32(0x08240000, 0xFFFFFFFFu);
    b.write8(0x0824000F, 0xFF);
    EXPECT_EQ(b.read32(0x08240000), 0u);
    EXPECT_EQ(b.read32(0x0824000C), 0u);
    EXPECT_TRUE(b.topUnmappedPages().empty());
    b.read32(0x08240010);                                // just past the window
    EXPECT_EQ(b.topUnmappedPages().size(), 1u);
}

TEST(BusMap, RomWindowMovedTo10000000)
{
    Bus b;
    const uint8_t d[] = { 1, 2, 3, 4 };
    ASSERT_TRUE(b.loadImage(d, sizeof(d), 0x10000000u));
    EXPECT_EQ(b.read32(0x90000000u), 0x01020304u);
    EXPECT_EQ(b.read32(0xB0000000u), 0x01020304u);
    EXPECT_TRUE(b.topUnmappedPages().empty());
    b.read32(0x1F000000u);                               // the old window is unmapped now
    EXPECT_EQ(b.topUnmappedPages().size(), 1u);
}

TEST(BusMap, UnmappedLogCountsReadsAndWritesPerPage)
{
    Bus b;
    b.read32(0x08210030);
    b.read8(0x08210FFF);
    b.write32(0x08210000, 1);
    b.read32(0x08030050);

    auto top = b.topUnmappedPages(10);
    ASSERT_EQ(top.size(), 2u);
    EXPECT_EQ(top[0].page, 0x08210000u);
    EXPECT_EQ(top[0].reads, 2u);
    EXPECT_EQ(top[0].writes, 1u);
    EXPECT_EQ(top[1].page, 0x08030000u);
    EXPECT_EQ(top[1].reads, 1u);

    EXPECT_EQ(b.topUnmappedPages(1).size(), 1u);
    const std::string s = b.unmappedSummary(10);
    EXPECT_NE(s.find("0x08210000"), std::string::npos);
    EXPECT_NE(s.find("reads 2  writes 1"), std::string::npos);

    b.clearUnmappedLog();
    EXPECT_TRUE(b.topUnmappedPages().empty());
}

TEST(BusMap, UnmappedReadsReturnZeroAndWritesAreDropped)
{
    Bus b;
    b.write32(0x08210030, 0xFFFFFFFFu);
    EXPECT_EQ(b.read32(0x08210030), 0u);
    EXPECT_EQ(b.read16(0x08210032), 0u);
}
