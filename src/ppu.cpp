#include "ppu.h"

#include <cmath>

// ============================================================================
// Construction / reset
// ============================================================================
PPU::PPU()
{
	reset();
}

void PPU::reset()
{
	m_ctrl.fill(0);
	m_palette.fill(0);
	m_hvoffset.fill(0);
	m_hcomp.fill(0);
	m_spriteNumX.fill(0);
	m_spriteAttY.fill(0);
}

// ============================================================================
// Register file
// ============================================================================
uint32_t PPU::read32(uint32_t addr) const
{
	uint32_t off = offsetOf(addr);

	if (off < 0x1000)
		return m_ctrl[(off >> 2) & 0x3F];
	if (off >= PALETTE_BASE && off < PALETTE_BASE + 0x800)
		return m_palette[(off - PALETTE_BASE) >> 2];
	if (off >= HVOFFSET_BASE && off < HVOFFSET_BASE + 0x800)
		return m_hvoffset[(off - HVOFFSET_BASE) >> 2];
	if (off >= HCOMP_BASE && off < HCOMP_BASE + 0x800)
		return m_hcomp[(off - HCOMP_BASE) >> 2];
	if (off >= SPRITE_BASE && off < SPRITE_BASE + 0x1000)
	{
		uint32_t rel = off - SPRITE_BASE;
		uint32_t idx = rel >> 3;
		return (rel & 0x4) ? m_spriteAttY[idx] : m_spriteNumX[idx];
	}
	return 0;
}

void PPU::write32(uint32_t addr, uint32_t data)
{
	uint32_t off = offsetOf(addr);

	if (off < 0x1000)
		m_ctrl[(off >> 2) & 0x3F] = data;
	else if (off >= PALETTE_BASE && off < PALETTE_BASE + 0x800)
		m_palette[(off - PALETTE_BASE) >> 2] = data;
	else if (off >= HVOFFSET_BASE && off < HVOFFSET_BASE + 0x800)
		m_hvoffset[(off - HVOFFSET_BASE) >> 2] = data;
	else if (off >= HCOMP_BASE && off < HCOMP_BASE + 0x800)
		m_hcomp[(off - HCOMP_BASE) >> 2] = data;
	else if (off >= SPRITE_BASE && off < SPRITE_BASE + 0x1000)
	{
		uint32_t rel = off - SPRITE_BASE;
		uint32_t idx = rel >> 3;
		if (rel & 0x4) m_spriteAttY[idx] = data;
		else           m_spriteNumX[idx] = data;
	}
}

void PPU::resolutionSize(Resolution r, int &w, int &h)
{
	switch (r)
	{
	case RES_VGA:  w = 640; h = 480; break;
	case RES_HVGA: w = 640; h = 240; break;
	case RES_QVGA:
	case RES_CIF:
	default:       w = 320; h = 240; break;
	}
}

// ============================================================================
// Attribute decode
// ============================================================================
PPU::TextAttr PPU::decodeAttr(uint32_t a)
{
	TextAttr t;
	t.color     = (ColorMode)((a >> ATTR_COLOR_SHIFT) & ATTR_COLOR_MASK);
	t.flip      = (Flip)((a >> ATTR_FLIP_SHIFT) & ATTR_FLIP_MASK);
	t.hsizeCode = (uint8_t)((a >> ATTR_HSIZE_SHIFT) & ATTR_HSIZE_MASK);
	t.vsizeCode = (uint8_t)((a >> ATTR_VSIZE_SHIFT) & ATTR_VSIZE_MASK);
	t.palette   = (uint8_t)((a >> ATTR_PALETTE_SHIFT) & ATTR_PALETTE_MASK);
	t.depth     = (uint8_t)((a >> ATTR_DEPTH_SHIFT) & ATTR_DEPTH_MASK);
	return t;
}

