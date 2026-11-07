#pragma once
#include <cstdint>
#include <vector>

#include "ppu.h"

class Bus;

// ============================================================================
// SPG29x TV encoder (TVE) and framebuffer scan-out [MAME spg29x.cpp, unverified].
//
// All five demos draw RGB565 pixels into RAM and show them through these
// registers; none use the PPU. The device owns a handful of 32-bit registers
// scattered through the physical map:
//
//   0x08030000  control  bits 3:2 mode (0 QVGA 320x240, 1 VGA 640x480, 2 HVGA 640x240),
//                        bit 1 PAL (vs NTSC), bit 0 interlaced. Reads back.
//   0x0803000C  fade     low 8 bits; output = pixel * (255 - fade) / 255
//   0x08070000/04/08     TV start address 0/1/2 (read/write)
//   0x08090020  buffer select: value & 3; 3 = framebuffer off (reset value)
//
// compose() builds one output frame: the PPU frame (if any), overwritten by the
// RGB565 framebuffer chosen by the buffer select, then faded.
// ============================================================================
class TVE
{
public:
	static constexpr uint32_t CONTROL  = 0x08030000u;
	static constexpr uint32_t FADE     = 0x0803000Cu;
	static constexpr uint32_t START0   = 0x08070000u;
	static constexpr uint32_t START1   = 0x08070004u;
	static constexpr uint32_t START2   = 0x08070008u;
	static constexpr uint32_t BUFSEL   = 0x08090020u;

	// MAME, when not interlaced, reads only even source lines and doubles them
	// [MAME, unverified]. Compile with -DTVE_PROGRESSIVE_LINE_DOUBLING=0 to start
	// with it off; it can also be switched at run time.
#ifndef TVE_PROGRESSIVE_LINE_DOUBLING
#define TVE_PROGRESSIVE_LINE_DOUBLING 1
#endif
	bool progressiveLineDoubling = TVE_PROGRESSIVE_LINE_DOUBLING != 0;

	TVE() { reset(); }
	void reset();

	// True when the word-aligned physical address is one of the registers above.
	static bool owns(uint32_t physWord);
	uint32_t read32(uint32_t physWord) const;
	void     write32(uint32_t physWord, uint32_t data);

	uint32_t controlReg() const    { return m_control; }
	bool     interlaced() const    { return (m_control & 1u) != 0; }
	bool     pal() const           { return (m_control & 2u) != 0; }
	uint8_t  fade() const          { return (uint8_t)(m_fade & 0xFFu); }
	uint32_t startAddr(int i) const { return m_start[i]; }
	uint8_t  bufferSelect() const  { return (uint8_t)(m_bufSel & 3u); }
	bool     framebufferOn() const { return bufferSelect() < 3; }

	// Output size for the current mode (mode 3 is undefined and treated as QVGA).
	void size(int& w, int& h) const;

	// Build an output frame. `ppuFrame` (may be null) is the PPU's frame at the
	// PPU's own resolution; it is placed at the top-left of the TVE frame and the
	// rest stays black. Framebuffer pixels are read little-endian from the bus.
	std::vector<PPU::Color> compose(const Bus& bus, const std::vector<PPU::Color>* ppuFrame,
	                                int& w, int& h) const;

private:
	uint32_t m_control = 0;
	uint32_t m_fade    = 0;
	uint32_t m_start[3] = { 0, 0, 0 };
	uint32_t m_bufSel  = 3;
};
