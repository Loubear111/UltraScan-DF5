#include "Bus.h"

Bus::Bus()
{
	cpu.ConnectBus(this);

	// Clear RAM just to be sure!
	for (auto &i : ram) i = 0x00;
}

Bus::~Bus()
{
}

void Bus::loadROM(const uint32_t* words, size_t count)
{
	for (size_t i = 0; i < count; ++i)
		write32((uint32_t)(i * 4), words[i]);
}

// ---- RAM region helpers (big-endian, matching the Score7 default) --------
static bool inRam(uint32_t addr, uint32_t span)
{
	return addr <= Bus::RAM_SIZE - span;
}

uint8_t Bus::read8(uint32_t addr) const
{
	if (inRam(addr, 1))
		return ram[addr];

	if (addr >= PPU::BASE && addr < PPU::BASE + 0x10000)
	{
		uint32_t word  = ppu.read32(addr & ~3u);
		uint8_t  shift = 8u * (3u - (addr & 3u));   // big-endian byte lane
		return (uint8_t)(word >> shift);
	}
	return 0;
}

void Bus::write8(uint32_t addr, uint8_t data)
{
	if (inRam(addr, 1)) { ram[addr] = data; return; }

	if (addr >= PPU::BASE && addr < PPU::BASE + 0x10000)
	{
		uint32_t wordAddr = addr & ~3u;
		uint32_t word     = ppu.read32(wordAddr);
		uint8_t  shift    = 8u * (3u - (addr & 3u));
		word = (word & ~(0xFFu << shift)) | ((uint32_t)data << shift);
		ppu.write32(wordAddr, word);
	}
}

uint16_t Bus::read16(uint32_t addr) const
{
	if (inRam(addr, 2))
		return (uint16_t)(((uint16_t)ram[addr] << 8) | ram[addr + 1]);

	if (addr >= PPU::BASE && addr < PPU::BASE + 0x10000)
	{
		uint32_t word  = ppu.read32(addr & ~3u);
		uint8_t  shift = (addr & 2u) ? 0u : 16u;     // big-endian half-lane
		return (uint16_t)(word >> shift);
	}
	return 0;
}

void Bus::write16(uint32_t addr, uint16_t data)
{
	if (inRam(addr, 2)) { ram[addr] = (uint8_t)(data >> 8); ram[addr + 1] = (uint8_t)data; return; }

	if (addr >= PPU::BASE && addr < PPU::BASE + 0x10000)
	{
		uint32_t wordAddr = addr & ~3u;
		uint32_t word     = ppu.read32(wordAddr);
		uint8_t  shift    = (addr & 2u) ? 0u : 16u;
		word = (word & ~(0xFFFFu << shift)) | ((uint32_t)data << shift);
		ppu.write32(wordAddr, word);
	}
}

uint32_t Bus::read32(uint32_t addr) const
{
	if (inRam(addr, 4))
		return ((uint32_t)ram[addr] << 24) | ((uint32_t)ram[addr + 1] << 16)
		     | ((uint32_t)ram[addr + 2] << 8) | (uint32_t)ram[addr + 3];

	if (addr >= PPU::BASE && addr < PPU::BASE + 0x10000)
		return ppu.read32(addr);

	return 0;
}

void Bus::write32(uint32_t addr, uint32_t data)
{
	if (inRam(addr, 4))
	{
		ram[addr]     = (uint8_t)(data >> 24);
		ram[addr + 1] = (uint8_t)(data >> 16);
		ram[addr + 2] = (uint8_t)(data >> 8);
		ram[addr + 3] = (uint8_t)data;
		return;
	}

	if (addr >= PPU::BASE && addr < PPU::BASE + 0x10000)
		ppu.write32(addr, data);
}
