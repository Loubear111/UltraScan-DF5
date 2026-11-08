#pragma once
#include <cstdint>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "spg290.h"
#include "ppu.h"
#include "IRQController.h"
#include "TVE.h"
#include "Timer.h"
#include "I2C.h"
#include "CdServo.h"
#include "VideoTiming.h"

class Bus
{
public:
	// Default RAM size: the original 64K-word (256 KiB) window.
	static constexpr uint32_t DEFAULT_RAM_SIZE = 256u * 1024u;

	// Size of the real machine's RAM (System uses this).
	static constexpr uint32_t SYSTEM_RAM_SIZE = 16u * 1024u * 1024u;

	// RAM answers every physical address below this limit, mirrored every
	// ram.size() bytes (MAME: 16 MiB mirrored through 0x0800_0000).
	static constexpr uint32_t RAM_LIMIT = 0x08000000u;

	// Internal SRAM, 16 KiB at physical 0x0A00_0000 [MAME].
	static constexpr uint32_t SRAM_PHYS = 0x0A000000u;
	static constexpr uint32_t SRAM_SIZE = 16u * 1024u;

	// Small fixed devices [MAME]: the MIU status register reads 1; a 16-byte
	// window at 0x0824_0000 reads 0 and ignores writes.
	static constexpr uint32_t MIU_STATUS  = 0x0807006Cu;
	static constexpr uint32_t DUMMY_PHYS  = 0x08240000u;
	static constexpr uint32_t DUMMY_SIZE  = 0x10u;

	// UART transmit register: the BIOS prints its boot messages by writing characters
	// here (MAME has it commented out as a printf). Only this one word is modelled; the
	// low byte of every write is appended to uartOutput.
	static constexpr uint32_t UART_TX = 0x08150000u;
	std::string uartOutput;

	// Read-only ROM window (physical 0x1000_0000, alias 0x9000_0000/0xB000_0000)
	// meant for a future BIOS image (MAME places the BIOS here). Reads beyond
	// the loaded image return 0; writes are ignored.
	static constexpr uint32_t ROM_PHYS   = 0x10000000u;
	static constexpr uint32_t ROM_WINDOW = 0x01000000u;   // 16 MiB

	// With a BIOS booted (System::bootBios) the image repeats every romMirror bytes
	// through 0x1000_0000-0x1FFF_FFFF. The BIOS reads its own header through
	// 0x1F00_0040 and copies its code from 0x1E00_1000, so it is mapped more than
	// once [inferred from the BIOS code; MAME's exact mirror is unverified].
	static constexpr uint32_t ROM_MIRROR_END = 0x20000000u;
	uint32_t romMirror = 0;   // 0 = no mirroring (a loaded image fills the 16 MiB window once)

	// `ramBytes` must be a power of two (RAM is mirrored by masking).
	explicit Bus(uint32_t ramBytes = DEFAULT_RAM_SIZE);
	~Bus();

public: // Devices on bus
	spg290 cpu;

	// 2D Picture Process Unit (memory mapped at PPU::BASE = 0x8801_0000).
	PPU ppu;

	// Peripheral interrupt controller (IRQController::BASE = 0x880A_0000).
	IRQController irqc;

	// TV encoder / framebuffer scan-out registers (see TVE.h for the addresses).
	TVE tve;

	// Timer block at 0x0816_0000 (see Timer.h; inferred from the BIOS).
	TimerBlock timers;

	// I2C master and the HyperScan controller ports at 0x0813_0000 (see I2C.h).
	I2CMaster i2c;

	// CD servo and sector transfer engine at 0x0806_0000 (see CdServo.h).
	CdServo cdservo;

	// Scanline counter that drives the PPU vblank / H-V events.
	VideoTiming video;

	// RAM region, byte-addressed starting at address 0 and mirrored through
	// the whole physical range below RAM_LIMIT (index = addr & (size - 1)).
	// Loading real images at their real addresses is done with
	// loadImage()/loadFile(). loadROM() still places code at address 0 to
	// match the existing CPU/ROM test-harness convention.
	std::vector<uint8_t> ram;

	// Internal SRAM (SRAM_PHYS, SRAM_SIZE bytes).
	std::vector<uint8_t> sram;

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
	// device that owns it (RAM, SRAM, ROM window, PPU, IRQ controller, MIU
	// status); reads outside any mapped region return 0, writes are ignored,
	// and both are counted in the unmapped-access log.
	uint8_t  read8(uint32_t addr) const;
	void     write8(uint32_t addr, uint8_t data);
	uint16_t read16(uint32_t addr) const;
	void     write16(uint32_t addr, uint16_t data);
	uint32_t read32(uint32_t addr) const;
	void     write32(uint32_t addr, uint32_t data);

	// Re-evaluate every interrupt source and drive the CPU's IRQ input. Called
	// automatically after MMIO writes; call it after raising a PPU event hook.
	void updateIRQ();

	// Advance video time by `cpuCycles` CPU cycles and raise the PPU vblank /
	// H-V events that fall inside. Runs even while the CPU sleeps, so a sleeping
	// core can be woken by an interrupt.
	void tick(uint32_t cpuCycles);

