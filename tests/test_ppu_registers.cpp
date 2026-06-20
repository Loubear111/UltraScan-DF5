// ============================================================================
// PPU register-file tests (section 9.19)
//
// Exercises the memory-mapped register block: word read/write across every
// region (control, palette, per-line offset/compression, sprite), absolute vs
// BASE-relative addressing, typed field accessors and attribute decode.
// ============================================================================
#include <gtest/gtest.h>
#include "ppu.h"

using ColorMode = PPU::ColorMode;
using Flip = PPU::Flip;

// --- Basic round-trip in every region --------------------------------------
TEST(PPURegs, ControlRoundTripRelativeAndAbsolute)
{
	PPU ppu;
	ppu.write32(PPU::P_SP_Max, 0x1A3);
	EXPECT_EQ(ppu.read32(PPU::P_SP_Max), 0x1A3u & 0xFFFFFFFF);
	// Absolute address reads back the same register.
	EXPECT_EQ(ppu.read32(PPU::BASE + PPU::P_SP_Max), ppu.read32(PPU::P_SP_Max));

	ppu.write32(PPU::BASE + PPU::P_Blend_Sub, 0x1);
	EXPECT_EQ(ppu.read32(PPU::P_Blend_Sub), 0x1u);
}

TEST(PPURegs, PaletteRegionRoundTrip)
{
	PPU ppu;
	ppu.write32(PPU::PALETTE_BASE + 0 * 4, 0x1234);
	ppu.write32(PPU::PALETTE_BASE + 511 * 4, 0xABCD);
	EXPECT_EQ(ppu.read32(PPU::PALETTE_BASE + 0 * 4), 0x1234u);
	EXPECT_EQ(ppu.read32(PPU::PALETTE_BASE + 511 * 4), 0xABCDu);
	EXPECT_EQ(ppu.paletteRaw(0), 0x1234u);
	EXPECT_EQ(ppu.paletteRaw(511), 0xABCDu);
}

TEST(PPURegs, PerLineRegionsRoundTrip)
{
	PPU ppu;
	ppu.write32(PPU::HVOFFSET_BASE + 7 * 4, 0x155);
	ppu.write32(PPU::HCOMP_BASE + 9 * 4, 0x0AB);
	EXPECT_EQ(ppu.hvOffset(7), 0x155u);
	EXPECT_EQ(ppu.hCompValue(9), 0x0ABu);
}

TEST(PPURegs, SpriteRegionTwoWordView)
{
	PPU ppu;
	// Sprite 3 occupies offset 0x4000 + 3*8.
	uint32_t numX = 0x4018; // 0x4000 + 24
	ppu.write32(PPU::SPRITE_BASE + 3 * 8 + 0, 0xDEAD0007);
	ppu.write32(PPU::SPRITE_BASE + 3 * 8 + 4, 0xBEEF0009);
	EXPECT_EQ(ppu.read32(PPU::SPRITE_BASE + 3 * 8 + 0), 0xDEAD0007u);
	EXPECT_EQ(ppu.read32(PPU::SPRITE_BASE + 3 * 8 + 4), 0xBEEF0009u);
	(void)numX;
}

TEST(PPURegs, UnmappedReadsZero)
{
	PPU ppu;
	EXPECT_EQ(ppu.read32(PPU::PALETTE_BASE - 4), 0u); // 0x0FFC reserved gap
}

TEST(PPURegs, ResetClearsEverything)
{
	PPU ppu;
	ppu.write32(PPU::P_PPU_Control, 0xFFFF);
	ppu.write32(PPU::PALETTE_BASE, 0x1234);
	ppu.write32(PPU::SPRITE_BASE, 0x5678);
	ppu.reset();
	EXPECT_EQ(ppu.read32(PPU::P_PPU_Control), 0u);
	EXPECT_EQ(ppu.read32(PPU::PALETTE_BASE), 0u);
	EXPECT_EQ(ppu.read32(PPU::SPRITE_BASE), 0u);
}

