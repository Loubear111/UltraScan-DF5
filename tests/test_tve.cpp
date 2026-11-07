// TV encoder registers and framebuffer scan-out (plan story 11).
#include <gtest/gtest.h>
#include "Bus.h"

namespace {

// Store one RGB565 pixel little-endian at `addr`, whatever the bus byte order is.
void putPixel(Bus& b, uint32_t addr, uint16_t v)
{
    b.write8(addr, (uint8_t)(v & 0xFF));
    b.write8(addr + 1, (uint8_t)(v >> 8));
}

PPU::Color rgb(uint8_t r, uint8_t g, uint8_t bl) { PPU::Color c; c.r = r; c.g = g; c.b = bl; return c; }

} // namespace

TEST(TVE, RegistersRoundTripAndHaveResetValues)
{
    Bus b;
    EXPECT_EQ(b.read32(TVE::BUFSEL), 3u);                // framebuffer off at reset
    EXPECT_EQ(b.read32(TVE::CONTROL), 0u);

    b.write32(TVE::CONTROL, 0x0000000Du);
    b.write32(TVE::FADE, 0x1234u);
    b.write32(TVE::START0, 0xA0100000u);
    b.write32(TVE::START1, 0xA0200000u);
    b.write32(TVE::START2, 0xA0300000u);
    b.write32(TVE::BUFSEL, 0xFFFFFFFEu);

    EXPECT_EQ(b.read32(TVE::CONTROL), 0x0Du);
    EXPECT_EQ(b.read32(TVE::FADE), 0x34u);               // low 8 bits
    EXPECT_EQ(b.read32(TVE::START0), 0xA0100000u);
    EXPECT_EQ(b.read32(TVE::START1), 0xA0200000u);
    EXPECT_EQ(b.read32(TVE::START2), 0xA0300000u);
    EXPECT_EQ(b.read32(TVE::BUFSEL), 2u);                // value & 3
    EXPECT_EQ(b.read32(0x88030000u), 0x0Du);             // uncached alias
    EXPECT_TRUE(b.topUnmappedPages().empty());
}

TEST(TVE, NeighbouringAddressesStayUnmapped)
{
    Bus b;
    b.read32(0x08030050);                                // touched by the demos, not modelled
    b.write32(0x08070010, 1);
    EXPECT_EQ(b.topUnmappedPages().size(), 2u);
    EXPECT_EQ(b.read32(TVE::CONTROL), 0u);
}

TEST(TVE, ModeSelectsOutputSize)
{
    Bus b;
    int w = 0, h = 0;
    b.tve.write32(TVE::CONTROL, 0u << 2);
    auto f = b.tve.compose(b, nullptr, w, h);
    EXPECT_EQ(w, 320); EXPECT_EQ(h, 240); EXPECT_EQ(f.size(), 320u * 240u);

    b.tve.write32(TVE::CONTROL, 1u << 2);
    f = b.tve.compose(b, nullptr, w, h);
    EXPECT_EQ(w, 640); EXPECT_EQ(h, 480); EXPECT_EQ(f.size(), 640u * 480u);

    b.tve.write32(TVE::CONTROL, 2u << 2);
    f = b.tve.compose(b, nullptr, w, h);
    EXPECT_EQ(w, 640); EXPECT_EQ(h, 240); EXPECT_EQ(f.size(), 640u * 240u);
}

TEST(TVE, BufferSelectThreeLeavesTheFrameBlack)
{
    Bus b;
    putPixel(b, 0x10000, 0xFFFF);
    b.tve.write32(TVE::START0, 0x10000);
    b.tve.write32(TVE::BUFSEL, 3);
    int w, h;
    auto f = b.tve.compose(b, nullptr, w, h);
    EXPECT_EQ(f[0], rgb(0, 0, 0));
}

TEST(TVE, Rgb565PixelsAreExpandedTo888AtTheRightPlaces)
{
    for (bool le : { false, true })
    {
        Bus b;
        b.setLittleEndian(le);
        // 4x2 pattern at the top-left of the buffer; interlaced bit set so rows are not doubled.
        const uint16_t row0[4] = { 0xF800, 0x07E0, 0x001F, 0xFFFF };
        const uint16_t row1[4] = { 0x0000, 0x8410, 0x1082, 0x0841 };
        for (int x = 0; x < 4; ++x)
        {
            putPixel(b, 0x20000 + x * 2, row0[x]);
            putPixel(b, 0x20000 + (320 + x) * 2, row1[x]);
        }
        b.tve.write32(TVE::CONTROL, 1u);                 // QVGA, interlaced
        b.tve.write32(TVE::START0, 0x20000);
        b.tve.write32(TVE::BUFSEL, 0);

        int w, h;
        auto f = b.tve.compose(b, nullptr, w, h);
        ASSERT_EQ(w, 320);
        EXPECT_EQ(f[0], rgb(255, 0, 0)) << "le=" << le;
        EXPECT_EQ(f[1], rgb(0, 255, 0));
        EXPECT_EQ(f[2], rgb(0, 0, 255));
        EXPECT_EQ(f[3], rgb(255, 255, 255));
        EXPECT_EQ(f[320 + 0], rgb(0, 0, 0));
        EXPECT_EQ(f[320 + 1], rgb(132, 130, 132));       // 5/6/5 expansion replicates the top bits
        EXPECT_EQ(f[320 + 2], rgb(16, 16, 16));          // r5=2 -> 16, g6=4 -> 16, b5=2 -> 16
        EXPECT_EQ(f[320 + 3], rgb(8, 8, 8));             // r5=1 -> 8,  g6=2 -> 8,  b5=1 -> 8
        EXPECT_EQ(f[4], rgb(0, 0, 0));                   // beyond the pattern
    }
}

