// ============================================================================
// PPU renderer integration tests
//
// Drives the full sprite + text compositor through a self-contained flat DRAM
// model (FlatMemory). Each test builds a tiny scene - palette, character data,
// sprite/text registers - and asserts the composited RGB output. Exercises:
// sprite rendering, palette lookup, transparency (palette Tr + colour key +
// transparent character), character flip, depth priority, alpha blending,
// text character mode, and high-colour bitmap mode.
// ============================================================================
#include <gtest/gtest.h>
#include "ppu.h"
#include "ppu_image.h"

using Color = PPU::Color;

// QVGA dimensions used by every scene in this file.
static constexpr int SCENE_W = 320;
static constexpr int SCENE_H = 240;

namespace
{
constexpr uint16_t RED555   = 0x1F << 10; // R=31
constexpr uint16_t GREEN555 = 0x1F << 5;  // G=31
constexpr uint16_t BLUE555  = 0x1F;       // B=31

const Color RED{255, 0, 0};
const Color GREEN{0, 255, 0};
const Color BLUE{0, 0, 255};
const Color BACKDROP{1, 2, 3};

// Fill a contiguous run of bytes with a single value (a solid character of a
// 4-bpp index where the value packs two identical nibbles).
void fillBytes(FlatMemory &mem, uint32_t addr, uint32_t len, uint8_t v)
{
	for (uint32_t i = 0; i < len; i++) mem.writeByte(addr + i, v);
}

// Configure a basic QVGA scene with sprites enabled and top-left coordinates.
PPU makeSpriteScene()
{
	PPU ppu;
	ppu.write32(PPU::P_PPU_Control, PPU::PPUEN_BIT | PPU::RES_QVGA);
	ppu.write32(PPU::P_SP_Control, PPU::SP_EN_BIT | PPU::COORD_SEL_BIT);
	ppu.write32(PPU::P_SP_Max, 0);
	ppu.write32(PPU::P_SP_BUF_SA, 0x1000);
	return ppu;
}

// Helper to index the rendered frame buffer.
Color at(const std::vector<Color> &fb, int x, int y, int w = 320)
{
	return fb[(size_t)y * w + x];
}

// 8x8 bitmap glyphs (subset of the public-domain font8x8_basic). Each byte is
// one row, top to bottom; the least-significant bit is the leftmost column.
const uint8_t FONT_H[8] = {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00};
const uint8_t FONT_e[8] = {0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00};
const uint8_t FONT_l[8] = {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00};
const uint8_t FONT_o[8] = {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00};
const uint8_t FONT_W[8] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00};
const uint8_t FONT_r[8] = {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00};
const uint8_t FONT_d[8] = {0x38, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x6E, 0x00};

// Write a single 4-bpp palette index into packed character data (LSB-first,
// matching PPU::unpackIndex).
void putPixel4(FlatMemory &mem, uint32_t charBase, int cw, int x, int y, uint8_t idx)
{
	uint32_t bitPos = (uint32_t)(y * cw + x) * 4;
	uint32_t addr = charBase + bitPos / 8;
	uint8_t shift = bitPos & 7;          // 0 or 4
	uint8_t b = mem.readByte(addr);
	b = (uint8_t)((b & ~(0xF << shift)) | ((idx & 0xF) << shift));
	mem.writeByte(addr, b);
}

// Render an 8x8 glyph scaled by `scale` into a (cw x cw) 4-bpp character.
// Lit pixels become index 1, background index 0.
void drawGlyph(FlatMemory &mem, uint32_t charBase, int cw, int scale, const uint8_t glyph[8])
{
	for (int cy = 0; cy < cw; cy++)
		for (int cx = 0; cx < cw; cx++)
		{
			int sx = cx / scale;
			int sy = cy / scale;
			uint8_t lit = (sx < 8 && sy < 8) ? ((glyph[sy] >> sx) & 1) : 0;
			putPixel4(mem, charBase, cw, cx, cy, lit ? 1 : 0);
		}
}
} // namespace