// --- P_PPU_Control ----------------------------------------------------------
TEST(PPURegs, PpuEnableAndResolution)
{
	PPU ppu;
	EXPECT_FALSE(ppu.ppuEnabled());

	ppu.write32(PPU::P_PPU_Control, PPU::PPUEN_BIT | PPU::RES_VGA);
	EXPECT_TRUE(ppu.ppuEnabled());
	EXPECT_EQ(ppu.resolution(), PPU::RES_VGA);

	int w, h;
	ppu.screenSize(w, h);
	EXPECT_EQ(w, 640);
	EXPECT_EQ(h, 480);

	ppu.write32(PPU::P_PPU_Control, PPU::PPUEN_BIT | PPU::RES_QVGA);
	ppu.screenSize(w, h);
	EXPECT_EQ(w, 320);
	EXPECT_EQ(h, 240);

	ppu.write32(PPU::P_PPU_Control, PPU::PPUEN_BIT | PPU::RES_HVGA);
	ppu.screenSize(w, h);
	EXPECT_EQ(w, 640);
	EXPECT_EQ(h, 240);
}

// --- P_SP_Control / P_Blend_Sub / P_Trans_RGB -------------------------------
TEST(PPURegs, SpriteControlFields)
{
	PPU ppu;
	EXPECT_FALSE(ppu.spriteEnabled());
	EXPECT_FALSE(ppu.coordTopLeft());

	ppu.write32(PPU::P_SP_Control, PPU::SP_EN_BIT | PPU::COORD_SEL_BIT);
	EXPECT_TRUE(ppu.spriteEnabled());
	EXPECT_TRUE(ppu.coordTopLeft());

	ppu.write32(PPU::P_SP_Max, 511);
	EXPECT_EQ(ppu.spriteMax(), 511u);
}

TEST(PPURegs, BlendSubAndTransRGB)
{
	PPU ppu;
	EXPECT_FALSE(ppu.blendSubtract());
	ppu.write32(PPU::P_Blend_Sub, PPU::BLEND_SUB_BIT);
	EXPECT_TRUE(ppu.blendSubtract());

	EXPECT_FALSE(ppu.transRGBEnabled());
	ppu.write32(PPU::P_Trans_RGB, PPU::TRANS_RGB_EN_BIT | 0xF81F);
	EXPECT_TRUE(ppu.transRGBEnabled());
	EXPECT_EQ(ppu.transRGB(), 0xF81Fu);
}

// --- Per-text-layer registers -----------------------------------------------
TEST(PPURegs, TextLayerAddressesAreDistinct)
{
	PPU ppu;
	for (int l = 0; l < 3; l++)
	{
		ppu.write32(PPU::TEXT_BASE + l * PPU::TEXT_STRIDE + PPU::TX_ATTR, 0x1000 + l);
		ppu.write32(PPU::TEXT_BASE + l * PPU::TEXT_STRIDE + PPU::TX_NPTR, 0xAABB0000u + l);
	}
	for (int l = 0; l < 3; l++)
	{
		EXPECT_EQ(ppu.textAttr(l), (uint32_t)(0x1000 + l));
		EXPECT_EQ(ppu.textNPtr(l), 0xAABB0000u + l);
	}
}

TEST(PPURegs, TextPositionSignExtended)
{
	PPU ppu;
	ppu.write32(PPU::TEXT_BASE + PPU::TX_X, 0xFFFF); // -1
	ppu.write32(PPU::TEXT_BASE + PPU::TX_Y, 0xFE00); // -512
	EXPECT_EQ(ppu.textX(0), -1);
	EXPECT_EQ(ppu.textY(0), -512);

	ppu.write32(PPU::TEXT_BASE + PPU::TX_X, 0x01FF); // +511
	EXPECT_EQ(ppu.textX(0), 511);
}

