#pragma once
#include <cstdint>
#include <functional>
#include <vector>

class DiscImage;

// ============================================================================
// SPG290 CD servo at 0x0806_0000 (Phase 5, story 24). A high-level model of the
// servo DSP and the sector transfer engine, following MAME's spg290_cdservo_device
// (BSD-3-Clause, Sandro Ronco) register by register. It exposes the disc as a
// stream of raw 2352-byte sectors written into a RAM ring buffer, plus the Q
// subchannel table (lead-in TOC, program area, lead-out) the BIOS reads through
// the DSP command interface.
//
//   +0x04 DSP command (address)   +0x08 DSP data   +0x0C exec (bit 0: 1 = read, 0 = write)
//   +0x40 control 0 (bit 15: CDDA)  +0x44 control 1 (a write seeks; bit 2 pauses transfers)
//   +0x48/4C/50 seek minute/second/frame (BCD)   +0x54 CRC flags (reads 0)
//   +0x60 buffer start  +0x64 buffer end  +0x68 buffer pointer  +0x6C sector size
// One sector is delivered per 1/(75 * speed) s once a seek has reached its target,
// and each delivery raises the interrupt (line 60 in MAME), which has to be taken
// as an edge: takeIrq() returns true once per delivered sector.
// The lead-in is 7500 sectors and the lead-out 6750, as in MAME. The disc is taken
// to hold one data track starting at LBA 0 with no pregap (true for the images the
// DiscImage reader accepts).
// ============================================================================
class CdServo
{
public:
	static constexpr uint32_t BASE   = 0x08060000u;   // physical
	static constexpr uint32_t WINDOW = 0x10000u;
	static constexpr uint32_t LEADIN_LEN  = 7500;
	static constexpr uint32_t LEADOUT_LEN = 6750;
	static constexpr uint32_t MAX_SECTOR  = 2448;   // raw sector plus subcode space
	static constexpr uint32_t CPU_HZ = 108000000u;

	using ByteWriter = std::function<void(uint32_t addr, uint8_t value)>;

	CdServo() { reset(); }

	// Where delivered sector bytes go (MAME: the CPU's address space).
	void setMemoryWriter(ByteWriter w) { m_write = std::move(w); }

	// The disc, not owned; may be null (no disc). Rebuilds the Q subchannel table.
	void attachDisc(DiscImage* disc);
	bool hasDisc() const { return m_disc != nullptr; }

	void reset();

	static bool owns(uint32_t physWord) { return physWord >= BASE && physWord < BASE + WINDOW; }
	uint32_t read32(uint32_t physWord) const;
	void     write32(uint32_t physWord, uint32_t data);

	void tick(uint32_t cpuCycles);

	// True once for every sector delivered since the last call (an interrupt edge).
	bool takeIrq() { const bool r = m_irqEdge; m_irqEdge = false; return r; }

	// CPU cycles until the next sector step (~0 when the drive is stopped or empty). Steps
	// are the only time-driven changes, and tick() calls compose.
	uint64_t cyclesToEvent() const
	{
		if (!m_timerOn)
			return ~0ull;
		const uint64_t p = periodCycles();
		return p > m_acc ? p - m_acc : 1;
	}

	// Introspection for tests.
	uint32_t bufferPointer() const { return m_bufPtr; }
	uint32_t seekLba() const { return m_seekLba; }
	int64_t  currentSector() const { return m_curSector; }
	uint8_t  qsubByte(uint32_t sector, unsigned i) const { return m_qsub.size() > (size_t)sector * 12 + i ? m_qsub[(size_t)sector * 12 + i] : 0; }
	uint32_t totalSectors() const { return m_totSectors; }

private:
	void generateQsub();
	void addQsub(uint32_t sector, uint8_t addrctrl, uint8_t track, uint8_t index, uint32_t relMsf, uint32_t absMsf);
	void servoRead();
	void servoWrite();
	void changeStatus();
	void step();
	uint64_t periodCycles() const;

	DiscImage* m_disc = nullptr;
	ByteWriter m_write;

	uint32_t m_addr = 0, m_data = 0;
	uint32_t m_bufStart = 0, m_bufEnd = 0, m_bufPtr = 0;
	uint8_t  m_speed = 1;
	uint8_t  m_seekMin = 0, m_seekSec = 0, m_seekFrm = 0;
	uint32_t m_seekLba = 0, m_sectorSize = 0, m_dspData = 0;
	bool     m_frameFound = false;
	uint32_t m_control0 = 0, m_control1 = 0;
	uint16_t m_skip = 0;
	uint32_t m_dspRegs[0x10];
	std::vector<uint32_t> m_dspMemory;
	std::vector<uint8_t>  m_qsub;
	uint32_t m_totSectors = 0;
	int64_t  m_curSector = 0;

	bool     m_timerOn = false;
	uint64_t m_acc = 0;
	bool     m_irqEdge = false;
	uint32_t m_rand = 12345;
};