// --- A single solid sprite --------------------------------------------------
TEST(PPURender, SingleSpriteDrawsRedBlock)
{
	PPU ppu = makeSpriteScene();
	ppu.setPaletteRaw(1, RED555); // bank0, BPP4, index1 -> red

	FlatMemory mem(0, 0x4000);
	// 8x8 BPP4 character #1 -> 32 bytes, all nibbles = index 1.
	fillBytes(mem, 0x1000 + 1 * 32, 32, 0x11);

	// Sprite 0: charNum=1, X=5, Y=7, color BPP4, depth0.
	ppu.write32(PPU::SPRITE_BASE + 0, 1u | (5u << 16));
	ppu.write32(PPU::SPRITE_BASE + 4, (1u << 0) | (7u << 16));

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_single_sprite", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_single_sprite", fb, SCENE_W, SCENE_H);

	EXPECT_EQ(at(fb, 5, 7), RED);
	EXPECT_EQ(at(fb, 12, 14), RED);   // bottom-right of the 8x8 block
	EXPECT_EQ(at(fb, 13, 7), BACKDROP); // just outside
	EXPECT_EQ(at(fb, 0, 0), BACKDROP);
}

// --- PPU disabled -> nothing is drawn ---------------------------------------
TEST(PPURender, DisabledPPUProducesBackdrop)
{
	PPU ppu = makeSpriteScene();
	ppu.write32(PPU::P_PPU_Control, PPU::RES_QVGA); // PPUEN clear
	ppu.setPaletteRaw(1, RED555);

	FlatMemory mem(0, 0x4000);
	fillBytes(mem, 0x1000 + 1 * 32, 32, 0x11);
	ppu.write32(PPU::SPRITE_BASE + 0, 1u | (5u << 16));
	ppu.write32(PPU::SPRITE_BASE + 4, (1u << 0) | (7u << 16));

	// A clearly visible backdrop shows that a disabled PPU emits a uniform
	// fill: the red sprite must NOT appear anywhere.
	const Color OFF_SCREEN{0, 90, 140};
	auto fb = ppu.renderFrame(mem, OFF_SCREEN);
	ppu_artifacts::dump("render_disabled", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_disabled", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(fb.size(), 320u * 240u);
	for (const Color &c : fb) EXPECT_EQ(c, OFF_SCREEN);
}

// --- Transparent character (charNum 0) is skipped entirely ------------------
TEST(PPURender, CharNumZeroSpriteSkipped)
{
	PPU ppu = makeSpriteScene();
	ppu.write32(PPU::P_SP_Max, 1); // enable sprites 0 and 1
	ppu.setPaletteRaw(1, RED555);
	FlatMemory mem(0, 0x4000);
	fillBytes(mem, 0x1000 + 1 * 32, 32, 0x11); // char #1 = solid red

	// Sprite 0 references character 0 -> the transparent character, skipped.
	ppu.write32(PPU::SPRITE_BASE + 0 * 8 + 0, 0u | (5u << 16));
	ppu.write32(PPU::SPRITE_BASE + 0 * 8 + 4, (1u << 0) | (7u << 16));
	// Sprite 1 references character 1 -> a visible red block as a control.
	ppu.write32(PPU::SPRITE_BASE + 1 * 8 + 0, 1u | (20u << 16));
	ppu.write32(PPU::SPRITE_BASE + 1 * 8 + 4, (1u << 0) | (7u << 16));

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_charnum_zero", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_charnum_zero", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(at(fb, 5, 7), BACKDROP); // char 0 sprite skipped
	EXPECT_EQ(at(fb, 20, 7), RED);     // visible control sprite
}

// --- Palette Tr bit makes pixels transparent --------------------------------
TEST(PPURender, PaletteTransparentIndexShowsBackdrop)
{
	PPU ppu = makeSpriteScene();
	// Character #2: left half palette index 0 (marked transparent), right
	// half index 1 (opaque red) so the transparency is visible.
	ppu.setPaletteRaw(0, 0x8000); // Tr set -> transparent
	ppu.setPaletteRaw(1, RED555);

	FlatMemory mem(0, 0x4000);
	uint32_t cbase = 0x1000 + 2 * 32;
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++)
			putPixel4(mem, cbase, 8, x, y, x < 4 ? 0 : 1);
	ppu.write32(PPU::SPRITE_BASE + 0, 2u | (5u << 16));
	ppu.write32(PPU::SPRITE_BASE + 4, (1u << 0) | (7u << 16));

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_palette_transparent", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_palette_transparent", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(at(fb, 5, 7), BACKDROP); // transparent half
	EXPECT_EQ(at(fb, 11, 7), RED);     // opaque half
}

