#include "I2C.h"

void I2CMaster::reset()
{
	m_config = m_irqControl = m_clockConf = m_id = 0;
	m_portAddr = m_wdata = m_rdata = 0;
	m_running = false;
	m_left = m_repeat = 0;
	for (auto& p : m_ctrl)
	{
		p[0] = 0;
		p[1] = 0;
		p[2] = 0x7F;   // analog Y centred
		p[3] = 0x7F;   // analog X centred
	}
}

uint16_t I2CMaster::readPort(uint32_t addr) const
{
	const unsigned port = (addr >> 4) & 0x0F;
	const unsigned idx = addr & 0x0F;
	if (port >= PORTS)
		return 0xFFFF;
	if (idx < 4)
		return m_ctrl[port][idx];
	const uint32_t sum = (uint32_t)m_ctrl[port][0] + m_ctrl[port][1] + m_ctrl[port][2] + m_ctrl[port][3];
	return (uint16_t)((sum << 2) | port);
}

uint32_t I2CMaster::read32(uint32_t physWord) const
{
	switch (physWord - BASE)
	{
	case 0x20: return m_config;
	case 0x24: return m_irqControl;
	case 0x28: return m_clockConf;
	case 0x2C: return m_id;
	case 0x30: return m_portAddr;
	case 0x34: return m_wdata;
	case 0x38: return m_rdata;
	default:   return 0;
	}
}

void I2CMaster::write32(uint32_t physWord, uint32_t data)
{
	switch (physWord - BASE)
	{
	case 0x20:
	{
		m_config = data;
		const uint64_t clk = 16ull * ((m_clockConf & 0x3FFu) + 1u);   // CPU cycles per bus clock
		if (m_config & 0x0001)       { m_running = true; m_left = clk * (2 + 9 * 4); m_repeat = 0; }
		else if (m_config & 0x0002)  { m_running = true; m_left = clk * (2 + 9 * 5); m_repeat = 0; }
		else if (m_config & 0x0004)  { m_running = true; m_left = clk * (2 + 9 * 4); m_repeat = m_left; }
		else if (m_config & 0x0100)  { m_running = false; }
		break;
	}
	case 0x24:
		m_irqControl = data;
		m_irqControl &= ~(data & 1u);                  // acknowledge
		break;
	case 0x28: m_clockConf = data; break;
	case 0x2C: m_id = data; break;
	case 0x30: m_portAddr = data; break;
	case 0x34: m_wdata = data; break;
	case 0x38: m_rdata = data; break;
	default: break;
	}
}

void I2CMaster::tick(uint32_t cpuCycles)
{
	uint64_t remaining = cpuCycles;
	while (m_running && remaining >= m_left)
	{
		remaining -= m_left;
		if (m_config & 0x40)
			m_rdata = readPort(m_portAddr);            // a write goes nowhere: nothing is attached
		m_config |= (m_config & 7u) << 3;              // ack
		if (m_irqControl & 0x02u)
			m_irqControl |= 1u;
		if (m_repeat)
			m_left = m_repeat;
		else
		{
			m_running = false;
			m_left = 0;
		}
	}
	if (m_running)
		m_left -= remaining;
}
