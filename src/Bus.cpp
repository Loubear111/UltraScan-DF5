#include "Bus.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

Bus::Bus(uint32_t ramBytes)
{
	cpu.ConnectBus(this);
	// Only touch the CPU's IRQ line when a pulsed source was cleared; tests (and anything
	// else) that drive the line directly are left alone.
	cpu.onIrqTaken = [this](uint8_t vector) { if (irqc.acknowledge(vector)) updateIRQ(); };
	cdservo.setMemoryWriter([this](uint32_t addr, uint8_t value) { write8(addr, value); });

	// RAM is mirrored by masking, so its size must be a power of two.
	assert(ramBytes != 0 && (ramBytes & (ramBytes - 1)) == 0);

	// Zero-filled RAM of the requested size and the internal SRAM.
	ram.assign(ramBytes, 0);
	sram.assign(SRAM_SIZE, 0);
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

	if (p < RAM_LIMIT)
	{
		// Mirrored RAM: the image must fit between the mirrored start and the end of RAM.
		const uint64_t idx = p & (ram.size() - 1);
		if (size > ram.size() - idx)
			return false;
		std::memcpy(&ram[(size_t)idx], data, size);
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

// ---- Memory regions ------------------------------------------------------
Bus::Region Bus::classify(uint32_t p) const
{
	if (p < RAM_LIMIT)                                  return Region::Ram;
	if (p >= SRAM_PHYS && p < SRAM_PHYS + SRAM_SIZE)    return Region::Sram;
	if (inRom(p))                                       return Region::Rom;
	if (inPPU(p) || inIRQC(p) || inTve(p) || inTimer(p) || inUart(p) || inI2C(p) || inCdServo(p) || inMiu(p) || inDummy(p)) return Region::Device;
	return Region::None;
}

uint8_t Bus::memByte(Region r, uint32_t p) const
{
	switch (r)
	{
	case Region::Ram:  return ram[p & (ram.size() - 1)];
	case Region::Sram: return sram[(p - SRAM_PHYS) & (SRAM_SIZE - 1)];
	case Region::Rom:  return romByte(p);
	default:           return 0;
	}
}

void Bus::memStore(Region r, uint32_t p, uint8_t v)
{
	if (r == Region::Ram)       ram[p & (ram.size() - 1)] = v;
	else if (r == Region::Sram) sram[(p - SRAM_PHYS) & (SRAM_SIZE - 1)] = v;
	// ROM is read-only.
}

// Read `span` bytes (2 or 4) from a memory region in the current byte order.
uint32_t Bus::memRead(Region r, uint32_t p, unsigned span) const
{
	uint8_t b[4] = { 0, 0, 0, 0 };
	if (r == Region::Ram)
	{
		const size_t idx = p & (ram.size() - 1);
		if (idx + span <= ram.size())
			return span == 4 ? get32(&ram[idx], little) : get16(&ram[idx], little);
	}
	for (unsigned i = 0; i < span; ++i)
		b[i] = memByte(r, p + i);
	return span == 4 ? get32(b, little) : get16(b, little);
}

void Bus::memWrite(Region r, uint32_t p, uint32_t v, unsigned span)
{
	if (r == Region::Ram)
	{
		const size_t idx = p & (ram.size() - 1);
		if (idx + span <= ram.size())
		{
			if (span == 4) put32(&ram[idx], v, little);
			else           put16(&ram[idx], (uint16_t)v, little);
			return;
		}
	}
	uint8_t b[4];
	if (span == 4) put32(b, v, little);
	else           put16(b, (uint16_t)v, little);
	for (unsigned i = 0; i < span; ++i)
		memStore(r, p + i, b[i]);
}

// ---- Unmapped-access log -------------------------------------------------
void Bus::logUnmapped(uint32_t p, bool write, uint32_t value, uint8_t size) const
{
	UnmappedCount& c = unmapped[p >> 12];
	if (write) ++c.writes; else ++c.reads;
	if (ioLogLimit && ioLog.size() < ioLogLimit)
		ioLog.push_back({ p, value, size, write, cpu.executingAddr() });
}

std::vector<Bus::UnmappedPage> Bus::topUnmappedPages(size_t n) const
{
	std::vector<UnmappedPage> v;
	v.reserve(unmapped.size());
	for (const auto& kv : unmapped)
		v.push_back({ kv.first << 12, kv.second.reads, kv.second.writes });
	std::sort(v.begin(), v.end(), [](const UnmappedPage& a, const UnmappedPage& b) {
		const uint64_t ta = a.reads + a.writes, tb = b.reads + b.writes;
		return ta != tb ? ta > tb : a.page < b.page;
	});
	if (v.size() > n)
		v.resize(n);
	return v;
}

std::string Bus::unmappedSummary(size_t n) const
{
	std::string out;
	char line[96];
	for (const UnmappedPage& u : topUnmappedPages(n))
	{
		std::snprintf(line, sizeof(line), "  0x%08X  reads %llu  writes %llu\n", u.page,
		              (unsigned long long)u.reads, (unsigned long long)u.writes);
		out += line;
	}
	return out;
}

// ---- MMIO word access (devices are word-granular) ------------------------
uint32_t Bus::mmioRead32(uint32_t phys) const
{
	syncForAccess();   // device state must include all the time that has passed
	if (inPPU(phys))
		return ppu.read32(PPU::BASE + (phys - PPU_PHYS));
	if (inIRQC(phys))
		return irqc.read32(phys - IRQC_PHYS);
	if (TVE::owns(phys))
		return tve.read32(phys);
	if (TimerBlock::owns(phys))
		return timers.read32(phys);
	if (I2CMaster::owns(phys))
		return i2c.read32(phys);
	if (CdServo::owns(phys))
		return cdservo.read32(phys);
	if (phys == MIU_STATUS)
		return 1;
	return 0;   // the small dummy window reads 0
}

void Bus::mmioWrite32(uint32_t phys, uint32_t data)
{
	syncForAccess();
	if (inPPU(phys))
		ppu.write32(PPU::BASE + (phys - PPU_PHYS), data);
	else if (inIRQC(phys))
		irqc.write32(phys - IRQC_PHYS, data);
	else if (TVE::owns(phys))
		tve.write32(phys, data);
	else if (TimerBlock::owns(phys))
		timers.write32(phys, data);
	else if (inUart(phys))
		uartOutput.push_back((char)(data & 0xFF));
	else if (I2CMaster::owns(phys))
		i2c.write32(phys, data);
	else if (CdServo::owns(phys))
		cdservo.write32(phys, data);
	else
		return;   // MIU status and the dummy window ignore writes
	updateIRQ();
	recomputeDeadline();   // the write may have started, stopped or retimed a device
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
	// The cached levels must follow every push to the controller: an acknowledge write
	// clears a device line here, and a stale "high" would hide the next rising edge from
	// applyTime() until some later tick happened to refresh it.
	m_timerIrq = timers.irq();
	m_i2cIrq   = i2c.irq();
	irqc.setSource(IRQController::SRC_PPU_VBLK_START, ppu.blkStartLine());
	irqc.setSource(IRQController::SRC_PPU_HV_HIT,     ppu.hvLine());
	irqc.setSource(IRQController::SRC_VBLK_END,       ppu.blkEndLine());
	irqc.setSource(IRQController::SRC_TIMER,          timers.irq());
	irqc.setSource(IRQController::SRC_I2C,            i2c.irq());
	cpu.setIRQ(irqc.irqActive(), irqc.activeVector());
}

void Bus::tick(uint32_t cpuCycles)
{
	m_pending += cpuCycles;
	sync();
}

void Bus::sync()
{
	if (m_pending)
	{
		const uint32_t c = (uint32_t)m_pending;   // never more than a line's worth plus one instruction
		m_pending = 0;
		applyTime(c);
	}
	recomputeDeadline();
}

void Bus::recomputeDeadline()
{
	uint64_t d = video.cyclesToEvent(ppu.read32(PPU::P_IRQTMV), ppu.read32(PPU::P_IRQTMH));
	const uint64_t t = timers.cyclesToEvent();
	if (t < d) d = t;
	const uint64_t i = i2c.cyclesToEvent();
	if (i < d) d = i;
	const uint64_t c = cdservo.cyclesToEvent();
	if (c < d) d = c;
	m_deadline = d ? d : 1;
}

void Bus::applyTime(uint32_t cpuCycles)
{
	timers.tick(cpuCycles);
	i2c.tick(cpuCycles);
	cdservo.tick(cpuCycles);
	if (cdservo.takeIrq())
	{
		irqc.pulse(IRQController::SRC_CDSERVO);
		updateIRQ();
	}
	if (timers.irq() != m_timerIrq || i2c.irq() != m_i2cIrq)
	{
		m_timerIrq = timers.irq();
		m_i2cIrq = i2c.irq();
		updateIRQ();
	}
	const uint32_t ev = video.advance(cpuCycles, ppu.read32(PPU::P_IRQTMV), ppu.read32(PPU::P_IRQTMH),
	                                  ppu.read32(PPU::P_VBLK_TIME));
	if (!ev)
		return;
	if (ev & VideoTiming::EV_VBLANK_START) { ppu.signalVBlankStart(); frameReady = true; }
	if (ev & VideoTiming::EV_VBLANK_END)   ppu.signalVBlankEnd();
	if (ev & VideoTiming::EV_HV_HIT)       ppu.signalHVHit();
	updateIRQ();
}

// ---- Public accessors (byte order selected by setLittleEndian()) ---------
uint8_t Bus::read8(uint32_t addr) const
{
	uint32_t p = addr & PHYS_MASK;
	Region r = classify(p);
	if (r == Region::None) { logUnmapped(p, false, 0, 1); return 0; }
	if (r != Region::Device)
		return memByte(r, p);

	uint32_t word  = mmioRead32(p & ~3u);
	uint8_t  shift = little ? (uint8_t)(8u * (p & 3u)) : (uint8_t)(8u * (3u - (p & 3u)));
	return (uint8_t)(word >> shift);
}

void Bus::write8(uint32_t addr, uint8_t data)
{
	uint32_t p = addr & PHYS_MASK;
	Region r = classify(p);
	if (r == Region::None) { logUnmapped(p, true, data, 1); return; }
	if (r != Region::Device) { memStore(r, p, data); return; }

	mmioWriteLane(p, data, 0xFFu, little ? 8u * (p & 3u) : 8u * (3u - (p & 3u)));
}

uint16_t Bus::read16(uint32_t addr) const
{
	uint32_t p = addr & PHYS_MASK;
	Region r = classify(p);
	if (r == Region::None) { logUnmapped(p, false, 0, 2); return 0; }
	if (r != Region::Device)
		return (uint16_t)memRead(r, p, 2);

	uint32_t word  = mmioRead32(p & ~3u);
	uint8_t  shift = little ? ((p & 2u) ? 16u : 0u) : ((p & 2u) ? 0u : 16u);
	return (uint16_t)(word >> shift);
}

void Bus::write16(uint32_t addr, uint16_t data)
{
	uint32_t p = addr & PHYS_MASK;
	Region r = classify(p);
	if (r == Region::None) { logUnmapped(p, true, data, 2); return; }
	if (r != Region::Device) { memWrite(r, p, data, 2); return; }

	mmioWriteLane(p, data, 0xFFFFu, little ? ((p & 2u) ? 16u : 0u) : ((p & 2u) ? 0u : 16u));
}

uint32_t Bus::read32(uint32_t addr) const
{
	uint32_t p = addr & PHYS_MASK;
	Region r = classify(p);
	if (r == Region::None) { logUnmapped(p, false, 0, 4); return 0; }
	if (r != Region::Device)
		return memRead(r, p, 4);

	return mmioRead32(p);
}

void Bus::write32(uint32_t addr, uint32_t data)
{
	uint32_t p = addr & PHYS_MASK;
	Region r = classify(p);
	if (r == Region::None) { logUnmapped(p, true, data, 4); return; }
	if (r != Region::Device) { memWrite(r, p, data, 4); return; }

	mmioWrite32(p, data);
}
