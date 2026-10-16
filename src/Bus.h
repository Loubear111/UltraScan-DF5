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

	// RAM region, byte-addressed starting at address 0. Same total capacity
	// as the old 64K-word array (256 KiB), just no longer word-truncated.
	// NOTE: real hardware resets at PC=0xFF00_0000 with the PPU/ROM mapped
	// far above this RAM window; loading real cartridge images at their
	// real addresses is deferred to the system-integration work (loadROM()
	// here still places code at address 0 to match the existing CPU/ROM
	// test-harness convention).
	static constexpr uint32_t RAM_SIZE = 64u * 1024u * 4u;
	std::array<uint8_t, RAM_SIZE> ram;

	// Copy `count` big-endian instruction words into ram[] starting at
	// address 0 (byte address, i.e. word i lands at byte i*4).
	void loadROM(const uint32_t* words, size_t count);

public: // Bus read/write, byte-addressed across the full 32-bit space.
	// Every access is routed by address range to the device that owns it
	// (RAM, or the PPU's register window at PPU::BASE); reads outside any
	// mapped region return 0 and writes are ignored, matching real
	// unmapped-bus behaviour rather than silently aliasing into RAM.
	uint8_t  read8(uint32_t addr) const;
	void     write8(uint32_t addr, uint8_t data);
	uint16_t read16(uint32_t addr) const;
	void     write16(uint32_t addr, uint16_t data);
	uint32_t read32(uint32_t addr) const;
	void     write32(uint32_t addr, uint32_t data);
};
