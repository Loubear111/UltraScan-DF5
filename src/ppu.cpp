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
	m_spritePalette.fill(0);
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
	if (off >= SPRITE_PALETTE_BASE && off < SPRITE_PALETTE_BASE + 0x800)
		return m_spritePalette[(off - SPRITE_PALETTE_BASE) >> 2];
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

	// Status flags are write-1-to-clear so a handler can acknowledge them.
	if (off == P_IRQ_Status)
		m_ctrl[P_IRQ_Status / 4] &= ~data;
	else if (off < 0x1000)
		m_ctrl[(off >> 2) & 0x3F] = data;
	else if (off >= PALETTE_BASE && off < PALETTE_BASE + 0x800)
		m_palette[(off - PALETTE_BASE) >> 2] = data;
	else if (off >= SPRITE_PALETTE_BASE && off < SPRITE_PALETTE_BASE + 0x800)
		m_spritePalette[(off - SPRITE_PALETTE_BASE) >> 2] = data;
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
                               ColorMode color, uint8_t palette, Flip flip, bool sprite) const
{
	applyFlip(flip, cx, cy, cw, ch);

	uint32_t pixelIndex = cy * cw + cx;
	int bpp = bitsPerPixel(color);
	uint64_t bitPos = (uint64_t)pixelIndex * bpp;

	const uint32_t shift = (uint32_t)(bitPos & 7);
	const uint32_t byteAddr = charBaseAddr + (uint32_t)(bitPos >> 3);
	uint32_t raw = mem.readByte(byteAddr);
	if (shift + (uint32_t)bpp > 8)
		raw |= (uint32_t)mem.readByte(byteAddr + 1) << 8;   // the pixel straddles two bytes
	uint32_t idx = (raw >> shift) & ((1u << bpp) - 1u);
	// Sprites hold the earlier pixel in the HIGH bits of a byte (2 and 4 bpp), unlike the
	// programming guide's section 9.4.5 (low bits first), which Ben 10's sprites contradict:
	// low-first draws the Omnitrix logo with every pixel pair swapped. This matches MAME's
	// spg290_ppu. Text-layer characters are left as the guide describes (unverified).
	if (sprite && (bpp == 2 || bpp == 4))
		idx = (raw >> (8 - (uint32_t)bpp - shift)) & ((1u << bpp) - 1u);

	uint32_t entryIdx = paletteEntryIndex(palette, idx, color);
	Pixel p = decodePaletteEntry(sprite ? spritePaletteRaw(entryIdx) : paletteRaw(entryIdx));
	applyColorKey(p);
	return p;
}

std::vector<PPU::SpriteRect> PPU::prepareSprites() const
{
	std::vector<SpriteRect> list;
	if (!spriteEnabled() || (debugHide & HIDE_SPRITES))
		return list;
	int sw, sh;
	screenSize(sw, sh);
	const int maxSprite = (int)spriteMax();
	for (int i = 0; i <= maxSprite && i < 512; i++)
	{
		SpriteRect r;
		r.s = decodeSprite(i);
		if (r.s.charNum == 0) continue; // transparent character
		r.index = i;
		r.w = sizeFromCode(r.s.hsizeCode);
		r.h = sizeFromCode(r.s.vsizeCode);
		r.left = r.s.x;
		r.top  = r.s.y;
		if (!coordTopLeft())
		{
			r.left = r.s.x + sw / 2;
			r.top  = r.s.y + sh / 2;
		}
		const uint32_t charBytes = (uint32_t)r.w * r.h * bitsPerPixel(r.s.color) / 8;
		r.charAddr = spriteBufSA() + (uint32_t)r.s.charNum * charBytes;
		list.push_back(r);
	}
	return list;
}

PPU::RenderState PPU::prepareState() const
{
	RenderState st;
	st.virtW = (resolution() == RES_VGA) ? 1024 : 512;
	st.virtH = (resolution() == RES_VGA) ? 512 : 256;
	for (int layer = 0; layer < 3; layer++)
	{
		TextLayerCtx &c = st.L[layer];
		c.ctrl = textCtrl(layer);
		c.on = (c.ctrl & TXC_TXE) && !((debugHide >> layer) & 1u);
		if (!c.on)
			continue;
		c.attr   = decodeAttr(textAttr(layer));
		c.tx     = textX(layer);
		c.ty     = textY(layer);
		c.buf    = textBufSA(layer, 0);
		c.nptr   = textNPtr(layer);
		c.bitmap = (c.ctrl & TXC_LINR) != 0;
		c.rgb565 = (c.ctrl & TXC_RGB565) != 0;
		c.wap    = (c.ctrl & TXC_WAP) != 0;
		c.mve    = (c.ctrl & TXC_MVE) != 0;
		c.blend  = (c.ctrl & TXC_BLEND) != 0;
		c.level  = (uint8_t)textBlendLevel(layer);
		c.slow   = (c.ctrl & TXC_VCMP) != 0 || ((c.ctrl & TXC_HCMP) && layer == 0);
		c.cw = (uint32_t)sizeFromCode(c.attr.hsizeCode);
		c.ch = (uint32_t)sizeFromCode(c.attr.vsizeCode);
		c.charsPerRow = c.cw ? (uint32_t)st.virtW / c.cw : 1;
		c.charBytes = c.cw * c.ch * (uint32_t)bitsPerPixel(c.attr.color) / 8;
	}
	return st;
}

