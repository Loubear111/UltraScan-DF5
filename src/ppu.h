#pragma once

// ============================================================================
// SPG290 2D Picture Process Unit (PPU)
//
// Software model of the SunPlus SPG290 "2D Picture Process Unit" described in
// chapter 9 of the SPG290 Programming Guide (v0.5). The PPU is a character /
// sprite based 2D compositor: it renders up to 512 foreground sprites and 3
// background "TEXT" layers into an output frame buffer that lives in external
// DRAM.
//
// This implementation focuses on faithfully modelling the parts of the PPU
// that have well-defined, testable behaviour:
//
//   * The memory-mapped register file (base 0x8801_0000) with the exact
//     register offsets and bit-field layouts from section 9.19.
//   * Colour / pixel format conversion (indexed palette modes, ARGB1555 and
//     RGB565 high-colour bitmap modes).
//   * Palette-bank addressing (section 9.3.1).
//   * Indexed pixel unpacking from packed character data (section 9.4.5).
//   * 64-grade alpha blending (section 9.13 / P_Blend_Sub).
//   * Depth / priority compositing (section 9.4.2).
//   * Vertical / horizontal compression scale math (section 9.9).
//   * A sprite + text compositing renderer that ties the above together and
//     reads its source data through an abstract memory interface.
//
// Where the programming guide is internally inconsistent (it disagrees with
// itself on a few bit positions and on palette channel ordering), the chosen
// convention is documented next to the relevant constant. The conventions are
// applied consistently throughout and are covered by the unit tests.
// ============================================================================

#include <cstdint>
#include <array>
#include <vector>
#include <algorithm>

// ----------------------------------------------------------------------------
// Abstract DRAM interface.
//
// The real PPU fetches character/sprite/bitmap data and writes the rendered
// frame buffer through the system bus into external DRAM. The renderer is
// decoupled from any particular memory implementation through this interface
// so it can be driven by the emulator bus or by a self-contained test buffer.
// All accesses are little-endian, matching section 9.1 ("Order of bytes").
// ----------------------------------------------------------------------------
class PPUMemory
{
public:
	virtual ~PPUMemory() = default;
	virtual uint8_t  readByte(uint32_t addr) const = 0;
	virtual uint16_t readHalf(uint32_t addr) const = 0;
	virtual uint32_t readWord(uint32_t addr) const = 0;
};

// Simple flat little-endian DRAM model backed by a byte vector. Useful for
// both the renderer's tests and for stand-alone demos. Addresses below `base`
// or beyond the buffer read back as zero.
class FlatMemory : public PPUMemory
{
public:
	explicit FlatMemory(uint32_t base = 0, size_t size = 0)
		: m_base(base), m_bytes(size, 0) {}

	void resize(size_t size) { m_bytes.resize(size, 0); }
	void setBase(uint32_t base) { m_base = base; }
	uint32_t base() const { return m_base; }
	size_t   size() const { return m_bytes.size(); }

	void writeByte(uint32_t addr, uint8_t v)
	{
		size_t i = index(addr);
		if (i < m_bytes.size()) m_bytes[i] = v;
	}
	void writeHalf(uint32_t addr, uint16_t v)
	{
		writeByte(addr,     (uint8_t)(v & 0xFF));
		writeByte(addr + 1, (uint8_t)((v >> 8) & 0xFF));
	}
	void writeWord(uint32_t addr, uint32_t v)
	{
		writeByte(addr,     (uint8_t)(v & 0xFF));
		writeByte(addr + 1, (uint8_t)((v >> 8) & 0xFF));
		writeByte(addr + 2, (uint8_t)((v >> 16) & 0xFF));
		writeByte(addr + 3, (uint8_t)((v >> 24) & 0xFF));
	}

	uint8_t readByte(uint32_t addr) const override
	{
		size_t i = index(addr);
		return (i < m_bytes.size()) ? m_bytes[i] : 0;
	}
	uint16_t readHalf(uint32_t addr) const override
	{
		return (uint16_t)(readByte(addr) | (readByte(addr + 1) << 8));
	}
	uint32_t readWord(uint32_t addr) const override
	{
		return (uint32_t)readByte(addr)
		     | ((uint32_t)readByte(addr + 1) << 8)
		     | ((uint32_t)readByte(addr + 2) << 16)
		     | ((uint32_t)readByte(addr + 3) << 24);
	}

private:
	size_t index(uint32_t addr) const
	{
		return (addr < m_base) ? (size_t)-1 : (size_t)(addr - m_base);
	}
	uint32_t m_base;
	std::vector<uint8_t> m_bytes;
};