PPU::SpriteAttr PPU::decodeSprite(int index) const
{
	index &= 0x1FF;
	uint32_t numX = m_spriteNumX[index];
	uint32_t attY = m_spriteAttY[index];

	SpriteAttr s;
	s.charNum = (uint16_t)(numX & 0xFFFF);

	uint32_t x10 = (numX >> 16) & 0x3FF;
	uint32_t y10 = (attY >> 16) & 0x3FF;
	if (coordTopLeft())
	{
		s.x = (int32_t)x10;
		s.y = (int32_t)y10;
	}
	else
	{
		s.x = signExtend(x10, 10);
		s.y = signExtend(y10, 10);
	}
	s.blendLevel = (uint8_t)((attY >> 26) & 0x3F);

	uint32_t a = attY & 0xFFFF;
	s.color     = (ColorMode)((a >> ATTR_COLOR_SHIFT) & ATTR_COLOR_MASK);
	s.flip      = (Flip)((a >> ATTR_FLIP_SHIFT) & ATTR_FLIP_MASK);
	s.hsizeCode = (uint8_t)((a >> ATTR_HSIZE_SHIFT) & ATTR_HSIZE_MASK);
	s.vsizeCode = (uint8_t)((a >> ATTR_VSIZE_SHIFT) & ATTR_VSIZE_MASK);
	s.palette   = (uint8_t)((a >> ATTR_PALETTE_SHIFT) & ATTR_PALETTE_MASK);
	s.depth     = (uint8_t)((a >> ATTR_DEPTH_SHIFT) & ATTR_DEPTH_MASK);
	s.blend     = (a & ATTR_BLEND_BIT) != 0;
	return s;
}

// ============================================================================
// Pure helpers
// ============================================================================
int PPU::bitsPerPixel(ColorMode m)
{
	switch (m)
	{
	case ColorMode::BPP2: return 2;
	case ColorMode::BPP4: return 4;
	case ColorMode::BPP6: return 6;
	case ColorMode::BPP8: return 8;
	}
	return 4;
}

int PPU::colorCount(ColorMode m)
{
	switch (m)
	{
	case ColorMode::BPP2: return 4;
	case ColorMode::BPP4: return 16;
	case ColorMode::BPP6: return 64;
	case ColorMode::BPP8: return 256;
	}
	return 16;
}

int PPU::sizeFromCode(uint8_t code)
{
	return 8 << (code & 0x3); // 8,16,32,64
}

PPU::Pixel PPU::decodeRGB555(uint16_t v)
{
	Pixel p;
	p.transparent = (v & 0x8000) != 0;
	p.color.r = expand5((v >> 10) & 0x1F);
	p.color.g = expand5((v >> 5) & 0x1F);
	p.color.b = expand5(v & 0x1F);
	return p;
}

PPU::Color PPU::decodeRGB565(uint16_t v)
{
	Color c;
	c.r = expand5((v >> 11) & 0x1F);
	c.g = expand6((v >> 5) & 0x3F);
	c.b = expand5(v & 0x1F);
	return c;
}

PPU::Pixel PPU::decodePaletteEntry(uint16_t entry)
{
	return decodeRGB555(entry);
}

uint32_t PPU::paletteEntryIndex(uint8_t bank, uint32_t colorIndex, ColorMode mode)
{
	return (uint32_t)bank * colorCount(mode) + colorIndex;
}

uint32_t PPU::palettePhysAddr(uint8_t bank, uint32_t colorIndex, ColorMode mode)
{
	return BASE + PALETTE_BASE + paletteEntryIndex(bank, colorIndex, mode) * 4;
}

uint32_t PPU::unpackIndex(const uint8_t *data, size_t dataLen, uint32_t pixelIndex, ColorMode mode)
{
	int bpp = bitsPerPixel(mode);
	uint64_t bitPos = (uint64_t)pixelIndex * bpp;
	uint32_t result = 0;
	for (int i = 0; i < bpp; i++)
	{
		uint64_t bp = bitPos + i;
		size_t byteIdx = (size_t)(bp >> 3);
		uint8_t bit = (byteIdx < dataLen) ? ((data[byteIdx] >> (bp & 7)) & 1) : 0;
		result |= (uint32_t)bit << i;
	}
	return result;
}

uint8_t PPU::blendChannel(uint8_t top, uint8_t bottom, uint8_t level, bool sub)
{
	level &= 0x3F;
	int inv = 64 - (int)level;
	int num = sub ? ((int)top * level - (int)bottom * inv)
	              : ((int)top * level + (int)bottom * inv);
	int c = num / 64;
	if (c < 0) c = 0;
	if (c > 255) c = 255;
	return (uint8_t)c;
}

PPU::Color PPU::blendColor(Color top, Color bottom, uint8_t level, bool sub)
{
	Color c;
	c.r = blendChannel(top.r, bottom.r, level, sub);
	c.g = blendChannel(top.g, bottom.g, level, sub);
	c.b = blendChannel(top.b, bottom.b, level, sub);
	return c;
}

double PPU::verticalSourceStep(uint32_t vcmpValue)
{
	return (double)vcmpValue / (double)VCMP_UNIT;
}

double PPU::horizontalSourceStep(uint32_t hcmpValue)
{
	return (double)hcmpValue / (double)HCMP_UNIT;
}

