#include "Timer.h"

void TimerBlock::reset()
{
	for (Channel& c : m_ch)
		c = Channel();
	m_clkSel = 0;
}

uint32_t TimerBlock::cyclesPerTick(unsigned channel) const
{
	if (m_clkSel & (0x100u << channel))
		return CPU_HZ / SLOW_HZ;                                   // 32.768 kHz
	return (CPU_HZ / XTAL_HZ) * ((m_clkSel & 0xFFu) + 1u);        // 27 MHz / divider
}

uint32_t TimerBlock::read32(uint32_t physWord) const
{
	if (physWord == CKG_CLK_SEL)
		return m_clkSel;
	if (physWord >= CKG_TIMER_CTRL && physWord < CKG_TIMER_CTRL + 4 * CHANNELS)
		return 0;   // write only
	const Channel& c = m_ch[(physWord - BASE) / STRIDE];
	switch (physWord & (STRIDE - 1))
	{
	case 0x00: return c.control;
	case 0x04: return c.control2;
	case 0x08: return c.preload;
	case 0x0C: return c.ccp;
	case 0x10: return c.upcount;
	default:   return 0;
	}
}

void TimerBlock::write32(uint32_t physWord, uint32_t data)
{
	if (physWord == CKG_CLK_SEL)
	{
		m_clkSel = data;
		return;
	}
	if (physWord >= CKG_TIMER_CTRL && physWord < CKG_TIMER_CTRL + 4 * CHANNELS)
	{
		Channel& c = m_ch[(physWord - CKG_TIMER_CTRL) / 4];
		c.clockOn = (data & 1u) != 0;
		if (!c.clockOn)
			c.acc = 0;
		if (data & 2u)
			c.counter = c.preload & 0xFFFFu;
		return;
	}
	Channel& c = m_ch[(physWord - BASE) / STRIDE];
	switch (physWord & (STRIDE - 1))
	{
	case 0x00:
	{
		// MAME stores the written word and then clears the flag if bit 26 was written
		// as 1, so every write leaves the flag clear.
		c.control = data & ~CTRL_FLAG;
		break;
	}
	case 0x04: c.control2 = data; break;
	case 0x08: c.preload = data; break;
	case 0x0C: c.ccp = data; break;
	case 0x10: c.upcount = data; break;
	default: break;
	}
}

void TimerBlock::tick(uint32_t cpuCycles)
{
	for (unsigned i = 0; i < CHANNELS; ++i)
	{
		Channel& c = m_ch[i];
		if (!c.clockOn || !(c.control & CTRL_ENABLE) || ((c.control2 >> 30) & 3u) != 0)
			continue;
		const uint32_t cpt = cyclesPerTick(i);
		c.acc += cpuCycles;
		uint64_t ticks = c.acc / cpt;
		c.acc %= cpt;
		if (!ticks)
			continue;

		const uint32_t preload = c.preload & 0xFFFFu;
		const uint64_t toTop = 0xFFFFu - c.counter;            // steps until the counter reads 0xFFFF
		if (ticks <= toTop)
		{
			c.counter += (uint32_t)ticks;
			continue;
		}
		ticks -= toTop + 1;                                      // the step that reloads
		if (c.control & CTRL_IRQ_EN)
			c.control |= CTRL_FLAG;
		const uint64_t period = 0x10000u - preload;              // reload step plus the climb back
		ticks %= period;
		c.counter = preload + (uint32_t)ticks;
	}
}

uint64_t TimerBlock::cyclesToEvent() const
{
	uint64_t best = ~0ull;
	for (unsigned i = 0; i < CHANNELS; ++i)
	{
		const Channel& c = m_ch[i];
		if (!c.clockOn || !(c.control & CTRL_ENABLE) || ((c.control2 >> 30) & 3u) != 0)
			continue;
		if (!(c.control & CTRL_IRQ_EN) || (c.control & CTRL_FLAG))
			continue;                                         // a wrap would change nothing visible
		const uint64_t cpt = cyclesPerTick(i);
		const uint64_t steps = 0x10000u - (c.counter & 0xFFFFu);     // the wrap happens on this step
		const uint64_t need = steps * cpt;
		const uint64_t cycles = need > c.acc ? need - c.acc : 1;
		if (cycles < best)
			best = cycles;
	}
	return best;
}

bool TimerBlock::irq() const
{
	for (const Channel& c : m_ch)
		if ((c.control & CTRL_FLAG) && (c.control & CTRL_IRQ_EN))
			return true;
	return false;
}