	// True once per vblank start; reading it clears it. The driver renders a
	// frame when it is set.
	bool takeFrameReady() { bool r = frameReady; frameReady = false; return r; }
	bool frameReady = false;

	// Unmapped-access log: reads and writes per 4 KiB physical page that no
	// region owns. Shows which I/O a program touches that is not modelled yet.
	struct UnmappedPage
	{
		uint32_t page;      // physical address of the page start
		uint64_t reads;
		uint64_t writes;
	};
	// The n busiest pages, most accesses first (ties by address).
	std::vector<UnmappedPage> topUnmappedPages(size_t n = 10) const;
	// One line per page from topUnmappedPages(), for the end-of-run report.
	std::string unmappedSummary(size_t n = 10) const;
	void clearUnmappedLog() { unmapped.clear(); ioLog.clear(); }

	// Optional detail log of unmapped accesses, one entry each (address, size, the value
	// written, the PC of the instruction), kept up to ioLogLimit entries; 0 = off.
	struct IoAccess { uint32_t addr; uint32_t value; uint8_t size; bool write; uint32_t pc; };
	size_t ioLogLimit = 0;
	mutable std::vector<IoAccess> ioLog;

private:
	static constexpr uint32_t PHYS_MASK = 0x1FFFFFFFu;
	static constexpr uint32_t PPU_PHYS  = PPU::BASE & PHYS_MASK;
	static constexpr uint32_t IRQC_PHYS = IRQController::BASE & PHYS_MASK;

	bool little = false;

	static bool inPPU(uint32_t p)  { return p >= PPU_PHYS  && p < PPU_PHYS  + 0x10000u; }
	static bool inIRQC(uint32_t p) { return p >= IRQC_PHYS && p < IRQC_PHYS + IRQController::WINDOW; }
	bool inRom(uint32_t p) const   { return p >= ROM_PHYS  && p < (romMirror ? ROM_MIRROR_END : ROM_PHYS + ROM_WINDOW); }
	static bool inTve(uint32_t p)   { return TVE::owns(p & ~3u); }
	static bool inTimer(uint32_t p) { return TimerBlock::owns(p & ~3u); }
	static bool inUart(uint32_t p)   { return (p & ~3u) == UART_TX; }
	static bool inI2C(uint32_t p)    { return I2CMaster::owns(p & ~3u); }
	static bool inCdServo(uint32_t p) { return CdServo::owns(p & ~3u); }
	static bool inMiu(uint32_t p)   { return p >= (MIU_STATUS & ~3u) && p < (MIU_STATUS & ~3u) + 4u; }
	static bool inDummy(uint32_t p) { return p >= DUMMY_PHYS && p < DUMMY_PHYS + DUMMY_SIZE; }

	enum class Region { Ram, Sram, Rom, Device, None };
	Region   classify(uint32_t p) const;
	uint8_t  memByte(Region r, uint32_t p) const;
	void     memStore(Region r, uint32_t p, uint8_t v);
	uint32_t memRead(Region r, uint32_t p, unsigned span) const;
	void     memWrite(Region r, uint32_t p, uint32_t v, unsigned span);

	struct UnmappedCount { uint64_t reads = 0; uint64_t writes = 0; };
	mutable std::map<uint32_t, UnmappedCount> unmapped;   // key = physical page number
	bool m_timerIrq = false;
	bool m_i2cIrq = false;                               // last timer line level pushed to the IRQ controller
	void logUnmapped(uint32_t p, bool write, uint32_t value, uint8_t size) const;

	// Byte of the ROM window; 0 beyond the loaded image.
	uint8_t romByte(uint32_t p) const
	{
		uint32_t o = p - ROM_PHYS;
		if (romMirror)
			o %= romMirror;
		return o < rom.size() ? rom[o] : 0;
	}

	uint32_t mmioRead32(uint32_t phys) const;
	void     mmioWrite32(uint32_t phys, uint32_t data);
	// Sub-word MMIO write: read-modify-write, except for write-1-to-act
	// registers where only the written lane may carry bits.
	void     mmioWriteLane(uint32_t phys, uint32_t data, uint32_t laneMask, unsigned shift);
	static bool isLaneOnlyReg(uint32_t phys);
};

// PPUMemory view of the bus. The PPU's byte order is fixed (little-endian, PPU
// manual 9.1), so halfwords and words are composed from byte reads whatever the
// Bus byte order is; the CPU/Bus byte-order question stays inside the Bus.
class BusMemory : public PPUMemory
{
public:
	explicit BusMemory(const Bus& b) : bus(b) {}
	uint8_t  readByte(uint32_t a) const override { return bus.read8(a); }
	uint16_t readHalf(uint32_t a) const override { return (uint16_t)(readByte(a) | (readByte(a + 1) << 8)); }
	uint32_t readWord(uint32_t a) const override { return (uint32_t)readHalf(a) | ((uint32_t)readHalf(a + 2) << 16); }

private:
	const Bus& bus;
};