class PPU
{
public:
	PPU();

	// ------------------------------------------------------------------
	// Memory map (section 9.19). All registers live in a 16K-byte window
	// based at 0x8801_0000.
	// ------------------------------------------------------------------
	static constexpr uint32_t BASE = 0x88010000u;

	// Global control registers.
	enum : uint32_t
	{
		P_PPU_Control = 0x0000, // PPUEN + Resolution[1:0]
		P_SP_Control  = 0x0004, // Coord_sel + Sp_en
		P_SP_Max      = 0x0008, // Max active sprite count (0..511)
		P_Blend_Sub   = 0x000C, // Global blend formula select
		P_Trans_RGB   = 0x0010, // Colour-key transparency (TransRGB_En + RGB565)

		// Per-text-layer registers are at TEXT_BASE + layer*TEXT_STRIDE.
		TEXT_BASE   = 0x0020,
		TEXT_STRIDE = 0x001C,
		TX_X        = 0x0000, // signed X position (-512..+511)
		TX_Y        = 0x0004, // signed Y position (-256..+255)
		TX_ATTR     = 0x0008, // attribute word (see ATTR_* bit fields)
		TX_CTRL     = 0x000C, // control word    (see TXC_*  bit fields)
		TX_NPTR     = 0x0010, // 32-bit number-array page pointer
		TX_BLEND    = 0x0018, // blend level (0..63)

		P_VComp_Value  = 0x0074, // vertical compression scale  (0x20 = unit gain)
		P_VComp_Offset = 0x0078, // vertical compression start offset
		P_VComp_Step   = 0x007C, // vertical compression per-line step (signed)

		P_IRQ_Control = 0x0080, // HV_IRQ_EN | BLK_END_EN | BLK_ST_EN
		P_IRQ_Status  = 0x0084, // HV_IRQ | BLK_IRQ_END | BLK_IRQ_ST
		P_IRQTMV      = 0x0088, // H/V hit IRQ vertical position
		P_IRQTMH      = 0x008C, // H/V hit IRQ horizontal position
		P_VBLK_TIME   = 0x0090, // vertical blank time extension (lines)

		// Buffer start addresses. Text layer L buffer k is at
		// BUF_SA_BASE + L*0x0C + k*4.
		BUF_SA_BASE   = 0x00A0,
		P_Frame_BUF_SA0 = 0x00C4,
		P_Frame_BUF_SA1 = 0x00C8,
		P_Frame_BUF_SA2 = 0x00CC,
		P_SP_BUF_SA     = 0x00D0,

		PALETTE_BASE  = 0x1000, // 512 entries, 4-byte stride
		HVOFFSET_BASE = 0x2000, // 512 per-line horizontal offsets
		HCOMP_BASE    = 0x3000, // 512 per-line horizontal compression scales
		SPRITE_BASE   = 0x4000, // 512 sprites, 8-byte stride (2 words each)
	};

	// Resolution field values (P_PPU_Control bits 1:0).
	enum Resolution : uint32_t
	{
		RES_QVGA = 0, // 320x240
		RES_VGA  = 1, // 640x480
		RES_HVGA = 2, // 640x240
		RES_CIF  = 3, // 320x240 (VGA scaled to CIF)
	};

	// --- P_PPU_Control bit fields ------------------------------------
	// PPUEN at bit 15, Resolution at bits 1:0.
	static constexpr uint32_t PPUEN_BIT   = 1u << 15;
	static constexpr uint32_t RES_MASK    = 0x3u;

	// --- P_SP_Control bit fields -------------------------------------
	static constexpr uint32_t SP_EN_BIT    = 1u << 0; // sprite engine enable
	static constexpr uint32_t COORD_SEL_BIT = 1u << 1; // 0=centre origin,1=top/left

	// --- P_Blend_Sub -------------------------------------------------
	// 0 : C = A*a + B*(1-a)   (blend over)
	// 1 : C = A*a - B*(1-a)   (subtractive)
	static constexpr uint32_t BLEND_SUB_BIT = 1u << 0;

	// --- P_Trans_RGB -------------------------------------------------
	static constexpr uint32_t TRANS_RGB_EN_BIT = 1u << 16;
	static constexpr uint32_t TRANS_RGB_MASK   = 0xFFFFu;

