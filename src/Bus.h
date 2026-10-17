#pragma once
#include <cstdint>
#include <array>

#include "spg290.h"
#include "ppu.h"
#include "IRQController.h"

class Bus
{
public:
	Bus();
	~Bus();

public: // Devices on bus
	spg290 cpu;

	// 2D Picture Process Unit (memory mapped at PPU::BASE = 0x8801_0000).
	PPU ppu;

	// Peripheral interrupt controller (IRQController::BASE = 0x880A_0000).
	IRQController irqc;

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
	// Addresses are reduced to a 29-bit physical address (addr & 0x1FFFFFFF),
	// so aliases such as 0xA000_0000 -> 0x0 and 0x8801_0000 -> 0x0801_0000
	// reach the same device. Every access is routed by physical range to the
	// device that owns it (RAM, PPU, IRQ controller); reads outside any mapped
	// region return 0 and writes are ignored.
	uint8_t  read8(uint32_t addr) const;
	void     write8(uint32_t addr, uint8_t data);
	uint16_t read16(uint32_t addr) const;
	void     write16(uint32_t addr, uint16_t data);
	uint32_t read32(uint32_t addr) const;
	void     write32(uint32_t addr, uint32_t data);

	// Re-evaluate every interrupt source and drive the CPU's IRQ input. Called
	// automatically after MMIO writes; call it after raising a PPU event hook.
	void updateIRQ();

private:
	static constexpr uint32_t PHYS_MASK = 0x1FFFFFFFu;
	static constexpr uint32_t PPU_PHYS  = PPU::BASE & PHYS_MASK;
	static constexpr uint32_t IRQC_PHYS = IRQController::BASE & PHYS_MASK;

	static bool inPPU(uint32_t p)  { return p >= PPU_PHYS  && p < PPU_PHYS  + 0x10000u; }
	static bool inIRQC(uint32_t p) { return p >= IRQC_PHYS && p < IRQC_PHYS + IRQController::WINDOW; }

	uint32_t mmioRead32(uint32_t phys) const;
	void     mmioWrite32(uint32_t phys, uint32_t data);
	// Sub-word MMIO write: read-modify-write, except for write-1-to-act
	// registers where only the written lane may carry bits.
	void     mmioWriteLane(uint32_t phys, uint32_t data, uint32_t laneMask, unsigned shift);
	static bool isLaneOnlyReg(uint32_t phys);
};