TEST(PPURender, OpaqueIndexZeroDrawsColor)
{
	PPU ppu = makeSpriteScene();
	ppu.setPaletteRaw(0, BLUE555); // Tr clear -> opaque blue

	FlatMemory mem(0, 0x4000);
	ppu.write32(PPU::SPRITE_BASE + 0, 2u | (5u << 16));
	ppu.write32(PPU::SPRITE_BASE + 4, (1u << 0) | (7u << 16));

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_opaque_index0", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_opaque_index0", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(at(fb, 5, 7), BLUE);
}

// --- Colour-key transparency (P_Trans_RGB) ----------------------------------
TEST(PPURender, ColorKeyMakesMatchingPixelTransparent)
{
	PPU ppu = makeSpriteScene();
	// Character #1: left half green (matches the colour key -> removed),
	// right half blue (kept), so the effect is clearly visible.
	ppu.setPaletteRaw(1, GREEN555);
	ppu.setPaletteRaw(2, BLUE555);
	ppu.write32(PPU::P_Trans_RGB, PPU::TRANS_RGB_EN_BIT | 0x07E0); // key = green565

	FlatMemory mem(0, 0x4000);
	uint32_t cbase = 0x1000 + 1 * 32;
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++)
			putPixel4(mem, cbase, 8, x, y, x < 4 ? 1 : 2);
	ppu.write32(PPU::SPRITE_BASE + 0, 1u | (5u << 16));
	ppu.write32(PPU::SPRITE_BASE + 4, (1u << 0) | (7u << 16));

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_color_key", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_color_key", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(at(fb, 5, 7), BACKDROP); // green half keyed out
	EXPECT_EQ(at(fb, 11, 7), BLUE);    // blue half kept
}

// --- Character horizontal flip ----------------------------------------------
TEST(PPURender, HorizontalFlipMirrorsCharacter)
{
	PPU ppu = makeSpriteScene();
	ppu.setPaletteRaw(0, 0x8000); // index0 transparent
	ppu.setPaletteRaw(1, RED555); // index1 red

	FlatMemory mem(0, 0x4000);
	// char #1: only column 0 of each row is index1, rest index0.
	for (int row = 0; row < 8; row++)
		mem.writeByte(0x1000 + 1 * 32 + row * 4, 0x01);

	// Unflipped sprite 0 at (0,0).
	ppu.write32(PPU::SPRITE_BASE + 0, 1u | (0u << 16));
	ppu.write32(PPU::SPRITE_BASE + 4, (1u << 0) | (0u << 16));

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_flip_none", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_flip_none", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(at(fb, 0, 0), RED);
	EXPECT_EQ(at(fb, 7, 0), BACKDROP);

	// Horizontally flipped: column 0 mirrors to column 7.
	uint32_t attrFlipH = (1u << 0) | ((uint32_t)PPU::Flip::H << PPU::ATTR_FLIP_SHIFT);
	ppu.write32(PPU::SPRITE_BASE + 4, attrFlipH | (0u << 16));
	fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_flip_horizontal", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_flip_horizontal", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(at(fb, 0, 0), BACKDROP);
	EXPECT_EQ(at(fb, 7, 0), RED);
}