	// --- Attribute word bit fields (text register-mode & sprites) ----
	// Color[1:0] Flip[3:2] Hs[5:4] Vs[7:6] Palette[12:8] Depth[14:13].
	// Sprites additionally use Blend at bit 15.
	static constexpr uint32_t ATTR_COLOR_SHIFT   = 0;  static constexpr uint32_t ATTR_COLOR_MASK   = 0x3;
	static constexpr uint32_t ATTR_FLIP_SHIFT    = 2;  static constexpr uint32_t ATTR_FLIP_MASK    = 0x3;
	static constexpr uint32_t ATTR_HSIZE_SHIFT   = 4;  static constexpr uint32_t ATTR_HSIZE_MASK   = 0x3;
	static constexpr uint32_t ATTR_VSIZE_SHIFT   = 6;  static constexpr uint32_t ATTR_VSIZE_MASK   = 0x3;
	static constexpr uint32_t ATTR_PALETTE_SHIFT = 8;  static constexpr uint32_t ATTR_PALETTE_MASK = 0x1F;
	static constexpr uint32_t ATTR_DEPTH_SHIFT   = 13; static constexpr uint32_t ATTR_DEPTH_MASK   = 0x3;
	static constexpr uint32_t ATTR_BLEND_BIT     = 1u << 15;

	// --- Text control word bit fields --------------------------------
	static constexpr uint32_t TXC_LINR  = 1u << 0;  // 0=character mode,1=bitmap mode
	static constexpr uint32_t TXC_RGM   = 1u << 1;  // 1=register set, 0=attribute array
	static constexpr uint32_t TXC_WAP   = 1u << 2;  // wallpaper effect enable
	static constexpr uint32_t TXC_TXE   = 1u << 3;  // text layer enable (visible)
	static constexpr uint32_t TXC_MVE   = 1u << 4;  // horizontal movement enable
	static constexpr uint32_t TXC_HCMP  = 1u << 5;  // horizontal compression (text1 only)
	static constexpr uint32_t TXC_VCMP  = 1u << 6;  // vertical compression enable
	static constexpr uint32_t TXC_BLEND = 1u << 8;  // blend enable (register mode)
	static constexpr uint32_t TXC_RGB555 = 1u << 12; // 32768-colour bitmap (ARGB1555)
	static constexpr uint32_t TXC_RGB565 = 1u << 15; // 65536-colour bitmap (RGB565)

	// Compression scale anchors (section 9.9).
	static constexpr uint32_t VCMP_UNIT = 0x20; // vertical unit gain
	static constexpr uint32_t HCMP_UNIT = 0x80; // horizontal unit gain

	// ------------------------------------------------------------------
	// Colour / pixel types.
	// ------------------------------------------------------------------
	struct Color
	{
		uint8_t r = 0, g = 0, b = 0;
		bool operator==(const Color &o) const { return r == o.r && g == o.g && b == o.b; }
		bool operator!=(const Color &o) const { return !(*this == o); }
	};

	// A decoded source pixel: a colour plus whether it is transparent.
	struct Pixel
	{
		Color color;
		bool  transparent = true;
	};

	// Colour mode (attribute Color field). Indexed modes only - the two
	// 16-bit high-colour modes are signalled separately by control bits.
	enum class ColorMode { BPP2 = 0, BPP4 = 1, BPP6 = 2, BPP8 = 3 };

	// Flip mode (attribute Flip field).
	enum class Flip { NONE = 0, H = 1, V = 2, HV = 3 };

	// ------------------------------------------------------------------
	// Register file access. Both absolute (0x8801_xxxx) and BASE-relative
	// offsets are accepted. Reads/writes are 32-bit word granular; the
	// sprite region is exposed as the two-word "Num_X" / "Att_Y" view.
	// ------------------------------------------------------------------
	uint32_t read32(uint32_t addr) const;
	void     write32(uint32_t addr, uint32_t data);

	void reset();

	// --- Typed global-register accessors -----------------------------
	bool       ppuEnabled()  const { return (m_ctrl[P_PPU_Control / 4] & PPUEN_BIT) != 0; }
	Resolution resolution()  const { return (Resolution)(m_ctrl[P_PPU_Control / 4] & RES_MASK); }
	bool       spriteEnabled() const { return (m_ctrl[P_SP_Control / 4] & SP_EN_BIT) != 0; }
	bool       coordTopLeft()  const { return (m_ctrl[P_SP_Control / 4] & COORD_SEL_BIT) != 0; }
	uint32_t   spriteMax()   const { return m_ctrl[P_SP_Max / 4] & 0x1FF; }
	bool       blendSubtract() const { return (m_ctrl[P_Blend_Sub / 4] & BLEND_SUB_BIT) != 0; }
	bool       transRGBEnabled() const { return (m_ctrl[P_Trans_RGB / 4] & TRANS_RGB_EN_BIT) != 0; }
	uint16_t   transRGB()    const { return (uint16_t)(m_ctrl[P_Trans_RGB / 4] & TRANS_RGB_MASK); }

