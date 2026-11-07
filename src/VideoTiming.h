#pragma once
#include <cstdint>

// ============================================================================
// Scanline / frame counter. Pure arithmetic, no dependency on Bus or PPU.
//
// Constants are MAME's (27 MHz pixel clock, 858x525 total, 108 MHz CPU, i.e. 4
// CPU cycles per pixel clock). The active-line count is a placeholder.
// ============================================================================
class VideoTiming
{
public:
	static constexpr uint32_t CPU_CYCLES_PER_PIXEL = 4;
	static constexpr uint32_t PIXELS_PER_LINE      = 858;
	static constexpr uint32_t CPU_CYCLES_PER_LINE  = CPU_CYCLES_PER_PIXEL * PIXELS_PER_LINE;   // 3432
	static constexpr uint32_t LINES_PER_FRAME      = 525;
	static constexpr uint32_t ACTIVE_LINES         = 480;   // vblank starts when the line counter reaches this

	enum Event : uint32_t { EV_VBLANK_START = 1, EV_VBLANK_END = 2, EV_HV_HIT = 4 };

	void reset() { line = 0; lineCycle = 0; hvFired = false; }

	// Advance by `cpuCycles`. `hvV`/`hvH` come from P_IRQTMV/P_IRQTMH and
	// `vblkExtra` from P_VBLK_TIME (extra lines at the end of vblank [unknown
	// whether the hardware lengthens the frame; isolated in one expression]).
	// Returns an OR of Event bits.
	//
	// The H/V hit fires once per frame, at the first advance that has reached
	// the target line and pixel; it assumes steps shorter than one line.
	uint32_t advance(uint32_t cpuCycles, uint32_t hvV, uint32_t hvH, uint32_t vblkExtra)
	{
		uint32_t events = 0;
		lineCycle += cpuCycles;
		while (lineCycle >= CPU_CYCLES_PER_LINE)
		{
			lineCycle -= CPU_CYCLES_PER_LINE;
			++line;
			if (line == ACTIVE_LINES)
				events |= EV_VBLANK_START;
			if (line >= LINES_PER_FRAME + vblkExtra)
			{
				line = 0;
				hvFired = false;
				events |= EV_VBLANK_END;
			}
		}
		if (!hvFired && line == (hvV & 0x1FFu) && lineCycle >= (hvH & 0x3FFu) * CPU_CYCLES_PER_PIXEL)
		{
			hvFired = true;
			events |= EV_HV_HIT;
		}
		return events;
	}

	uint32_t currentLine() const      { return line; }
	uint32_t currentLineCycle() const { return lineCycle; }

private:
	uint32_t line = 0;
	uint32_t lineCycle = 0;
	bool     hvFired = false;
};
