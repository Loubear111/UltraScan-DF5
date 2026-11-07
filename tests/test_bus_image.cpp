#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Bus.h"
#include "test_helpers.h"

// ============================================================
// Phase 4.2: configurable RAM, ROM window, image loading and the
// byte-order flag.
// ============================================================

namespace
{

// Removes the file on destruction.
struct TempFile
{
    std::filesystem::path path;
    explicit TempFile(const std::string& name)
        : path(std::filesystem::temp_directory_path() / name) {}
    ~TempFile() { std::error_code ec; std::filesystem::remove(path, ec); }
    void write(const std::vector<uint8_t>& bytes) const
    {
        std::ofstream f(path, std::ios::binary);
        f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
    }
};

} // namespace

// ---- RAM -----------------------------------------------------------------
TEST(BusImage, DefaultRamSizeIsUnchanged)
{
    Bus b;
    EXPECT_EQ(b.ram.size(), 256u * 1024u);
    b.write32(0x3FFFC, 0x11223344u);
    EXPECT_EQ(b.read32(0x3FFFC), 0x11223344u);
    b.write32(0x40000, 0x55667788u);                 // one past the end: mirrors to address 0
    EXPECT_EQ(b.read32(0x40000), 0x55667788u);
    EXPECT_EQ(b.read32(0), 0x55667788u);
}

TEST(BusImage, ConfigurableRamRoundTripAndMirror)
{
    Bus b(1024u * 1024u);
    b.write32(0xFFFFC, 0xDEADBEEFu);
    EXPECT_EQ(b.read32(0xFFFFC), 0xDEADBEEFu);

    b.write32(0x100000, 0x12345678u);                // just above RAM: mirrors to 0
    EXPECT_EQ(b.read32(0), 0x12345678u);
    b.write8(0x100001, 0x99);
    EXPECT_EQ(b.read8(1), 0x99u);
    EXPECT_EQ(b.read32(0xFFFFC), 0xDEADBEEFu);       // neighbour untouched
}

TEST(BusImage, AccessStraddlingRamEndWrapsToTheStart)
{
    Bus b(1024u);
    b.write32(1022, 0xAABBCCDDu);                    // bytes 1022, 1023, then 0, 1
    EXPECT_EQ(b.read8(1022), 0xAAu);
    EXPECT_EQ(b.read8(1023), 0xBBu);
    EXPECT_EQ(b.read8(0), 0xCCu);
    EXPECT_EQ(b.read8(1), 0xDDu);
    EXPECT_EQ(b.read32(1022), 0xAABBCCDDu);
}

// ---- loadImage -----------------------------------------------------------
TEST(BusImage, LoadImageIntoRamKeepsFileOrder)
{
    Bus b;
    const uint8_t data[] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
    ASSERT_TRUE(b.loadImage(data, sizeof(data), 0x100));
    for (size_t i = 0; i < sizeof(data); ++i)
        EXPECT_EQ(b.read8(0x100 + (uint32_t)i), data[i]);
}

TEST(BusImage, LoadImageAliasedAddressReachesRam)
{
    Bus b;
    const uint8_t data[] = { 1, 2, 3, 4 };
    ASSERT_TRUE(b.loadImage(data, sizeof(data), 0xA0000200u));
    EXPECT_EQ(b.read8(0x200), 1u);
    EXPECT_EQ(b.read8(0x203), 4u);
}

TEST(BusImage, LoadImageOverrunFailsAndChangesNothing)
{
    Bus b(1024u);
    b.write8(1020, 0x77);
    const uint8_t data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    EXPECT_FALSE(b.loadImage(data, sizeof(data), 1020));
    EXPECT_EQ(b.read8(1020), 0x77u);
    EXPECT_EQ(b.read8(1023), 0u);
}

TEST(BusImage, LoadImageOutsideAnyRegionFails)
{
    Bus b;
    const uint8_t data[] = { 1, 2, 3, 4 };
    EXPECT_FALSE(b.loadImage(data, sizeof(data), 0x0C000000u));
    EXPECT_FALSE(b.loadImage(nullptr, 4, 0));
    EXPECT_TRUE(b.loadImage(data, 0, 0));            // empty image is a no-op
}

