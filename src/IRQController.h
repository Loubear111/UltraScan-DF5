#pragma once

#include <cstdint>

// ============================================================================
// SPG290 peripheral interrupt controller (SPG290 Programming Guide ch. 26).
//
// 40 level-sensitive, active-high sources are merged into the single IRQ line
// of the Score7 core. Priority arbitration happens here, and the winning source
// is reported as a vector number (63 - source) which the core uses to pick its
// entry address. The register file lives at BASE (map: 0x080A_0000, IRQCTL).
//
// Mask polarity (1 = masked) and the 0x20/0x24 mask register offsets come from
// the Sunplus Linux port, not the guide; see docs/phase3-interrupt-wiring.md.
// ============================================================================
class IRQController
{
public:
	IRQController() { reset(); }

	static constexpr uint32_t BASE   = 0x880A0000u;
	static constexpr uint32_t WINDOW = 0x10000u;

	enum : uint32_t
	{
		P_INTPND    = 0x00, // R   pending, sources 0..31
		P_INTPND_H  = 0x04, // R   pending, sources 32..39
		P_I_PMST    = 0x08, // R/W slave-group priorities (2 bits per group)
		P_I_GPUSWI  = 0x0C, // R/W bit0 raise software IRQ, bit8 clear it
		P_I_PSLV0   = 0x10, // R/W per-source priority, 3 bits per source
		P_I_PSLV1   = 0x14,
		P_I_PSLV2   = 0x18,
		P_I_PSLV3   = 0x1C,
		P_I_MASKL   = 0x20, // R/W 1 = masked, sources 0..31
		P_I_MASKH   = 0x24, // R/W 1 = masked, sources 32..39
	};

	static constexpr unsigned NUM_SOURCES = 40;

	// Sources driven by modelled devices.
	static constexpr unsigned SRC_PPU_VBLK_START = 10; // vector 53
	static constexpr unsigned SRC_PPU_HV_HIT     = 17; // vector 46
	static constexpr unsigned SRC_VBLK_END       = 36; // vector 27

	// The software interrupt (P_I_GPUSWI) is delivered on the PPU vblank source.
	static constexpr unsigned SRC_GPU_SWI = SRC_PPU_VBLK_START;

	static constexpr uint8_t vectorOf(unsigned source) { return (uint8_t)(63u - source); }

	void reset();

	// Drive one input line. Sources >= NUM_SOURCES are ignored.
	void setSource(unsigned source, bool level);

	bool    irqActive() const    { return m_winner >= 0; }
	uint8_t activeVector() const { return m_winner >= 0 ? vectorOf((unsigned)m_winner) : 0; }
	int     activeSource() const { return m_winner; }

	uint32_t read32(uint32_t addr) const;
	void     write32(uint32_t addr, uint32_t data);

private:
	static constexpr uint64_t SOURCE_MASK = (1ull << NUM_SOURCES) - 1;

	uint64_t pending() const;
	void     arbitrate();

	uint64_t m_lines = 0;
	uint64_t m_mask  = SOURCE_MASK;
	uint32_t m_pmst  = 0;
	uint32_t m_pslv[4] = {0, 0, 0, 0};
	bool     m_swi   = false;
	int      m_winner = -1;
};