void PPU::applyFlip(Flip flip, uint32_t &x, uint32_t &y, uint32_t w, uint32_t h)
{
	bool hf = (flip == Flip::H || flip == Flip::HV);
	bool vf = (flip == Flip::V || flip == Flip::HV);
	if (hf && w) x = w - 1 - x;
	if (vf && h) y = h - 1 - y;
}

bool PPU::isAbove(const LayerRef &a, const LayerRef &b)
{
	if (a.depth != b.depth) return a.depth > b.depth;
	if (a.isSprite != b.isSprite) return a.isSprite; // sprite over text at equal depth
	return a.subIndex > b.subIndex;
}

// ============================================================================
// Renderer
// ============================================================================
void PPU::applyColorKey(Pixel &p) const
{
	if (p.transparent || !transRGBEnabled()) return;
	Color key = decodeRGB565(transRGB());
	if (p.color == key) p.transparent = true;
}

PPU::Pixel PPU::fetchCharPixel(const PPUMemory &mem, uint32_t charBaseAddr,
                               uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch,
                               ColorMode color, uint8_t palette, Flip flip) const
{
	applyFlip(flip, cx, cy, cw, ch);

	uint32_t pixelIndex = cy * cw + cx;
	int bpp = bitsPerPixel(color);
	uint64_t bitPos = (uint64_t)pixelIndex * bpp;

	uint32_t idx = 0;
	for (int i = 0; i < bpp; i++)
	{
		uint64_t bp = bitPos + i;
		uint32_t byteAddr = charBaseAddr + (uint32_t)(bp >> 3);
		uint8_t bit = (mem.readByte(byteAddr) >> (bp & 7)) & 1;
		idx |= (uint32_t)bit << i;
	}

	uint32_t entryIdx = paletteEntryIndex(palette, idx, color);
	Pixel p = decodePaletteEntry(paletteRaw(entryIdx));
	applyColorKey(p);
	return p;
}

// Internal fragment for per-pixel compositing.
namespace
{
	struct Fragment
	{
		PPU::LayerRef ref;
		PPU::Color    color;
		bool          blend = false;
		uint8_t       level = 0;
	};
}