	static void resolutionSize(Resolution r, int &w, int &h);
	void screenSize(int &w, int &h) const { resolutionSize(resolution(), w, h); }

	// --- Per-text-layer register accessors (layer 0..2) --------------
	int32_t  textX(int layer)        const { return signExtend(ctrlAt(textReg(layer, TX_X)), 16); }
	int32_t  textY(int layer)        const { return signExtend(ctrlAt(textReg(layer, TX_Y)), 16); }
	uint32_t textAttr(int layer)     const { return ctrlAt(textReg(layer, TX_ATTR)); }
	uint32_t textCtrl(int layer)     const { return ctrlAt(textReg(layer, TX_CTRL)); }
	uint32_t textNPtr(int layer)     const { return ctrlAt(textReg(layer, TX_NPTR)); }
	uint32_t textBlendLevel(int layer) const { return ctrlAt(textReg(layer, TX_BLEND)) & 0x3F; }
	uint32_t textBufSA(int layer, int k) const { return ctrlAt(BUF_SA_BASE + layer * 0x0C + k * 4); }
	bool     textEnabled(int layer)  const { return (textCtrl(layer) & TXC_TXE) != 0; }

	uint32_t frameBufSA(int idx) const { return ctrlAt(P_Frame_BUF_SA0 + idx * 4); }
	uint32_t spriteBufSA()       const { return ctrlAt(P_SP_BUF_SA); }

	// Palette entries (raw 16-bit values, 512 entries).
	uint16_t paletteRaw(uint32_t i) const { return (uint16_t)(m_palette[i & 0x1FF] & 0xFFFF); }
	void     setPaletteRaw(uint32_t i, uint16_t v) { m_palette[i & 0x1FF] = v; }

	uint16_t hvOffset(uint32_t line) const { return (uint16_t)(m_hvoffset[line & 0x1FF] & 0x3FF); }
	uint16_t hCompValue(uint32_t line) const { return (uint16_t)(m_hcomp[line & 0x1FF] & 0x1FF); }

	// --- Sprite attribute decode -------------------------------------
	struct SpriteAttr
	{
		uint16_t  charNum   = 0;
		int32_t   x         = 0;
		int32_t   y         = 0;
		uint8_t   depth     = 0;
		uint8_t   palette   = 0;
		uint8_t   hsizeCode = 0;
		uint8_t   vsizeCode = 0;
		ColorMode color     = ColorMode::BPP4;
		Flip      flip      = Flip::NONE;
		bool      blend     = false;
		uint8_t   blendLevel = 0;
	};
	SpriteAttr decodeSprite(int index) const;

	// --- Text attribute decode ---------------------------------------
	struct TextAttr
	{
		uint8_t   depth     = 0;
		uint8_t   palette   = 0;
		uint8_t   hsizeCode = 0;
		uint8_t   vsizeCode = 0;
		ColorMode color     = ColorMode::BPP4;
		Flip      flip      = Flip::NONE;
	};
	static TextAttr decodeAttr(uint32_t attrWord);

	// ==================================================================
	// Pure helper functions (all static, side-effect free). These are the
	// data-path primitives the renderer is built from and the bulk of what
	// the unit tests exercise.
	// ==================================================================

	static int bitsPerPixel(ColorMode m);   // 2/4/6/8
	static int colorCount(ColorMode m);      // 4/16/64/256
	static int sizeFromCode(uint8_t code);   // 0->8,1->16,2->32,3->64

	// 5/6-bit -> 8-bit channel expansion (bit replication).
	static uint8_t expand5(uint8_t c) { return (uint8_t)((c << 3) | (c >> 2)); }
	static uint8_t expand6(uint8_t c) { return (uint8_t)((c << 2) | (c >> 4)); }

	// Palette entry (indexed modes) and high-colour bitmap pixels.
	// Channel order follows the section 9.19 register-set table:
	//   ARGB1555 / RGB555 : Tr[15] R[14:10] G[9:5] B[4:0]
	//   RGB565            :        R[15:11] G[10:5] B[4:0]
	static Pixel decodeRGB555(uint16_t v); // transparent when bit15 set
	static Color decodeRGB565(uint16_t v);

	// Decode a palette entry honouring its transparent (Tr) bit.
	static Pixel decodePaletteEntry(uint16_t entry);

