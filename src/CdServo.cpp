#include "CdServo.h"

#include <algorithm>

#include "DiscImage.h"

namespace {
uint8_t decToBcd(uint32_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
uint32_t bcdToDec(uint32_t v) { return ((v >> 4) & 0xF) * 10 + (v & 0xF); }

// A frame count as BCD minute:second:frame packed into 24 bits (MAME's lba_to_msf).
uint32_t lbaToMsf(uint32_t lba)
{
	const uint32_t m = lba / (75 * 60), s = (lba / 75) % 60, f = lba % 75;
	return ((uint32_t)decToBcd(m) << 16) | ((uint32_t)decToBcd(s) << 8) | decToBcd(f);
}
} // namespace

void CdServo::reset()
{
	m_addr = m_data = 0;
	m_bufStart = m_bufEnd = m_bufPtr = 0;
	m_speed = 1;
	m_seekMin = m_seekSec = m_seekFrm = 0;
	m_seekLba = m_sectorSize = m_dspData = 0;
	m_frameFound = false;
	m_control0 = m_control1 = 0;
	m_skip = 0;
	std::fill(std::begin(m_dspRegs), std::end(m_dspRegs), 0u);
	m_dspMemory.assign(0x10000, 0u);
	m_curSector = 0;
	m_acc = 0;
	m_irqEdge = false;
	generateQsub();
	changeStatus();
}

void CdServo::attachDisc(DiscImage* disc)
{
	m_disc = disc;
	generateQsub();
	changeStatus();
}

uint64_t CdServo::periodCycles() const
{
	return (uint64_t)CPU_HZ / (75ull * (m_speed ? m_speed : 1));
}

void CdServo::changeStatus()
{
	m_timerOn = m_speed != 0 && m_disc != nullptr;
	m_acc = 0;
}

void CdServo::addQsub(uint32_t sector, uint8_t addrctrl, uint8_t track, uint8_t index, uint32_t relMsf, uint32_t absMsf)
{
	uint8_t* d = &m_qsub[(size_t)sector * 12];
	d[0] = (uint8_t)(((addrctrl & 0x0F) << 4) | ((addrctrl & 0xF0) >> 4));
	d[1] = track;
	d[2] = index;
	d[3] = (uint8_t)(relMsf >> 16);
	d[4] = (uint8_t)(relMsf >> 8);
	d[5] = (uint8_t)relMsf;
	d[6] = 0;
	d[7] = (uint8_t)(absMsf >> 16);
	d[8] = (uint8_t)(absMsf >> 8);
	d[9] = (uint8_t)absMsf;
	d[10] = 0;
	d[11] = 0;
}

void CdServo::generateQsub()
{
	m_qsub.clear();
	m_totSectors = 0;
	if (!m_disc)
		return;

	const uint32_t numTracks = 1;
	const uint32_t frames = (uint32_t)m_disc->sectorCount();
	const uint32_t totalSectors = 0 + frames + 150;              // track start + frames + pregap
	m_totSectors = LEADIN_LEN + totalSectors + LEADOUT_LEN;
	m_qsub.assign((size_t)m_totSectors * 12, 0);

	uint32_t lba = 0;
	const uint8_t adrCtrl = 0x14;                                // data track
	// 7500 sectors of lead-in: A0, A1, A2 and the track entries, repeated
	for (uint32_t s = 0; s < LEADIN_LEN; s += numTracks + 3)
	{
		if (lba < LEADIN_LEN) addQsub(lba++, 0x14, 0, 0xA0, lbaToMsf(s + 0), 1u << 16);
		if (lba < LEADIN_LEN) addQsub(lba++, 0x14, 0, 0xA1, lbaToMsf(s + 1), numTracks << 16);
		if (lba < LEADIN_LEN) addQsub(lba++, 0x14, 0, 0xA2, lbaToMsf(s + 2), lbaToMsf(totalSectors));
		for (uint32_t t = 0; t < numTracks; ++t)
			if (lba < LEADIN_LEN)
				addQsub(lba++, adrCtrl, 0, decToBcd(t + 1), lbaToMsf(s + 3 + t), lbaToMsf(0 + 150));
	}
	// the data track: a 150-frame pregap (MAME adds it when the first track has none), then the frames
	const uint32_t trackStart = 0;
	for (uint32_t s = 0; s < 150; ++s)
		addQsub(lba++, adrCtrl, decToBcd(1), 0, lbaToMsf(s), lbaToMsf(trackStart + s));
	for (uint32_t s = 0; s < frames; ++s)
		addQsub(lba++, adrCtrl, decToBcd(1), 1, lbaToMsf(s), lbaToMsf(trackStart + 150 + s));
	// 6750 sectors of lead-out
	for (uint32_t s = 0; s < LEADOUT_LEN; ++s)
		addQsub(lba++, 0x14, 0xAA, 1, lbaToMsf(s), lbaToMsf(totalSectors + s));
}

void CdServo::step()
{
	if (!(m_control1 & 0x04) && m_curSector == (int64_t)m_seekLba + LEADIN_LEN)
	{
		// MAME copies sector-size bytes out of a 2448-byte, zero-filled buffer that holds the
		// 2352 raw bytes, so a larger sector size (the BIOS programs 2368) is padded with
		// zeros and the ring pointer advances by the full sector size.
		uint8_t buf[MAX_SECTOR] = { 0 };
		const int64_t lba = m_curSector - 150 - LEADIN_LEN;
		if (m_disc && lba >= 0)
			m_disc->readRawSector((uint32_t)lba, buf);               // a failed read leaves zeros
		const uint32_t n = (m_control0 & (1u << 15)) ? DiscImage::RAW_SECTOR : m_sectorSize;   // CDDA: whole sector
		for (uint32_t i = 0; i < n && i < MAX_SECTOR; ++i)
		{
			if (m_write)
				m_write(m_bufPtr, buf[i]);
			++m_bufPtr;
			if (m_bufPtr > m_bufEnd)
				m_bufPtr = m_bufStart;
		}
		++m_seekLba;
		m_frameFound = true;
		m_irqEdge = true;
	}
	if (m_curSector < (int64_t)m_seekLba + LEADIN_LEN && m_curSector < (int64_t)m_totSectors)
		++m_curSector;
}

void CdServo::tick(uint32_t cpuCycles)
{
	if (!m_timerOn)
		return;
	m_acc += cpuCycles;
	const uint64_t period = periodCycles();
	while (m_acc >= period)
	{
		m_acc -= period;
		step();
	}
}

uint32_t CdServo::read32(uint32_t physWord) const
{
	switch (physWord - BASE)
	{
	case 0x08: return m_data;            // DSP reply
	case 0x0C: return 0;                 // servo ready
	case 0x40: return m_control0;
	case 0x44: return m_control1;
	case 0x48: return m_seekMin;
	case 0x4C: return m_seekSec;
	case 0x50: return m_seekFrm;
	case 0x54: return 0;                 // sector CRC error flags
	case 0x60: return m_bufStart;
	case 0x64: return m_bufEnd;
	case 0x68: return m_bufPtr;
	case 0x6C: return m_sectorSize;
	default:   return 0;
	}
}

void CdServo::write32(uint32_t physWord, uint32_t data)
{
	switch (physWord - BASE)
	{
	case 0x04: m_addr = data; break;
	case 0x08: m_data = data; break;
	case 0x0C:
		if (data & 1) servoRead(); else servoWrite();
		break;
	case 0x40: m_control0 = data; break;
	case 0x44:
		m_control1 = data;
		m_seekLba = (bcdToDec(m_seekMin) * 60 + bcdToDec(m_seekSec)) * 75 + bcdToDec(m_seekFrm);
		m_curSector = (int64_t)LEADIN_LEN + m_seekLba;           // MAME: no seek time
		break;
	case 0x48: m_seekMin = (uint8_t)data; break;
	case 0x4C: m_seekSec = (uint8_t)data; break;
	case 0x50: m_seekFrm = (uint8_t)data; break;
	case 0x60: m_bufStart = data; break;
	case 0x64: m_bufEnd = data; break;
	case 0x68: m_bufPtr = data; break;
	case 0x6C: m_sectorSize = data; break;
	default: break;
	}
}

void CdServo::servoRead()
{
	const uint32_t a = m_addr;
	if      (a == 0x079) m_data = 0xE3;                       // servo status
	else if (a == 0x07B) m_data = 0x06;                       // unknown, 4-7
	else if (a == 0x07C) m_data = m_dspData >> 8;
	else if (a == 0x07D) m_data = m_dspData;
	else if (a == 0x082) m_data = 0x04;
	else if (a == 0x083) { m_rand = m_rand * 1664525u + 1013904223u; m_data = (m_rand >> 16) & 0xFF; }
	else if (a == 0x307) m_data = m_frameFound ? 1 : 0;
	else if (a >= 0x340 && a <= 0x349)                        // Q subchannel
		m_data = (m_curSector >= 0 && (uint64_t)m_curSector < m_totSectors) ? m_qsub[(size_t)m_curSector * 12 + (a & 0x0F)] : 0;
	else if (a == 0x34C) m_data = 0x40;                       // Q subchannel ready
	else if (a == 0x506) m_data = 0x04;                       // DSP ready
	else if (a >= 0x500 && a < 0x50A) m_data = m_dspRegs[a & 0x0F];
	else if (a == 0x41F) m_data = 0xFF;                       // zero would cause a servo timeout
	else m_data = 0;
}

void CdServo::servoWrite()
{
	const uint32_t a = m_addr;
	if (a == 0x013)                                           // skip n sectors
	{
		if (m_data & 0x01)
			m_curSector += (m_data & 0x10) ? -(int64_t)m_skip : (int64_t)m_skip;
		if (m_curSector < 0)
			m_curSector = 0;
		if (m_totSectors && m_curSector >= (int64_t)m_totSectors)
			m_curSector = (int64_t)m_totSectors - 1;
	}
	else if (a == 0x020)                                      // speed
	{
		const uint8_t speed = (uint8_t)(1u << (m_data & 0x03));
		if (m_speed != speed)
		{
			m_speed = speed;
			changeStatus();
		}
	}
	else if (a == 0x030)                                      // disc id: <10 none, <97 CD-RW, >=97 CD-ROM
		m_dspData = (m_data == 0x13) ? 8u : (m_disc ? 100u : 0u);
	else if (a == 0x032) m_dspData = 0x0102;                  // DSP version
	else if (a == 0x19A) m_skip = (uint16_t)((m_skip & 0x00FF) | (m_data << 8));
	else if (a == 0x29A) m_skip = (uint16_t)((m_skip & 0xFF00) | (m_data & 0xFF));
	else if (a == 0x307) m_frameFound = false;
	else if (a == 0x505)                                      // DSP memory
	{
		const uint16_t addr = (uint16_t)((m_dspRegs[0] << 8) | m_dspRegs[1]);
		if ((m_data & 0x0F) == 0x02)                          // write, 3-byte chunks
			m_dspMemory[addr] = (m_dspRegs[2] << 16) | (m_dspRegs[3] << 8) | m_dspRegs[4];
		else if ((m_data & 0x0F) == 0x03)                     // read
		{
			m_dspRegs[7] = (m_dspMemory[addr] >> 16) & 0xFF;
			m_dspRegs[8] = (m_dspMemory[addr] >> 8) & 0xFF;
			m_dspRegs[9] = (m_dspMemory[addr] >> 0) & 0xFF;
		}
	}
	else if (a >= 0x500 && a < 0x50A)
		m_dspRegs[a & 0x0F] = m_data;
}