// ---- ROM window ----------------------------------------------------------
TEST(BusImage, RomWindowIsReadableAtBothAliasesAndReadOnly)
{
    Bus b;
    const uint8_t data[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    ASSERT_TRUE(b.loadImage(data, sizeof(data), 0x10000000u));

    EXPECT_EQ(b.read8(0x10000000u), 0xAAu);
    EXPECT_EQ(b.read8(0xB0000003u), 0xDDu);
    EXPECT_EQ(b.read32(0x10000000u), 0xAABBCCDDu);   // big-endian default
    EXPECT_EQ(b.read32(0xB0000000u), 0xAABBCCDDu);
    EXPECT_EQ(b.read16(0x10000002u), 0xCCDDu);

    b.write32(0x10000000u, 0x01020304u);             // ignored
    b.write8(0xB0000001u, 0x55);
    b.write16(0x10000002u, 0x6666);
    EXPECT_EQ(b.read32(0x10000000u), 0xAABBCCDDu);

    EXPECT_EQ(b.read32(0x10000004u), 0u);            // beyond the image
    EXPECT_EQ(b.read32(0x10FFFFFCu), 0u);            // end of the window
}

TEST(BusImage, RomWindowLittleEndianAndPartialRead)
{
    Bus b;
    b.setLittleEndian(true);
    const uint8_t data[] = { 0x01, 0x02, 0x03 };     // only 3 bytes loaded
    ASSERT_TRUE(b.loadImage(data, sizeof(data), 0x10000000u));
    EXPECT_EQ(b.read32(0x10000000u), 0x00030201u);   // 4th byte reads as 0
    EXPECT_EQ(b.read16(0x10000000u), 0x0201u);
}

TEST(BusImage, RomImageMayStartInsideTheWindowAndMustFit)
{
    Bus b;
    const uint8_t data[] = { 9, 8, 7, 6 };
    ASSERT_TRUE(b.loadImage(data, sizeof(data), 0x10000010u));
    EXPECT_EQ(b.read8(0x10000010u), 9u);
    EXPECT_EQ(b.read8(0x10000000u), 0u);
    EXPECT_FALSE(b.loadImage(data, sizeof(data), 0x10FFFFFEu));  // crosses the end
}

// ---- Endianness ----------------------------------------------------------
TEST(BusImage, EndiannessFlagChangesWordComposition)
{
    Bus b;
    const uint8_t data[] = { 1, 2, 3, 4 };
    ASSERT_TRUE(b.loadImage(data, sizeof(data), 0x100));

    EXPECT_FALSE(b.isLittleEndian());
    EXPECT_EQ(b.read32(0x100), 0x01020304u);
    EXPECT_EQ(b.read16(0x100), 0x0102u);

    b.setLittleEndian(true);
    EXPECT_TRUE(b.isLittleEndian());
    EXPECT_EQ(b.read32(0x100), 0x04030201u);
    EXPECT_EQ(b.read16(0x100), 0x0201u);
    EXPECT_EQ(b.read16(0x102), 0x0403u);
}

TEST(BusImage, Write16AndWrite32AgreeWithByteReadsInBothOrders)
{
    for (bool le : { false, true })
    {
        Bus b;
        b.setLittleEndian(le);
        b.write16(0x200, 0x1234);
        b.write32(0x204, 0xA1B2C3D4u);
        if (le)
        {
            EXPECT_EQ(b.read8(0x200), 0x34u);  EXPECT_EQ(b.read8(0x201), 0x12u);
            EXPECT_EQ(b.read8(0x204), 0xD4u);  EXPECT_EQ(b.read8(0x207), 0xA1u);
        }
        else
        {
            EXPECT_EQ(b.read8(0x200), 0x12u);  EXPECT_EQ(b.read8(0x201), 0x34u);
            EXPECT_EQ(b.read8(0x204), 0xA1u);  EXPECT_EQ(b.read8(0x207), 0xD4u);
        }
        EXPECT_EQ(b.read16(0x200), 0x1234u);
        EXPECT_EQ(b.read32(0x204), 0xA1B2C3D4u);
    }
}

TEST(BusImage, MmioByteLanesFollowEndianness)
{
    const uint32_t reg = PPU::BASE + PPU::P_SP_Max;

    Bus be;
    be.write32(reg, 0x55u);
    EXPECT_EQ(be.read8(reg + 3), 0x55u);             // low byte is the last one
    be.write8(reg + 2, 0x01);
    EXPECT_EQ(be.read32(reg), 0x155u);

    Bus le;
    le.setLittleEndian(true);
    le.write32(reg, 0x55u);
    EXPECT_EQ(le.read8(reg), 0x55u);                 // low byte is the first one
    le.write8(reg + 1, 0x01);
    EXPECT_EQ(le.read32(reg), 0x155u);
    EXPECT_EQ(le.read16(reg), 0x155u);
    EXPECT_EQ(le.read16(reg + 2), 0u);
}

TEST(BusImage, LoadRomUsesCurrentByteOrder)
{
    const uint32_t words[] = { 0x01020304u };
    Bus be;
    be.loadROM(words, 1);
    EXPECT_EQ(be.read8(0), 0x01u);

    Bus le;
    le.setLittleEndian(true);
    le.loadROM(words, 1);
    EXPECT_EQ(le.read8(0), 0x04u);
    EXPECT_EQ(le.read32(0), 0x01020304u);
}

// ---- loadFile ------------------------------------------------------------
TEST(BusImage, LoadFileMissingOrEmptyReturnsFalse)
{
    Bus b;
    EXPECT_FALSE(b.loadFile("this/path/does/not/exist.bin", 0));

    TempFile empty("ultrascan_bus_empty.bin");
    empty.write({});
    EXPECT_FALSE(b.loadFile(empty.path.string(), 0));
}

TEST(BusImage, LoadFileCopiesBytesVerbatim)
{
    TempFile f("ultrascan_bus_file.bin");
    f.write({ 0x00, 0x83, 0x12, 0x88, 0xFF });

    Bus b;
    ASSERT_TRUE(b.loadFile(f.path.string(), 0x300));
    EXPECT_EQ(b.read8(0x300), 0x00u);
    EXPECT_EQ(b.read8(0x301), 0x83u);
    EXPECT_EQ(b.read8(0x304), 0xFFu);
    EXPECT_EQ(b.read8(0x305), 0u);
}

TEST(BusImage, LoadFileThatDoesNotFitReturnsFalse)
{
    TempFile f("ultrascan_bus_big.bin");
    f.write(std::vector<uint8_t>(64, 0xEE));

    Bus b(32u);
    EXPECT_FALSE(b.loadFile(f.path.string(), 0));
    EXPECT_EQ(b.read8(0), 0u);
}

// ---- Instruction fetch in both byte orders -------------------------------
TEST(BusImage, LittleEndianFetchDecodes32BitInstruction)
{
    Bus b(1024u * 1024u);
    b.setLittleEndian(true);

    // HYPER.EXE vector word: bytes 00 83 12 88 = 0x88128300 (j 0x90300).
    const uint8_t j[] = { 0x00, 0x83, 0x12, 0x88 };
    ASSERT_TRUE(b.loadImage(j, sizeof(j), 0x90200));

    b.cpu.pc = 0x90200;
    b.cpu.clock();
    EXPECT_EQ(b.cpu.pc, 0x90300u);
}

TEST(BusImage, LittleEndianFetchAdvancesByTwoForSixteenBitInstructions)
{
    Bus b(1024u * 1024u);
    b.setLittleEndian(true);

    // nop! = 0x0000 (bytes 00 00); b! AL, disp 0 = 0x4F00 (bytes 00 4F),
    // a branch to itself.
    const uint8_t code[] = { 0x00, 0x00, 0x00, 0x4F };
    ASSERT_TRUE(b.loadImage(code, sizeof(code), 0x1000));

    b.cpu.pc = 0x1000;
    b.cpu.clock();
    EXPECT_EQ(b.cpu.pc, 0x1002u);                    // nop!: +2

    b.cpu.clock();
    EXPECT_EQ(b.cpu.pc, 0x1002u);                    // b! to itself: unchanged
}

TEST(BusImage, BigEndianFetchUnchanged)
{
    Bus b(1024u * 1024u);
    b.write32(0x1000, encodeJX(0x4000, false));      // j to 0x8000
    b.cpu.pc = 0x1000;
    b.cpu.clock();
    EXPECT_EQ(b.cpu.pc, 0x8000u);
}