TEST(PPURegs, TextControlFlagsAndBlendLevel)
{
	PPU ppu;
	ppu.write32(PPU::TEXT_BASE + PPU::TX_CTRL, PPU::TXC_TXE | PPU::TXC_BLEND);
	EXPECT_TRUE(ppu.textEnabled(0));

	ppu.write32(PPU::TEXT_BASE + PPU::TX_BLEND, 0x3F);
	EXPECT_EQ(ppu.textBlendLevel(0), 0x3Fu);
}

TEST(PPURegs, BufferStartAddresses)
{
	PPU ppu;
	ppu.write32(PPU::BUF_SA_BASE + 0 * 0x0C + 0, 0x10000000); // tx0 buf0
	ppu.write32(PPU::BUF_SA_BASE + 1 * 0x0C + 4, 0x20000000); // tx1 buf1
	ppu.write32(PPU::P_Frame_BUF_SA0, 0x30000000);
	ppu.write32(PPU::P_SP_BUF_SA, 0x40000000);
	EXPECT_EQ(ppu.textBufSA(0, 0), 0x10000000u);
	EXPECT_EQ(ppu.textBufSA(1, 1), 0x20000000u);
	EXPECT_EQ(ppu.frameBufSA(0), 0x30000000u);
	EXPECT_EQ(ppu.spriteBufSA(), 0x40000000u);
}

// --- Attribute decode -------------------------------------------------------
TEST(PPURegs, DecodeTextAttribute)
{
	// Color=BPP8(3), Flip=H(1), Hs=16(1), Vs=32(2), Palette=5, Depth=2.
	uint32_t a = (3u << 0) | (1u << 2) | (1u << 4) | (2u << 6) | (5u << 8) | (2u << 13);
	PPU::TextAttr t = PPU::decodeAttr(a);
	EXPECT_EQ(t.color, ColorMode::BPP8);
	EXPECT_EQ(t.flip, Flip::H);
	EXPECT_EQ(t.hsizeCode, 1);
	EXPECT_EQ(t.vsizeCode, 2);
	EXPECT_EQ(t.palette, 5);
	EXPECT_EQ(t.depth, 2);
}

// --- Sprite decode ----------------------------------------------------------
TEST(PPURegs, DecodeSpriteTopLeftCoords)
{
	PPU ppu;
	ppu.write32(PPU::P_SP_Control, PPU::COORD_SEL_BIT); // top-left origin

	// Sprite 0: charNum=7, X=100; attribute (color BPP4, blend), Y=50, level=20.
	uint32_t numX = 7u | (100u << 16);
	uint32_t attr = (1u << 0) | PPU::ATTR_BLEND_BIT; // BPP4 + blend
	uint32_t attY = attr | (50u << 16) | (20u << 26);
	ppu.write32(PPU::SPRITE_BASE + 0, numX);
	ppu.write32(PPU::SPRITE_BASE + 4, attY);

	PPU::SpriteAttr s = ppu.decodeSprite(0);
	EXPECT_EQ(s.charNum, 7);
	EXPECT_EQ(s.x, 100);
	EXPECT_EQ(s.y, 50);
	EXPECT_EQ(s.color, ColorMode::BPP4);
	EXPECT_TRUE(s.blend);
	EXPECT_EQ(s.blendLevel, 20);
}

TEST(PPURegs, DecodeSpriteCenterCoordsSignExtended)
{
	PPU ppu;
	// Coord_sel = 0 -> centre origin, signed 10-bit coordinates.
	uint32_t xNeg = (uint32_t)(0x3FF) << 16; // 10-bit all-ones = -1
	ppu.write32(PPU::SPRITE_BASE + 0, 1u | xNeg);
	uint32_t yNeg = (uint32_t)(0x200) << 16; // 0x200 = -512 in 10-bit
	ppu.write32(PPU::SPRITE_BASE + 4, yNeg);

	PPU::SpriteAttr s = ppu.decodeSprite(0);
	EXPECT_EQ(s.x, -1);
	EXPECT_EQ(s.y, -512);
}
