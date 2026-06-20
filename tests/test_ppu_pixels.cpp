// ============================================================================
// PPU pixel-unpacking, geometry, scale and priority tests
//
// Covers: colour-mode bits/colours, character size decode (section 9.4.4),
// indexed pixel unpacking from packed little-endian character data
// (section 9.4.5), character flip (section 9.4.3), compression scale math
// (section 9.9) and depth/priority compositing order (section 9.4.2).
// ============================================================================
#include <gtest/gtest.h>
#include "ppu.h"
#include <vector>

using ColorMode = PPU::ColorMode;
using Flip = PPU::Flip;
using LayerRef = PPU::LayerRef;

// Independent reference packer: lay out palette indices LSB-first, exactly as
// section 9.4.5 describes (earliest pixel in the lowest-order bits).
static std::vector<uint8_t> packLSB(const std::vector<uint32_t> &idxs, int bpp)
{
	size_t totalBits = idxs.size() * bpp;
	std::vector<uint8_t> out((totalBits + 7) / 8, 0);
	size_t bit = 0;
	for (uint32_t v : idxs)
		for (int i = 0; i < bpp; i++, bit++)
			if ((v >> i) & 1) out[bit >> 3] |= (1u << (bit & 7));
	return out;
}

// --- Colour-mode descriptors ------------------------------------------------
TEST(PPUPixels, BitsPerPixel)
{
	EXPECT_EQ(PPU::bitsPerPixel(ColorMode::BPP2), 2);
	EXPECT_EQ(PPU::bitsPerPixel(ColorMode::BPP4), 4);
	EXPECT_EQ(PPU::bitsPerPixel(ColorMode::BPP6), 6);
	EXPECT_EQ(PPU::bitsPerPixel(ColorMode::BPP8), 8);
}

TEST(PPUPixels, ColorCount)
{
	EXPECT_EQ(PPU::colorCount(ColorMode::BPP2), 4);
	EXPECT_EQ(PPU::colorCount(ColorMode::BPP4), 16);
	EXPECT_EQ(PPU::colorCount(ColorMode::BPP6), 64);
	EXPECT_EQ(PPU::colorCount(ColorMode::BPP8), 256);
}

TEST(PPUPixels, SizeFromCode)
{
	EXPECT_EQ(PPU::sizeFromCode(0), 8);
	EXPECT_EQ(PPU::sizeFromCode(1), 16);
	EXPECT_EQ(PPU::sizeFromCode(2), 32);
	EXPECT_EQ(PPU::sizeFromCode(3), 64);
}

// --- Indexed pixel unpacking ------------------------------------------------
TEST(PPUPixels, Unpack8bpp)
{
	std::vector<uint8_t> d = {10, 20, 30, 255};
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 0, ColorMode::BPP8), 10u);
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 1, ColorMode::BPP8), 20u);
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 3, ColorMode::BPP8), 255u);
}

TEST(PPUPixels, Unpack4bpp_LowNibbleFirst)
{
	// 0x21 -> pixel0 = 1 (low nibble), pixel1 = 2 (high nibble).
	std::vector<uint8_t> d = {0x21, 0x43};
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 0, ColorMode::BPP4), 1u);
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 1, ColorMode::BPP4), 2u);
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 2, ColorMode::BPP4), 3u);
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 3, ColorMode::BPP4), 4u);
}

TEST(PPUPixels, Unpack2bpp)
{
	// 0xE4 = 11 10 01 00 -> pixels 0,1,2,3 (LSB pair first).
	std::vector<uint8_t> d = {0xE4};
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 0, ColorMode::BPP2), 0u);
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 1, ColorMode::BPP2), 1u);
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 2, ColorMode::BPP2), 2u);
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 3, ColorMode::BPP2), 3u);
}

