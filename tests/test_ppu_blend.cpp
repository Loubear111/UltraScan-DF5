// ============================================================================
// PPU alpha-blending tests (section 9.13 / P_Blend_Sub)
//
// 64-grade blending. alpha = level/64.
//   Blend_Sub = 0 : C = top*alpha + bottom*(1-alpha)
//   Blend_Sub = 1 : C = top*alpha - bottom*(1-alpha)   (clamped to >= 0)
// ============================================================================
#include <gtest/gtest.h>
#include "ppu.h"

using Color = PPU::Color;

TEST(PPUBlend, LevelZeroKeepsBottom)
{
	// alpha = 0 -> result equals the bottom (destination) colour.
	EXPECT_EQ(PPU::blendChannel(200, 100, 0, false), 100);
	EXPECT_EQ(PPU::blendChannel(0, 255, 0, false), 255);
}

TEST(PPUBlend, MaxLevelIsAlmostTop)
{
	// level 63 -> (top*63 + bottom)/64
	EXPECT_EQ(PPU::blendChannel(255, 255, 63, false), 255);
	EXPECT_EQ(PPU::blendChannel(64, 0, 63, false), 63); // 64*63/64
}

TEST(PPUBlend, MidpointAdditive)
{
	// level 32 -> exactly half top + half bottom.
	EXPECT_EQ(PPU::blendChannel(200, 100, 32, false), 150); // (6400+3200)/64
	EXPECT_EQ(PPU::blendChannel(100, 200, 32, false), 150);
	EXPECT_EQ(PPU::blendChannel(0, 0, 32, false), 0);
}

TEST(PPUBlend, SubtractiveMode)
{
	// level 32 -> (top*32 - bottom*32)/64
	EXPECT_EQ(PPU::blendChannel(200, 100, 32, true), 50); // (6400-3200)/64
	EXPECT_EQ(PPU::blendChannel(255, 0, 32, true), 127);  // (255*32)/64
}

TEST(PPUBlend, SubtractiveClampsToZero)
{
	EXPECT_EQ(PPU::blendChannel(0, 255, 32, true), 0);
	// top*level - bottom*(64-level) = 10*32 - 255*32 < 0 -> clamp to 0.
	EXPECT_EQ(PPU::blendChannel(10, 255, 32, true), 0);
}

TEST(PPUBlend, AdditiveSaturatesAtMax)
{
	// Additive blend can never exceed 255 since alpha+ (1-alpha) = 1, but
	// verify the boundary remains in range.
	EXPECT_LE(PPU::blendChannel(255, 255, 40, false), 255);
	EXPECT_EQ(PPU::blendChannel(255, 255, 40, false), 255);
}

TEST(PPUBlend, ColorBlendsPerChannel)
{
	Color top{200, 100, 50};
	Color bottom{100, 200, 250};
	Color out = PPU::blendColor(top, bottom, 32, false);
	EXPECT_EQ(out.r, 150);
	EXPECT_EQ(out.g, 150);
	EXPECT_EQ(out.b, 150);
}

TEST(PPUBlend, LevelMaskedToSixBits)
{
	// Only the low 6 bits of the level are significant.
	EXPECT_EQ(PPU::blendChannel(200, 100, 0x40 | 0, false), 100);
}
