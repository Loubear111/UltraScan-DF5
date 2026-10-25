#include "Bus.h"

#include <cstring>
#include <fstream>
#include <iterator>

Bus::Bus(uint32_t ramBytes)
{
	cpu.ConnectBus(this);

	// Zero-filled RAM of the requested size.
	ram.assign(ramBytes, 0);
}

Bus::~Bus()
{
}

void Bus::loadROM(const uint32_t* words, size_t count)
{
	for (size_t i = 0; i < count; ++i)
		write32((uint32_t)(i * 4), words[i]);
}

bool Bus::loadImage(const uint8_t* data, size_t size, uint32_t addr)
{
	if (size == 0)
		return true;
	if (!data)
		return false;

	const uint64_t p = addr & PHYS_MASK;

	if (p < ram.size() && size <= ram.size() - p)
	{
		std::memcpy(&ram[(size_t)p], data, size);
		return true;
	}

	const uint64_t romEnd = (uint64_t)ROM_PHYS + ROM_WINDOW;
	if (p >= ROM_PHYS && p < romEnd && size <= romEnd - p)
	{
		const size_t off = (size_t)(p - ROM_PHYS);
		if (rom.size() < off + size)
			rom.resize(off + size, 0);
		std::memcpy(&rom[off], data, size);
		return true;
	}

	return false;
}

bool Bus::loadFile(const std::string& path, uint32_t addr)
{
	std::ifstream f(path, std::ios::binary);
	if (!f)
		return false;

	std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (data.empty())
		return false;

	return loadImage(data.data(), data.size(), addr);
}

// ---- Byte-order helpers for RAM / ROM storage ----------------------------
static uint16_t get16(const uint8_t* b, bool le)
{
	return le ? (uint16_t)(b[0] | (b[1] << 8))
	          : (uint16_t)((b[0] << 8) | b[1]);
}

static uint32_t get32(const uint8_t* b, bool le)
{
	return le ? ((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24))
	          : (((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3]);
}

static void put16(uint8_t* b, uint16_t v, bool le)
{
	if (le) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }
	else    { b[0] = (uint8_t)(v >> 8); b[1] = (uint8_t)v; }
}

static void put32(uint8_t* b, uint32_t v, bool le)
{
	if (le)
	{
		b[0] = (uint8_t)v;         b[1] = (uint8_t)(v >> 8);
		b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
	}
	else
	{
		b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16);
		b[2] = (uint8_t)(v >> 8);  b[3] = (uint8_t)v;
	}
}

// ---- MMIO word access (devices are word-granular) ------------------------
uint32_t Bus::mmioRead32(uint32_t phys) const
{
	if (inPPU(phys))
		return ppu.read32(PPU::BASE + (phys - PPU_PHYS));
	if (inIRQC(phys))
		return irqc.read32(phys - IRQC_PHYS);
	return 0;
}

void Bus::mmioWrite32(uint32_t phys, uint32_t data)
{
	if (inPPU(phys))
		ppu.write32(PPU::BASE + (phys - PPU_PHYS), data);
	else if (inIRQC(phys))
		irqc.write32(phys - IRQC_PHYS, data);
	else
		return;
	updateIRQ();
}

bool Bus::isLaneOnlyReg(uint32_t phys)
{
	return phys == PPU_PHYS + PPU::P_IRQ_Status
	    || phys == IRQC_PHYS + IRQController::P_I_GPUSWI;
}

void Bus::mmioWriteLane(uint32_t phys, uint32_t data, uint32_t laneMask, unsigned shift)
{
	uint32_t wordAddr = phys & ~3u;
	uint32_t word = isLaneOnlyReg(wordAddr) ? 0u : mmioRead32(wordAddr);
	word = (word & ~(laneMask << shift)) | ((data & laneMask) << shift);
	mmioWrite32(wordAddr, word);
}

void Bus::updateIRQ()
{
	irqc.setSource(IRQController::SRC_PPU_VBLK_START, ppu.blkStartLine());
	irqc.setSource(IRQController::SRC_PPU_HV_HIT,     ppu.hvLine());
	irqc.setSource(IRQController::SRC_VBLK_END,       ppu.blkEndLine());
	cpu.setIRQ(irqc.irqActive(), irqc.activeVector());
}

// ---- Public accessors (byte order selected by setLittleEndian()) ---------
uint8_t Bus::read8(uint32_t addr) const
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 1))
		return ram[p];
	if (inRom(p))
		return romByte(p);

	uint32_t word  = mmioRead32(p & ~3u);
	uint8_t  shift = little ? (uint8_t)(8u * (p & 3u)) : (uint8_t)(8u * (3u - (p & 3u)));
	return (uint8_t)(word >> shift);
}

void Bus::write8(uint32_t addr, uint8_t data)
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 1)) { ram[p] = data; return; }
	if (inRom(p)) return;

	mmioWriteLane(p, data, 0xFFu, little ? 8u * (p & 3u) : 8u * (3u - (p & 3u)));
}

uint16_t Bus::read16(uint32_t addr) const
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 2))
		return get16(&ram[p], little);
	if (inRom(p))
	{
		uint8_t b[2] = { romByte(p), romByte(p + 1) };
		return get16(b, little);
	}

	uint32_t word  = mmioRead32(p & ~3u);
	uint8_t  shift = little ? ((p & 2u) ? 16u : 0u) : ((p & 2u) ? 0u : 16u);
	return (uint16_t)(word >> shift);
}

void Bus::write16(uint32_t addr, uint16_t data)
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 2)) { put16(&ram[p], data, little); return; }
	if (inRom(p)) return;

	mmioWriteLane(p, data, 0xFFFFu, little ? ((p & 2u) ? 16u : 0u) : ((p & 2u) ? 0u : 16u));
}

uint32_t Bus::read32(uint32_t addr) const
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 4))
		return get32(&ram[p], little);
	if (inRom(p))
	{
		uint8_t b[4] = { romByte(p), romByte(p + 1), romByte(p + 2), romByte(p + 3) };
		return get32(b, little);
	}

	return mmioRead32(p);
}

void Bus::write32(uint32_t addr, uint32_t data)
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 4)) { put32(&ram[p], data, little); return; }
	if (inRom(p)) return;

	mmioWrite32(p, data);
}