PPU::Color PPU::samplePixel(const PPUMemory &mem, int px, int py, Color backdrop) const
{
	std::vector<SpriteRect> sprites = prepareSprites();
	std::vector<const SpriteRect *> ptrs;
	for (const SpriteRect &r : sprites)
		ptrs.push_back(&r);
	std::vector<Fragment> frags;
	RenderState st = prepareState();
	return samplePixelImpl(mem, px, py, backdrop, ptrs.data(), ptrs.size(), frags, st);
}

PPU::Color PPU::samplePixelImpl(const PPUMemory &mem, int px, int py, Color backdrop,
                                const SpriteRect *const *sprites, size_t spriteCount,
                                std::vector<Fragment> &frags, RenderState &st) const
{
	frags.clear();

	const int virtW = st.virtW;
	const int virtH = st.virtH;

	// Per-scanline values. The virtual screen sizes are powers of two, so wrapping is a mask
	// (the same as ((v % n) + n) % n for negative v too).
	if (st.row != py)
	{
		st.row = py;
		for (int layer = 0; layer < 3; layer++)
		{
			const TextLayerCtx &c = st.L[layer];
			if (!c.on || c.slow)
				continue;
			const int iy = (py + c.ty) & (virtH - 1);
			st.iy[layer] = iy;
			st.sxBase[layer] = c.tx + (c.mve ? (int)hvOffset((uint32_t)(py & (virtH - 1))) : 0);
			st.rowOff[layer] = c.bitmap ? mem.readWord(c.nptr + (uint32_t)iy * 4) : 0;
			// A bitmap row is virtW consecutive 16-bit pixels (the x index wraps inside it).
			st.rowPtr[layer] = c.bitmap ? mem.directSpan(c.buf + st.rowOff[layer] * 2, (uint32_t)virtW * 2) : nullptr;
		}
	}

	// ---- Text layers ------------------------------------------------
	for (int layer = 0; layer < 3; layer++)
	{
		const TextLayerCtx &c = st.L[layer];
		if (!c.on) continue;

		int ix, iy;
		uint32_t rowOff = 0;
		if (!c.slow)
		{
			ix = (px + st.sxBase[layer]) & (virtW - 1);
			iy = st.iy[layer];
			rowOff = st.rowOff[layer];
		}
		else
		{
			// Source coordinate in the (wrapping) virtual screen.
			double sx = (double)px + (double)c.tx;
			double sy = (double)py + (double)c.ty;

			// Per-line horizontal movement.
			if (c.mve)
				sx += (double)hvOffset((uint32_t)((py % virtH + virtH) % virtH));

			// Vertical compression / extension (whole layer).
			if (c.ctrl & TXC_VCMP)
			{
				uint32_t vval = read32(P_VComp_Value) ? read32(P_VComp_Value) : VCMP_UNIT;
				sy = (double)read32(P_VComp_Offset) + (double)py * verticalSourceStep(vval);
			}
			// Horizontal compression (text1 only).
			if ((c.ctrl & TXC_HCMP) && layer == 0)
			{
				uint32_t hval = hCompValue((uint32_t)((py % virtH + virtH) % virtH));
				if (hval == 0) hval = HCMP_UNIT;
				sx = (double)px * horizontalSourceStep(hval) + (double)c.tx;
			}

			ix = (int)std::floor(sx);
			iy = (int)std::floor(sy);
			ix = ((ix % virtW) + virtW) % virtW;
			iy = ((iy % virtH) + virtH) % virtH;
			if (c.bitmap)
				rowOff = mem.readWord(c.nptr + (uint32_t)iy * 4);
		}

		Pixel pix;
		pix.transparent = true;

		if (c.bitmap)
		{
			// Bitmap mode: one 16-bit pixel per position. Each scan line is found through
			// the number-array pointer, which addresses a table of 32-bit row offsets in
			// pixels (Ben 10: row r at r * 0x140 for a 320-pixel line). The format is
			// RGB565 with TXC_RGB565, otherwise ARGB1555. The guide's indexed bitmap
			// variant is gone: Ben 10 uses this mode with neither format flag set and
			// the buffer holds 16-bit colours, not indices.
			uint16_t v;
			if (const uint8_t *rp = c.slow ? nullptr : st.rowPtr[layer])
				v = (uint16_t)(rp[ix * 2] | (rp[ix * 2 + 1] << 8));
			else
				v = mem.readHalf(c.buf + (rowOff + (uint32_t)ix) * 2);
			if (c.rgb565)
			{
				pix.color = decodeRGB565(v);
				pix.transparent = false;
				applyColorKey(pix);
			}
			else
				pix = decodeRGB555(v); // bit15 = transparency flag
		}
		else
		{
			// Character mode.
			const uint32_t cw = c.cw, ch = c.ch;
			const uint32_t col = (uint32_t)ix / cw;
			const uint32_t row = (uint32_t)iy / ch;
			const uint32_t cxi = (uint32_t)ix % cw;
			const uint32_t cyi = (uint32_t)iy % ch;

			const uint32_t arrayIndex = c.wap ? 0 : (row * c.charsPerRow + col);
			const uint16_t charNum = mem.readHalf(c.nptr + arrayIndex * 2);

			if (charNum != 0)
			{
				const uint32_t charAddr = c.buf + (uint32_t)charNum * c.charBytes;
				pix = fetchCharPixel(mem, charAddr, cxi, cyi, cw, ch, c.attr.color, c.attr.palette, c.attr.flip);
			}
		}

		if (!pix.transparent)
		{
			Fragment f;
			f.ref = LayerRef{ false, c.attr.depth, (uint16_t)layer };
			f.color = pix.color;
			f.blend = c.blend;
			f.level = c.level;
			frags.push_back(f);
		}
	}

	// ---- Sprites ----------------------------------------------------
	for (size_t si = 0; si < spriteCount; ++si)
	{
		const SpriteRect &r = *sprites[si];
		const SpriteAttr &s = r.s;
		if (px < r.left || px >= r.left + r.w || py < r.top || py >= r.top + r.h)
			continue;

		uint32_t cxi = (uint32_t)(px - r.left);
		uint32_t cyi = (uint32_t)(py - r.top);

		Pixel pix = fetchCharPixel(mem, r.charAddr, cxi, cyi, r.w, r.h, s.color, s.palette, s.flip, true);
		if (pix.transparent) continue;

		Fragment f;
		f.ref = LayerRef{ true, s.depth, (uint16_t)r.index };
		f.color = pix.color;
		f.blend = s.blend;
		f.level = s.blendLevel;
		frags.push_back(f);
	}

	// ---- Composite bottom -> top -----------------------------------
	// Insertion sort: there are only a few fragments, and every layer reference is unique
	// (a layer or sprite index), so the order is the same as std::sort would give.
	for (size_t k = 1; k < frags.size(); ++k)
	{
		const Fragment f = frags[k];
		size_t m = k;
		while (m > 0 && isAbove(frags[m - 1].ref, f.ref))
		{
			frags[m] = frags[m - 1];
			--m;
		}
		frags[m] = f;
	}

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

	const std::vector<SpriteRect> sprites = prepareSprites();   // decoded once per frame
	std::vector<const SpriteRect *> row;                        // the sprites that touch this scanline, by left edge
	std::vector<const SpriteRect *> active;                     // those covering the current pixel
	std::vector<Fragment> frags;
	frags.reserve(16);
	RenderState st = prepareState();
	for (int y = 0; y < h; y++)
	{
		row.clear();
		for (const SpriteRect &r : sprites)
			if (y >= r.top && y < r.top + r.h)
				row.push_back(&r);
		std::stable_sort(row.begin(), row.end(),
		                 [](const SpriteRect *a, const SpriteRect *b) { return a->left < b->left; });
		size_t next = 0;
		active.clear();
		for (int x = 0; x < w; x++)
		{
			// Sweep: a sprite joins the active set at its left edge and leaves after its right
			// edge. The order inside the set does not matter (fragments are depth-sorted).
			while (next < row.size() && row[next]->left <= x)
				active.push_back(row[next++]);
			for (size_t k = 0; k < active.size();)
			{
				if (active[k]->left + active[k]->w <= x)
				{
					active[k] = active.back();
					active.pop_back();
				}
				else
					++k;
			}
			fb[(size_t)y * w + x] = samplePixelImpl(mem, x, y, backdrop, active.data(), active.size(), frags, st);
		}
	}

	return fb;
}
