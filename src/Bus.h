#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "spg290.h"
#include "ppu.h"
#include "IRQController.h"

class Bus
{
public:
	// Default RAM size: the original 64K-word (256 KiB) window.
	static constexpr uint32_t DEFAULT_RAM_SIZE = 256u * 1024u;

	// Read-only ROM window (physical 0x1F00_0000, alias 0xBF00_0000) meant for a
	// future BIOS image. Reads beyond the loaded image return 0; writes are ignored.
	static constexpr uint32_t ROM_PHYS   = 0x1F000000u;
	static constexpr uint32_t ROM_WINDOW = 0x01000000u;   // 16 MiB

	// `ramBytes` must stay below the PPU window (0x0801_0000 physical) or it
	// would shadow the PPU registers.
	explicit Bus(uint32_t ramBytes = DEFAULT_RAM_SIZE);
	~Bus();

public: // Devices on bus
	spg290 cpu;

	// 2D Picture Process Unit (memory mapped at PPU::BASE = 0x8801_0000).
	PPU ppu;

	// Peripheral interrupt controller (IRQController::BASE = 0x880A_0000).
	IRQController irqc;

	// RAM region, byte-addressed starting at address 0.
	// NOTE: real hardware resets at PC=0xFF00_0000 with the PPU/ROM mapped
	// far above this RAM window; loading real cartridge images at their
	// real addresses is done with loadImage()/loadFile(). loadROM() still
	// places code at address 0 to match the existing CPU/ROM test-harness
	// convention.
	std::vector<uint8_t> ram;

	// Read-only ROM window contents (grown on demand by loadImage()).
	std::vector<uint8_t> rom;

	// Write `count` instruction words with write32() starting at address 0
	// (byte address, i.e. word i lands at byte i*4). The bytes land in the
	// Bus's current byte order (big-endian unless setLittleEndian(true)).
	void loadROM(const uint32_t* words, size_t count);

	// Copy raw bytes to a physical address (addr is masked with PHYS_MASK).
	// The bytes are stored in file order; endianness only matters when words
	// are read back. A RAM target is written directly (not through the CPU
	// write path); a ROM-window target grows `rom` as needed. Returns false
	// and changes nothing if the whole image does not fit in RAM or in the
	// ROM window.
	bool loadImage(const uint8_t* data, size_t size, uint32_t addr);

	// Read the whole file and loadImage() it. Returns false if the file cannot
	// be opened, is empty, or does not fit.
	bool loadFile(const std::string& path, uint32_t addr);

	// Byte order used to compose halfwords/words from bytes. Default is
	// big-endian (the Score7 toolchain default used by the test ROMs).
	// Real SPG290 images are little-endian.
	void setLittleEndian(bool le) { little = le; }
	bool isLittleEndian() const   { return little; }

public: // Bus read/write, byte-addressed across the full 32-bit space.
	// Addresses are reduced to a 29-bit physical address (addr & 0x1FFFFFFF),
	// so aliases such as 0xA000_0000 -> 0x0 and 0x8801_0000 -> 0x0801_0000
	// reach the same device. Every access is routed by physical range to the
	// device that owns it (RAM, ROM window, PPU, IRQ controller); reads outside
	// any mapped region return 0 and writes are ignored.
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

	bool little = false;

	static bool inPPU(uint32_t p)  { return p >= PPU_PHYS  && p < PPU_PHYS  + 0x10000u; }
	static bool inIRQC(uint32_t p) { return p >= IRQC_PHYS && p < IRQC_PHYS + IRQController::WINDOW; }
	static bool inRom(uint32_t p)  { return p >= ROM_PHYS  && p < ROM_PHYS  + ROM_WINDOW; }
	bool inRam(uint32_t p, uint32_t span) const { return p < ram.size() && span <= ram.size() - p; }

	// Byte of the ROM window; 0 beyond the loaded image.
	uint8_t romByte(uint32_t p) const { uint32_t o = p - ROM_PHYS; return o < rom.size() ? rom[o] : 0; }

	uint32_t mmioRead32(uint32_t phys) const;
	void     mmioWrite32(uint32_t phys, uint32_t data);
	// Sub-word MMIO write: read-modify-write, except for write-1-to-act
	// registers where only the written lane may carry bits.
	void     mmioWriteLane(uint32_t phys, uint32_t data, uint32_t laneMask, unsigned shift);
	static bool isLaneOnlyReg(uint32_t phys);
};
