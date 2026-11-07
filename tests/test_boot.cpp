// Fake-BIOS boot defaults (plan story 12): load at 0xA00901FC, start at
// 0xA0091000, touch nothing but the PC.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include "test_helpers.h"
#include "Boot.h"

namespace {

std::vector<uint8_t> patternImage(size_t n)
{
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; ++i)
        v[i] = (uint8_t)(i * 7 + 3);
    return v;
}

} // namespace

TEST(Boot, DefaultsAreMameQuickloadValues)
{
    BootParams p;
    EXPECT_EQ(p.loadAddr, 0xA00901FCu);
    EXPECT_EQ(p.entryPc, 0xA0091000u);
    EXPECT_TRUE(p.littleEndian);
    EXPECT_FALSE(p.cr3.has_value());
    EXPECT_FALSE(p.sp.has_value());
}

TEST(Boot, ImageLandsAtThe901FCVectorBaseAndPcStartsAt91000)
{
    Bus b(Bus::SYSTEM_RAM_SIZE);
    const auto img = patternImage(0x2000);
    ASSERT_TRUE(bootExecutable(b, img.data(), img.size()));

    EXPECT_EQ(b.read8(0x000901FC), img[0]);
    EXPECT_EQ(b.read8(0xA00901FC), img[0]);              // uncached alias
    EXPECT_EQ(b.read8(0x000901FC + 0x1234), img[0x1234]);
    EXPECT_EQ(b.cpu.pc, 0xA0091000u);
    EXPECT_TRUE(b.isLittleEndian());
    EXPECT_EQ(b.read8(0x000901FB), 0u);                  // nothing before the image
}

TEST(Boot, TouchesNoRegisterBesidesThePc)
{
    Bus b(Bus::SYSTEM_RAM_SIZE);
    for (int i = 0; i < 32; ++i)
    {
        b.cpu.regs[i] = 1000u + i;
        b.cpu.cr[i]   = 2000u + i;
        b.cpu.sr[i]   = 3000u + i;
    }
    b.cpu.status = 0x15;
    b.cpu.ceh = 11;
    b.cpu.cel = 22;

    const auto img = patternImage(64);
    ASSERT_TRUE(bootExecutable(b, img.data(), img.size()));

    for (int i = 0; i < 32; ++i)
    {
        EXPECT_EQ(b.cpu.regs[i], 1000u + i) << "r" << i;
        EXPECT_EQ(b.cpu.cr[i], 2000u + i) << "cr" << i;
        EXPECT_EQ(b.cpu.sr[i], 3000u + i) << "sr" << i;
    }
    EXPECT_EQ(b.cpu.status, 0x15u);
    EXPECT_EQ(b.cpu.ceh, 11u);
    EXPECT_EQ(b.cpu.cel, 22u);
}

TEST(Boot, Cr3AndSpAreAppliedOnlyWhenGiven)
{
    Bus b(Bus::SYSTEM_RAM_SIZE);
    const auto img = patternImage(64);
    BootParams p;
    p.cr3 = 0xA0000000u;
    p.sp  = 0x08000000u;
    ASSERT_TRUE(bootExecutable(b, img.data(), img.size(), p));
    EXPECT_EQ(b.cpu.cr[3], 0xA0000000u);
    EXPECT_EQ(b.cpu.regs[0], 0x08000000u);
}

TEST(Boot, ImageThatDoesNotFitChangesNothing)
{
    Bus b(1024u);
    b.cpu.pc = 0x1234;
    const auto img = patternImage(2048);
    EXPECT_FALSE(bootExecutable(b, img.data(), img.size()));
    EXPECT_EQ(b.cpu.pc, 0x1234u);
    EXPECT_FALSE(b.isLittleEndian());
    EXPECT_FALSE(bootExecutable(b, img.data(), 0));
}

TEST(Boot, FirstInstructionAtTheEntryPointRuns)
{
    Bus b(Bus::SYSTEM_RAM_SIZE);
    const uint32_t entryOffset = 0xA0091000u - 0xA00901FCu;          // 0xE04
    std::vector<uint8_t> img(entryOffset + 8, 0);
    const uint32_t word = encodeLDI(4, 1);                           // ldi r4, 1
    for (int i = 0; i < 4; ++i)
        img[entryOffset + i] = (uint8_t)(word >> (8 * i));           // little-endian

    ASSERT_TRUE(bootExecutable(b, img.data(), img.size()));
    b.cpu.clock();
    EXPECT_EQ(b.cpu.regs[4], 1u);
    EXPECT_EQ(b.cpu.pc, 0xA0091004u);
    EXPECT_TRUE(b.cpu.faultEvents().empty());
}

TEST(Boot, BootFromFile)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "ultrascan_boot_test.bin";
    const auto img = patternImage(256);
    {
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(img.data()), (std::streamsize)img.size());
    }

    Bus b(Bus::SYSTEM_RAM_SIZE);
    EXPECT_TRUE(bootExecutableFile(b, path.string()));
    EXPECT_EQ(b.read8(0x000901FC + 10), img[10]);
    EXPECT_EQ(b.cpu.pc, 0xA0091000u);
    std::filesystem::remove(path);

    Bus c(Bus::SYSTEM_RAM_SIZE);
    EXPECT_FALSE(bootExecutableFile(c, (path.parent_path() / "ultrascan_missing.bin").string()));
    EXPECT_EQ(c.cpu.pc, 0u);
}