PPU::Color PPU::samplePixel(const PPUMemory &mem, int px, int py, Color backdrop) const
{
	int sw, sh;
	screenSize(sw, sh);

	int virtW = (resolution() == RES_VGA) ? 1024 : 512;
	int virtH = (resolution() == RES_VGA) ? 512 : 256;

	std::vector<Fragment> frags;

	// ---- Text layers ------------------------------------------------
	for (int layer = 0; layer < 3; layer++)
	{
		uint32_t ctrl = textCtrl(layer);
		if (!(ctrl & TXC_TXE)) continue;

		TextAttr attr = decodeAttr(textAttr(layer));

		// Source coordinate in the (wrapping) virtual screen.
		double sx = (double)px + (double)textX(layer);
		double sy = (double)py + (double)textY(layer);

		// Per-line horizontal movement.
		if (ctrl & TXC_MVE)
			sx += (double)hvOffset((uint32_t)((py % virtH + virtH) % virtH));

		// Vertical compression / extension (whole layer).
		if (ctrl & TXC_VCMP)
		{
			uint32_t vval = read32(P_VComp_Value) ? read32(P_VComp_Value) : VCMP_UNIT;
			sy = (double)read32(P_VComp_Offset) + (double)py * verticalSourceStep(vval);
		}
		// Horizontal compression (text1 only).
		if ((ctrl & TXC_HCMP) && layer == 0)
		{
			uint32_t hval = hCompValue((uint32_t)((py % virtH + virtH) % virtH));
			if (hval == 0) hval = HCMP_UNIT;
			sx = (double)px * horizontalSourceStep(hval) + (double)textX(layer);
		}

		int ix = (int)std::floor(sx);
		int iy = (int)std::floor(sy);
		ix = ((ix % virtW) + virtW) % virtW;
		iy = ((iy % virtH) + virtH) % virtH;

		Pixel pix;
		pix.transparent = true;

		if (ctrl & TXC_LINR)
		{
			// Bitmap mode: one 16-bit (high colour) or indexed pixel per
			// position laid out as scan lines.
			uint32_t base = textBufSA(layer, 0);
			if (ctrl & TXC_RGB565)
			{
				uint16_t v = mem.readHalf(base + (uint32_t)(iy * virtW + ix) * 2);
				pix.color = decodeRGB565(v);
				pix.transparent = false;
				applyColorKey(pix);
			}
			else if (ctrl & TXC_RGB555)
			{
				uint16_t v = mem.readHalf(base + (uint32_t)(iy * virtW + ix) * 2);
				pix = decodeRGB555(v); // bit15 = transparency enable
			}
			else
			{
				int bpp = bitsPerPixel(attr.color);
				uint64_t bitPos = (uint64_t)(iy * virtW + ix) * bpp;
				uint32_t idx = 0;
				for (int i = 0; i < bpp; i++)
				{
					uint64_t bp = bitPos + i;
					uint8_t bit = (mem.readByte(base + (uint32_t)(bp >> 3)) >> (bp & 7)) & 1;
					idx |= (uint32_t)bit << i;
				}
				pix = decodePaletteEntry(paletteRaw(paletteEntryIndex(attr.palette, idx, attr.color)));
				applyColorKey(pix);
			}
		}
		else
		{
			// Character mode.
			uint32_t cw = sizeFromCode(attr.hsizeCode);
			uint32_t ch = sizeFromCode(attr.vsizeCode);
			uint32_t charsPerRow = (cw ? (uint32_t)virtW / cw : 1);

			uint32_t col = (uint32_t)ix / cw;
			uint32_t row = (uint32_t)iy / ch;
			uint32_t cxi = (uint32_t)ix % cw;
			uint32_t cyi = (uint32_t)iy % ch;

			uint32_t arrayIndex = (ctrl & TXC_WAP) ? 0 : (row * charsPerRow + col);
			uint16_t charNum = mem.readHalf(textNPtr(layer) + arrayIndex * 2);

			if (charNum != 0)
			{
				uint32_t charBytes = cw * ch * bitsPerPixel(attr.color) / 8;
				uint32_t charAddr = textBufSA(layer, 0) + (uint32_t)charNum * charBytes;
				pix = fetchCharPixel(mem, charAddr, cxi, cyi, cw, ch, attr.color, attr.palette, attr.flip);
			}
		}

		if (!pix.transparent)
		{
			Fragment f;
			f.ref = LayerRef{ false, attr.depth, (uint16_t)layer };
			f.color = pix.color;
			f.blend = (ctrl & TXC_BLEND) != 0;
			f.level = (uint8_t)textBlendLevel(layer);
			frags.push_back(f);
		}
	}

	// ---- Sprites ----------------------------------------------------
	if (spriteEnabled())
	{
		int maxSprite = (int)spriteMax();
		for (int i = 0; i <= maxSprite && i < 512; i++)
		{
			SpriteAttr s = decodeSprite(i);
			if (s.charNum == 0) continue; // transparent character

			int w = sizeFromCode(s.hsizeCode);
			int h = sizeFromCode(s.vsizeCode);

			int left = s.x;
			int top  = s.y;
			if (!coordTopLeft())
			{
				left = s.x + sw / 2;
				top  = s.y + sh / 2;
			}

			if (px < left || px >= left + w || py < top || py >= top + h)
				continue;

			uint32_t cxi = (uint32_t)(px - left);
			uint32_t cyi = (uint32_t)(py - top);
			uint32_t charBytes = (uint32_t)w * h * bitsPerPixel(s.color) / 8;
			uint32_t charAddr = spriteBufSA() + (uint32_t)s.charNum * charBytes;

			Pixel pix = fetchCharPixel(mem, charAddr, cxi, cyi, w, h, s.color, s.palette, s.flip);
			if (pix.transparent) continue;

			Fragment f;
			f.ref = LayerRef{ true, s.depth, (uint16_t)i };
			f.color = pix.color;
			f.blend = s.blend;
			f.level = s.blendLevel;
			frags.push_back(f);
		}
	}

	// ---- Composite bottom -> top -----------------------------------
	std::sort(frags.begin(), frags.end(),
	          [](const Fragment &a, const Fragment &b) { return isAbove(b.ref, a.ref); });

	Color out = backdrop;
	for (const Fragment &f : frags)
	{
		if (f.blend)
			out = blendColor(f.color, out, f.level, blendSubtract());
		else
			out = f.color;
	}
	return out;
}

std::vector<PPU::Color> PPU::renderFrame(const PPUMemory &mem, Color backdrop) const
{
	int w, h;
	screenSize(w, h);
	std::vector<Color> fb((size_t)w * h, backdrop);

	if (!ppuEnabled())
		return fb;

	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			fb[(size_t)y * w + x] = samplePixel(mem, x, y, backdrop);

	return fb;
}