// --- Depth priority between two overlapping sprites -------------------------
TEST(PPURender, HigherDepthSpriteWins)
{
	PPU ppu = makeSpriteScene();
	ppu.write32(PPU::P_SP_Max, 1); // sprites 0 and 1
	ppu.setPaletteRaw(1, RED555);          // bank0 idx1 -> red
	ppu.setPaletteRaw(16 + 1, GREEN555);   // bank1 idx1 -> green

	FlatMemory mem(0, 0x4000);
	fillBytes(mem, 0x1000 + 1 * 32, 32, 0x11); // char #1 solid index1

	// Sprite0: red (bank0), depth 0, at (10,10).
	ppu.write32(PPU::SPRITE_BASE + 0 * 8 + 0, 1u | (10u << 16));
	ppu.write32(PPU::SPRITE_BASE + 0 * 8 + 4, (1u << 0) | (0u << PPU::ATTR_DEPTH_SHIFT) | (10u << 16));
	// Sprite1: green (bank1), depth 1, at (10,10).
	ppu.write32(PPU::SPRITE_BASE + 1 * 8 + 0, 1u | (10u << 16));
	ppu.write32(PPU::SPRITE_BASE + 1 * 8 + 4,
	            (1u << 0) | (1u << PPU::ATTR_PALETTE_SHIFT) | (1u << PPU::ATTR_DEPTH_SHIFT) | (10u << 16));

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_depth_green_top", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_depth_green_top", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(at(fb, 10, 10), GREEN); // depth1 sprite on top

	// Swap depths: now sprite0 (red) should win.
	ppu.write32(PPU::SPRITE_BASE + 0 * 8 + 4, (1u << 0) | (1u << PPU::ATTR_DEPTH_SHIFT) | (10u << 16));
	ppu.write32(PPU::SPRITE_BASE + 1 * 8 + 4,
	            (1u << 0) | (1u << PPU::ATTR_PALETTE_SHIFT) | (0u << PPU::ATTR_DEPTH_SHIFT) | (10u << 16));
	fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_depth_red_top", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_depth_red_top", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(at(fb, 10, 10), RED);
}

// --- Alpha blending of a sprite over another --------------------------------
TEST(PPURender, BlendSpriteOverSprite)
{
	PPU ppu = makeSpriteScene();
	ppu.write32(PPU::P_SP_Max, 1);
	ppu.setPaletteRaw(1, RED555);
	ppu.setPaletteRaw(16 + 1, GREEN555);

	FlatMemory mem(0, 0x4000);
	fillBytes(mem, 0x1000 + 1 * 32, 32, 0x11);

	// Sprite0: red, depth0 (bottom), opaque.
	ppu.write32(PPU::SPRITE_BASE + 0 * 8 + 0, 1u | (10u << 16));
	ppu.write32(PPU::SPRITE_BASE + 0 * 8 + 4, (1u << 0) | (10u << 16));
	// Sprite1: green, depth1 (top), blend enabled, level=32, over red.
	uint32_t attr = (1u << 0) | (1u << PPU::ATTR_PALETTE_SHIFT)
	              | (1u << PPU::ATTR_DEPTH_SHIFT) | PPU::ATTR_BLEND_BIT;
	ppu.write32(PPU::SPRITE_BASE + 1 * 8 + 0, 1u | (10u << 16));
	ppu.write32(PPU::SPRITE_BASE + 1 * 8 + 4, attr | (10u << 16) | (32u << 26));

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_blend", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_blend", fb, SCENE_W, SCENE_H);
	// blend(green over red, level 32) -> (127,127,0)
	Color expect = PPU::blendColor(GREEN, RED, 32, false);
	EXPECT_EQ(at(fb, 10, 10), expect);
	EXPECT_EQ(expect.r, 127);
	EXPECT_EQ(expect.g, 127);
	EXPECT_EQ(expect.b, 0);
}

