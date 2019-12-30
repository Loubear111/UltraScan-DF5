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
	if (addr >= 0x0000 && addr <= 0xFFFF)
		return ram[addr];

	return 0x00;
}