TEST(TVE, ProgressiveFramesReadEvenLinesAndDoubleThem)
{
    Bus b;
    putPixel(b, 0x20000, 0xF800);                        // line 0: red
    putPixel(b, 0x20000 + 320 * 2, 0x07E0);              // line 1: green
    putPixel(b, 0x20000 + 640 * 2, 0x001F);              // line 2: blue
    b.tve.write32(TVE::START0, 0x20000);
    b.tve.write32(TVE::BUFSEL, 0);
    int w, h;

    b.tve.write32(TVE::CONTROL, 0);                      // progressive
    auto f = b.tve.compose(b, nullptr, w, h);
    EXPECT_EQ(f[0], rgb(255, 0, 0));
    EXPECT_EQ(f[320], rgb(255, 0, 0));                   // line 1 repeats line 0
    EXPECT_EQ(f[640], rgb(0, 0, 255));

    b.tve.progressiveLineDoubling = false;
    f = b.tve.compose(b, nullptr, w, h);
    EXPECT_EQ(f[320], rgb(0, 255, 0));                   // every line is read as it is
}

TEST(TVE, BufferSelectSwitchesBetweenTheThreeStartAddresses)
{
    Bus b;
    putPixel(b, 0x10000, 0xF800);
    putPixel(b, 0x20000, 0x07E0);
    putPixel(b, 0x30000, 0x001F);
    b.tve.write32(TVE::START0, 0x10000);
    b.tve.write32(TVE::START1, 0x20000);
    b.tve.write32(TVE::START2, 0x30000);
    int w, h;
    const PPU::Color expect[3] = { rgb(255, 0, 0), rgb(0, 255, 0), rgb(0, 0, 255) };
    for (uint32_t sel = 0; sel < 3; ++sel)
    {
        b.tve.write32(TVE::BUFSEL, sel);
        EXPECT_EQ(b.tve.compose(b, nullptr, w, h)[0], expect[sel]) << "buffer " << sel;
    }
}

TEST(TVE, FadeScalesEveryChannel)
{
    Bus b;
    putPixel(b, 0x10000, 0xFFFF);
    b.tve.write32(TVE::START0, 0x10000);
    b.tve.write32(TVE::BUFSEL, 0);
    int w, h;

    b.tve.write32(TVE::FADE, 0);
    EXPECT_EQ(b.tve.compose(b, nullptr, w, h)[0], rgb(255, 255, 255));
    b.tve.write32(TVE::FADE, 128);
    EXPECT_EQ(b.tve.compose(b, nullptr, w, h)[0], rgb(127, 127, 127));   // 255 * 127 / 255
    b.tve.write32(TVE::FADE, 255);
    EXPECT_EQ(b.tve.compose(b, nullptr, w, h)[0], rgb(0, 0, 0));
}

TEST(TVE, PpuFrameShowsThroughUnlessTheFramebufferCoversIt)
{
    Bus b;
    int pw = 0, ph = 0;
    b.ppu.screenSize(pw, ph);
    std::vector<PPU::Color> ppuFrame((size_t)pw * (size_t)ph);
    ppuFrame[0] = rgb(10, 20, 30);
    ppuFrame[1] = rgb(40, 50, 60);

    int w, h;
    auto f = b.tve.compose(b, &ppuFrame, w, h);          // buffer select 3: PPU only
    EXPECT_EQ(f[0], rgb(10, 20, 30));
    EXPECT_EQ(f[1], rgb(40, 50, 60));

    putPixel(b, 0x10000, 0xFFFF);                        // framebuffer pixel 0 only; pixel 1 is black RGB565
    b.tve.write32(TVE::START0, 0x10000);
    b.tve.write32(TVE::BUFSEL, 0);
    f = b.tve.compose(b, &ppuFrame, w, h);
    EXPECT_EQ(f[0], rgb(255, 255, 255));                 // the framebuffer overwrites, it does not blend
    EXPECT_EQ(f[1], rgb(0, 0, 0));
}