// --- Text layer, character mode ---------------------------------------------
TEST(PPURender, TextCharacterModeDrawsCharacter)
{
	PPU ppu;
	ppu.write32(PPU::P_PPU_Control, PPU::PPUEN_BIT | PPU::RES_QVGA);
	ppu.setPaletteRaw(1, RED555);

	// Text layer 0: enabled, character mode (Linr=0), 8x8 BPP4, palette0.
	ppu.write32(PPU::TEXT_BASE + PPU::TX_CTRL, PPU::TXC_TXE);
	ppu.write32(PPU::TEXT_BASE + PPU::TX_ATTR, (1u << 0)); // BPP4, size 8x8
	ppu.write32(PPU::TEXT_BASE + PPU::TX_NPTR, 0x2000);    // number array
	ppu.write32(PPU::BUF_SA_BASE + 0, 0x3000);            // char data base

	FlatMemory mem(0, 0x6000);
	mem.writeHalf(0x2000 + 0, 1); // tile (0,0) -> character #1
	fillBytes(mem, 0x3000 + 1 * 32, 32, 0x11); // char #1 solid red index

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_text_character_mode", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_text_character_mode", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(at(fb, 0, 0), RED);
	EXPECT_EQ(at(fb, 7, 7), RED);
	// tile (1,0) has character number 0 -> transparent.
	EXPECT_EQ(at(fb, 8, 0), BACKDROP);
}

// --- Text layer, high-colour bitmap mode (RGB565) ---------------------------
TEST(PPURender, TextBitmapRGB565)
{
	PPU ppu;
	ppu.write32(PPU::P_PPU_Control, PPU::PPUEN_BIT | PPU::RES_QVGA);

	ppu.write32(PPU::TEXT_BASE + PPU::TX_CTRL, PPU::TXC_TXE | PPU::TXC_LINR | PPU::TXC_RGB565);
	ppu.write32(PPU::BUF_SA_BASE + 0, 0x3000); // bitmap base

	FlatMemory mem(0, 0x40000);
	int virtW = 512;
	mem.writeHalf(0x3000 + (0 * virtW + 0) * 2, 0x07E0); // green565 at (0,0)
	mem.writeHalf(0x3000 + (0 * virtW + 1) * 2, 0xF800); // red565 at (1,0)

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_text_bitmap_rgb565", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_text_bitmap_rgb565", fb, SCENE_W, SCENE_H, 0, 0, 16, 12, 40);
	EXPECT_EQ(at(fb, 0, 0), GREEN);
	EXPECT_EQ(at(fb, 1, 0), RED);
}

// --- Text scrolling wraps around the virtual screen -------------------------
TEST(PPURender, TextHorizontalPositionShiftsContent)
{
	PPU ppu;
	ppu.write32(PPU::P_PPU_Control, PPU::PPUEN_BIT | PPU::RES_QVGA);
	ppu.setPaletteRaw(1, RED555);

	ppu.write32(PPU::TEXT_BASE + PPU::TX_CTRL, PPU::TXC_TXE);
	ppu.write32(PPU::TEXT_BASE + PPU::TX_ATTR, (1u << 0));
	ppu.write32(PPU::TEXT_BASE + PPU::TX_NPTR, 0x2000);
	ppu.write32(PPU::BUF_SA_BASE + 0, 0x3000);
	// Scroll the visible window right by 8 px so virtual column 8..15 shows
	// at screen column 0..7.
	ppu.write32(PPU::TEXT_BASE + PPU::TX_X, 8);

	FlatMemory mem(0, 0x8000);
	mem.writeHalf(0x2000 + 1 * 2, 1); // tile index 1 (virtual col 8) -> char #1
	fillBytes(mem, 0x3000 + 1 * 32, 32, 0x11);

	auto fb = ppu.renderFrame(mem, BACKDROP);
	ppu_artifacts::dump("render_text_scroll", fb, SCENE_W, SCENE_H);
	ppu_artifacts::dumpGiant("giant_text_scroll", fb, SCENE_W, SCENE_H);
	EXPECT_EQ(at(fb, 0, 0), RED);       // scrolled tile now at screen origin
	EXPECT_EQ(at(fb, 8, 0), BACKDROP);  // tile (0,0)/(2,0) empty
}