TEST(PPUPixels, UnpackRoundTripAllModes)
{
	std::vector<uint32_t> idx2 = {0, 1, 2, 3, 3, 2, 1, 0};
	std::vector<uint32_t> idx4 = {0, 1, 15, 8, 4, 2, 9, 12};
	std::vector<uint32_t> idx6 = {0, 1, 63, 21, 42, 7, 33, 60};
	std::vector<uint32_t> idx8 = {0, 1, 255, 128, 77, 200, 33, 199};

	struct Case { std::vector<uint32_t> idx; ColorMode mode; int bpp; };
	std::vector<Case> cases = {
		{idx2, ColorMode::BPP2, 2},
		{idx4, ColorMode::BPP4, 4},
		{idx6, ColorMode::BPP6, 6},
		{idx8, ColorMode::BPP8, 8},
	};

	for (auto &c : cases)
	{
		auto data = packLSB(c.idx, c.bpp);
		for (size_t i = 0; i < c.idx.size(); i++)
			EXPECT_EQ(PPU::unpackIndex(data.data(), data.size(), (uint32_t)i, c.mode),
			          c.idx[i]) << "bpp=" << c.bpp << " pixel=" << i;
	}
}

TEST(PPUPixels, UnpackPastEndReadsZero)
{
	std::vector<uint8_t> d = {0xFF};
	EXPECT_EQ(PPU::unpackIndex(d.data(), d.size(), 100, ColorMode::BPP8), 0u);
}

// --- Character flip ---------------------------------------------------------
TEST(PPUPixels, FlipNone)
{
	uint32_t x = 2, y = 3;
	PPU::applyFlip(Flip::NONE, x, y, 8, 8);
	EXPECT_EQ(x, 2u);
	EXPECT_EQ(y, 3u);
}

TEST(PPUPixels, FlipHorizontal)
{
	uint32_t x = 0, y = 3;
	PPU::applyFlip(Flip::H, x, y, 8, 8);
	EXPECT_EQ(x, 7u);
	EXPECT_EQ(y, 3u);
}

TEST(PPUPixels, FlipVertical)
{
	uint32_t x = 2, y = 0;
	PPU::applyFlip(Flip::V, x, y, 8, 8);
	EXPECT_EQ(x, 2u);
	EXPECT_EQ(y, 7u);
}

TEST(PPUPixels, FlipBoth)
{
	uint32_t x = 1, y = 1;
	PPU::applyFlip(Flip::HV, x, y, 16, 16);
	EXPECT_EQ(x, 14u);
	EXPECT_EQ(y, 14u);
}

// --- Compression scale math (section 9.9) -----------------------------------
TEST(PPUPixels, VerticalScaleAnchors)
{
	EXPECT_DOUBLE_EQ(PPU::verticalSourceStep(0x20), 1.0); // unit gain
	EXPECT_DOUBLE_EQ(PPU::verticalSourceStep(0x40), 2.0); // half size (compress)
	EXPECT_DOUBLE_EQ(PPU::verticalSourceStep(0x10), 0.5); // double size (expand)
}

TEST(PPUPixels, HorizontalScaleAnchors)
{
	EXPECT_DOUBLE_EQ(PPU::horizontalSourceStep(0x80), 1.0); // unit gain
	EXPECT_DOUBLE_EQ(PPU::horizontalSourceStep(0x40), 0.5); // half size
}

// --- Depth / priority ordering (section 9.4.2) ------------------------------
TEST(PPUPriority, HigherDepthIsAbove)
{
	EXPECT_TRUE(PPU::isAbove(LayerRef{false, 3, 0}, LayerRef{false, 0, 0}));
	EXPECT_FALSE(PPU::isAbove(LayerRef{false, 0, 0}, LayerRef{false, 3, 0}));
}

TEST(PPUPriority, SpriteAboveTextAtSameDepth)
{
	EXPECT_TRUE(PPU::isAbove(LayerRef{true, 2, 0}, LayerRef{false, 2, 5}));
	EXPECT_FALSE(PPU::isAbove(LayerRef{false, 2, 5}, LayerRef{true, 2, 0}));
}

TEST(PPUPriority, HigherSpriteIndexWins)
{
	EXPECT_TRUE(PPU::isAbove(LayerRef{true, 1, 10}, LayerRef{true, 1, 3}));
	EXPECT_FALSE(PPU::isAbove(LayerRef{true, 1, 3}, LayerRef{true, 1, 10}));
}

TEST(PPUPriority, HigherTextLayerWins)
{
	// Text3 (subIndex 2) dominates Text1 (subIndex 0) at equal depth.
	EXPECT_TRUE(PPU::isAbove(LayerRef{false, 0, 2}, LayerRef{false, 0, 0}));
}