	// Palette-bank addressing (section 9.3.1):
	//   entryIndex = bank * colorCount(mode) + colorIndex
	static uint32_t paletteEntryIndex(uint8_t bank, uint32_t colorIndex, ColorMode mode);
	static uint32_t palettePhysAddr(uint8_t bank, uint32_t colorIndex, ColorMode mode);

	// Unpack the palette index of pixel `pixelIndex` (row-major, left/top
	// first) from packed little-endian character data. Within the bit
	// stream the lowest-order bits hold the earliest pixel (section 9.4.5).
	static uint32_t unpackIndex(const uint8_t *data, size_t dataLen,
	                            uint32_t pixelIndex, ColorMode mode);

	// 64-grade alpha blend (section 9.13). level in 0..63, alpha=level/64.
	//   sub=false : (top*level + bottom*(64-level)) / 64
	//   sub=true  : (top*level - bottom*(64-level)) / 64, clamped to [0,255]
	static uint8_t blendChannel(uint8_t top, uint8_t bottom, uint8_t level, bool sub);
	static Color   blendColor(Color top, Color bottom, uint8_t level, bool sub);

	// Compression scale source-step per output pixel (section 9.9):
	//   vertical   : value / 0x20   (0x20 unit, 0x40 half, 0x10 double)
	//   horizontal : value / 0x80   (0x80 unit, 0x40 half)
	static double verticalSourceStep(uint32_t vcmpValue);
	static double horizontalSourceStep(uint32_t hcmpValue);

	// Map an in-character coordinate through a flip mode.
	static void applyFlip(Flip flip, uint32_t &x, uint32_t &y, uint32_t w, uint32_t h);

	// Depth / priority compositing (section 9.4.2). A "layer reference" is
	// ordered by depth, then sprite-over-text at equal depth, then by sub
	// index (higher sprite number / higher text layer wins).
	struct LayerRef
	{
		bool     isSprite = false;
		uint8_t  depth    = 0;
		uint16_t subIndex = 0;
	};
	// Returns true when `a` should be composited ABOVE `b`.
	static bool isAbove(const LayerRef &a, const LayerRef &b);

	// ==================================================================
	// Renderer. Reads source data through `mem` and returns the composited
	// RGB frame buffer (row-major, width*height entries). Transparent
	// regions read back as the supplied backdrop colour.
	// ==================================================================
	std::vector<Color> renderFrame(const PPUMemory &mem, Color backdrop) const;
	std::vector<Color> renderFrame(const PPUMemory &mem) const { return renderFrame(mem, Color{}); }

	// Sample a single output pixel - exposed for fine-grained testing.
	Color samplePixel(const PPUMemory &mem, int px, int py, Color backdrop) const;

private:
	// Backing storage. The control block is word-addressed (offsets
	// 0x000..0x0FC); the palette / per-line / sprite regions are separate
	// arrays so the large offsets do not require a 20K backing array.
	std::array<uint32_t, 64>  m_ctrl{};       // 0x0000 .. 0x00FC
	std::array<uint32_t, 512> m_palette{};    // 0x1000 .. 0x17FC
	std::array<uint32_t, 512> m_hvoffset{};   // 0x2000 .. 0x27FC
	std::array<uint32_t, 512> m_hcomp{};      // 0x3000 .. 0x37FC
	std::array<uint32_t, 512> m_spriteNumX{}; // 0x4000 + 8*N
	std::array<uint32_t, 512> m_spriteAttY{}; // 0x4004 + 8*N

	static uint32_t offsetOf(uint32_t addr) { return (addr >= BASE) ? (addr - BASE) : addr; }
	static uint32_t textReg(int layer, uint32_t field) { return TEXT_BASE + layer * TEXT_STRIDE + field; }
	uint32_t ctrlAt(uint32_t off) const { return m_ctrl[(off / 4) & 0x3F]; }

	static int32_t signExtend(uint32_t v, int bits)
	{
		uint32_t m = 1u << (bits - 1);
		return (int32_t)((v ^ m) - m);
	}

	// Fetch one source pixel from a character at (cx, cy) within a
	// character of the given attributes, starting at `charBaseAddr` in DRAM.
	Pixel fetchCharPixel(const PPUMemory &mem, uint32_t charBaseAddr,
	                     uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch,
	                     ColorMode color, uint8_t palette, Flip flip) const;

	// Apply colour-key (P_Trans_RGB) transparency to an opaque pixel.
	void applyColorKey(Pixel &p) const;
};