// --- "Hello World" in large text through the PPU text layer -----------------
TEST(PPURender, HelloWorldLargeText)
{
	const int W = 640, H = 480;     // VGA
	const int CELL = 32;            // 32x32 character cells
	const int SCALE = CELL / 8;     // 8x8 font scaled 4x
	const Color WHITE{255, 255, 255};
	const Color BG{0, 0, 40};       // dark blue backdrop

	PPU ppu;
	ppu.write32(PPU::P_PPU_Control, PPU::PPUEN_BIT | PPU::RES_VGA);

	// Text layer 0: enabled, character mode, BPP4, 32x32 characters, palette 0.
	uint32_t attr = (1u << 0)
	              | (2u << PPU::ATTR_HSIZE_SHIFT)  // Hs=2 -> 32 px
	              | (2u << PPU::ATTR_VSIZE_SHIFT); // Vs=2 -> 32 px
	ppu.write32(PPU::TEXT_BASE + PPU::TX_CTRL, PPU::TXC_TXE);
	ppu.write32(PPU::TEXT_BASE + PPU::TX_ATTR, attr);
	ppu.write32(PPU::TEXT_BASE + PPU::TX_NPTR, 0x2000); // number array
	ppu.write32(PPU::BUF_SA_BASE + 0, 0x3000);          // character data

	// Palette: index 0 transparent (glyph background), index 1 white (text).
	ppu.setPaletteRaw(0, 0x8000);
	ppu.setPaletteRaw(1, 0x7FFF);

	FlatMemory mem(0, 0x20000);

	// Build the character set. charBytes for a 32x32 BPP4 glyph = 512.
	const uint32_t charBytes = (uint32_t)CELL * CELL * 4 / 8;
	const uint32_t charBase = 0x3000;
	struct Glyph { uint16_t num; const uint8_t *bits; };
	const Glyph glyphs[] = {
		{1, FONT_H}, {2, FONT_e}, {3, FONT_l}, {4, FONT_o},
		{5, FONT_W}, {6, FONT_r}, {7, FONT_d},
	};
	for (const Glyph &g : glyphs)
		drawGlyph(mem, charBase + g.num * charBytes, CELL, SCALE, g.bits);

	// "Hello World" -> per-tile character numbers (0 = blank/transparent space).
	const uint16_t text[] = {1, 2, 3, 3, 4, 0, 5, 4, 6, 3, 7};
	for (size_t i = 0; i < sizeof(text) / sizeof(text[0]); i++)
		mem.writeHalf(0x2000 + (uint32_t)i * 2, text[i]);

	auto fb = ppu.renderFrame(mem, BG);
	ppu_artifacts::dump("render_hello_world", fb, W, H);

	EXPECT_EQ(fb.size(), (size_t)W * H);
	// 'H' (tile 0): top-left column is lit -> white text pixel.
	EXPECT_EQ(at(fb, 2, 2, W), WHITE);
	// Gap between the H's two bars is transparent -> shows backdrop.
	EXPECT_EQ(at(fb, 9, 2, W), BG);
	// Tile 5 is the space -> entirely backdrop.
	EXPECT_EQ(at(fb, 5 * CELL + 16, 16, W), BG);
	// Empty region well past the text.
	EXPECT_EQ(at(fb, 600, 400, W), BG);
}
