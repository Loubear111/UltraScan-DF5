// ============================================================================
// PPU colour / palette conversion tests
//
// Covers the pixel-format helpers from sections 9.3 (palette memory) and
// 9.14 (high colour modes): 5/6-bit channel expansion, RGB555 / ARGB1555 /
// RGB565 decode, transparent-bit handling, and palette-bank addressing
// (section 9.3.1).
// ============================================================================
#include <gtest/gtest.h>
#include "ppu.h"

using Color = PPU::Color;
using Pixel = PPU::Pixel;
using ColorMode = PPU::ColorMode;

// --- 5/6-bit -> 8-bit channel expansion (bit replication) -------------------
TEST(PPUColor, Expand5Endpoints)
{
	EXPECT_EQ(PPU::expand5(0), 0);
	EXPECT_EQ(PPU::expand5(31), 255);
	EXPECT_EQ(PPU::expand5(1), 8);   // (1<<3)|(1>>2)
	EXPECT_EQ(PPU::expand5(16), 132); // (16<<3)|(16>>2) = 128|4
}

TEST(PPUColor, Expand6Endpoints)
{
	EXPECT_EQ(PPU::expand6(0), 0);
	EXPECT_EQ(PPU::expand6(63), 255);
	EXPECT_EQ(PPU::expand6(1), 4);   // (1<<2)|(1>>4)
	EXPECT_EQ(PPU::expand6(32), 130); // (32<<2)|(32>>4) = 128|2
}

// --- RGB555 / ARGB1555 ------------------------------------------------------
TEST(PPUColor, RGB555PureChannels)
{
	Pixel red = PPU::decodeRGB555(0x1F << 10);
	EXPECT_FALSE(red.transparent);
	EXPECT_EQ(red.color.r, 255);
	EXPECT_EQ(red.color.g, 0);
	EXPECT_EQ(red.color.b, 0);

	Pixel green = PPU::decodeRGB555(0x1F << 5);
	EXPECT_EQ(green.color.g, 255);
	EXPECT_EQ(green.color.r, 0);
	EXPECT_EQ(green.color.b, 0);

	Pixel blue = PPU::decodeRGB555(0x1F);
	EXPECT_EQ(blue.color.b, 255);
	EXPECT_EQ(blue.color.r, 0);
	EXPECT_EQ(blue.color.g, 0);
}

TEST(PPUColor, RGB555TransparentBit)
{
	// bit15 set -> transparent regardless of colour bits.
	Pixel p = PPU::decodeRGB555(0x8000 | (0x1F << 10));
	EXPECT_TRUE(p.transparent);
	EXPECT_EQ(p.color.r, 255);

	Pixel q = PPU::decodeRGB555(0x7FFF); // all colour bits, no Tr
	EXPECT_FALSE(q.transparent);
	EXPECT_EQ(q.color.r, 255);
	EXPECT_EQ(q.color.g, 255);
	EXPECT_EQ(q.color.b, 255);
}

TEST(PPUColor, PaletteEntryUsesRGB555)
{
	// decodePaletteEntry must honour the Tr bit at bit15.
	Pixel solid = PPU::decodePaletteEntry(0x03E0); // green, Tr=0
	EXPECT_FALSE(solid.transparent);
	EXPECT_EQ(solid.color.g, 255);

	Pixel clear = PPU::decodePaletteEntry(0x8000);
	EXPECT_TRUE(clear.transparent);
}

// --- RGB565 -----------------------------------------------------------------
TEST(PPUColor, RGB565PureChannels)
{
	Color red = PPU::decodeRGB565(0xF800);
	EXPECT_EQ(red.r, 255);
	EXPECT_EQ(red.g, 0);
	EXPECT_EQ(red.b, 0);

	Color green = PPU::decodeRGB565(0x07E0);
	EXPECT_EQ(green.g, 255);
	EXPECT_EQ(green.r, 0);
	EXPECT_EQ(green.b, 0);

	Color blue = PPU::decodeRGB565(0x001F);
	EXPECT_EQ(blue.b, 255);
	EXPECT_EQ(blue.r, 0);
	EXPECT_EQ(blue.g, 0);

	Color white = PPU::decodeRGB565(0xFFFF);
	EXPECT_EQ(white.r, 255);
	EXPECT_EQ(white.g, 255);
	EXPECT_EQ(white.b, 255);
}

// --- Palette-bank addressing (section 9.3.1) --------------------------------
TEST(PPUColor, PaletteEntryIndexByMode)
{
	// entryIndex = bank * colorCount(mode) + colorIndex
	EXPECT_EQ(PPU::paletteEntryIndex(0, 5, ColorMode::BPP4), 5u);
	EXPECT_EQ(PPU::paletteEntryIndex(2, 3, ColorMode::BPP4), 2u * 16 + 3);
	EXPECT_EQ(PPU::paletteEntryIndex(1, 10, ColorMode::BPP8), 1u * 256 + 10);
	EXPECT_EQ(PPU::paletteEntryIndex(3, 1, ColorMode::BPP6), 3u * 64 + 1);
	EXPECT_EQ(PPU::paletteEntryIndex(7, 2, ColorMode::BPP2), 7u * 4 + 2);
}

TEST(PPUColor, PalettePhysAddr)
{
	EXPECT_EQ(PPU::palettePhysAddr(0, 0, ColorMode::BPP4),
	          PPU::BASE + 0x1000u);
	EXPECT_EQ(PPU::palettePhysAddr(2, 3, ColorMode::BPP4),
	          PPU::BASE + 0x1000u + (2u * 16 + 3) * 4);
}
