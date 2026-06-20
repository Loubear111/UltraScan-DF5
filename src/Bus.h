#pragma once
#include <cstdint>
#include <array>

#include "spg290.h"
#include "ppu.h"

class Bus
{
public:
	Bus();
	~Bus();

public: // Devices on bus
	spg290 cpu;

	// 2D Picture Process Unit (memory mapped at PPU::BASE = 0x8801_0000).
	PPU ppu;

	// Fake RAM for testing of CPU -> Bus functionality
	std::array<uint32_t, 1 * 1024> ram;

public: // Bus Read and Write
	void write(uint16_t addr, uint32_t data);
	uint32_t read(uint16_t addr, bool bReadOnly = false);

	// 32-bit address path for memory-mapped devices such as the PPU. The
	// CPU's 16-bit RAM window is unaffected; these route full 32-bit device
	// addresses (e.g. the 0x8801_xxxx PPU register block) to the right
	// device.
	void     write32(uint32_t addr, uint32_t data);
	uint32_t read32(uint32_t addr);
};
