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

void Bus::write(uint16_t addr, uint32_t data)
{
	if (addr >= 0x0000 && addr <= 0xFFFF)
		ram[addr] = data;
}

uint32_t Bus::read(uint16_t addr, bool bReadOnly)
{
	(void)bReadOnly;
	if (addr >= 0x0000 && addr <= 0xFFFF)
		return ram[addr];

	return 0x00;
}

// Route a full 32-bit device address. The PPU register block occupies a
// 16K-byte window starting at PPU::BASE.
void Bus::write32(uint32_t addr, uint32_t data)
{
	if (addr >= PPU::BASE && addr < PPU::BASE + 0x10000)
		ppu.write32(addr, data);
}

uint32_t Bus::read32(uint32_t addr)
{
	if (addr >= PPU::BASE && addr < PPU::BASE + 0x10000)
		return ppu.read32(addr);
	return 0x00;
}