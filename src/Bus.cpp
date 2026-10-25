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

// ---- Public accessors (big-endian lanes, matching the Score7 default) ----
uint8_t Bus::read8(uint32_t addr) const
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 1))
		return ram[p];

	uint32_t word  = mmioRead32(p & ~3u);
	uint8_t  shift = 8u * (3u - (p & 3u));
	return (uint8_t)(word >> shift);
}

void Bus::write8(uint32_t addr, uint8_t data)
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 1)) { ram[p] = data; return; }

	mmioWriteLane(p, data, 0xFFu, 8u * (3u - (p & 3u)));
}

uint16_t Bus::read16(uint32_t addr) const
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 2))
		return (uint16_t)(((uint16_t)ram[p] << 8) | ram[p + 1]);

	uint32_t word  = mmioRead32(p & ~3u);
	uint8_t  shift = (p & 2u) ? 0u : 16u;
	return (uint16_t)(word >> shift);
}

void Bus::write16(uint32_t addr, uint16_t data)
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 2)) { ram[p] = (uint8_t)(data >> 8); ram[p + 1] = (uint8_t)data; return; }

	mmioWriteLane(p, data, 0xFFFFu, (p & 2u) ? 0u : 16u);
}

uint32_t Bus::read32(uint32_t addr) const
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 4))
		return ((uint32_t)ram[p] << 24) | ((uint32_t)ram[p + 1] << 16)
		     | ((uint32_t)ram[p + 2] << 8) | (uint32_t)ram[p + 3];

	return mmioRead32(p);
}

void Bus::write32(uint32_t addr, uint32_t data)
{
	uint32_t p = addr & PHYS_MASK;
	if (inRam(p, 4))
	{
		ram[p]     = (uint8_t)(data >> 24);
		ram[p + 1] = (uint8_t)(data >> 16);
		ram[p + 2] = (uint8_t)(data >> 8);
		ram[p + 3] = (uint8_t)data;
		return;
	}

	mmioWrite32(p, data);
}
