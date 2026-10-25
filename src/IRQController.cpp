#include "IRQController.h"

void IRQController::reset()
{
	m_lines = 0;
	m_mask  = SOURCE_MASK;
	m_pmst  = 0;
	for (auto &p : m_pslv) p = 0;
	m_swi    = false;
	m_winner = -1;
}

uint64_t IRQController::pending() const
{
	uint64_t p = m_lines;
	if (m_swi) p |= 1ull << SRC_GPU_SWI;
	return p & SOURCE_MASK;
}

void IRQController::setSource(unsigned source, bool level)
{
	if (source >= NUM_SOURCES) return;
	if (level) m_lines |= 1ull << source;
	else       m_lines &= ~(1ull << source);
	arbitrate();
}

// Lowest (group priority, source priority, source number) wins; 000 is the
// highest priority and a lower source number means a higher vector.
void IRQController::arbitrate()
{
	uint64_t active = pending() & ~m_mask;
	m_winner = -1;
	unsigned bestGroup = 0, bestSrc = 0;

	for (unsigned s = 0; s < NUM_SOURCES; ++s)
	{
		if (!(active & (1ull << s))) continue;
		unsigned g  = s / 8;
		unsigned gp = (m_pmst >> (2 * g)) & 0x3;
		unsigned sp = (m_pslv[g] >> (3 * (s % 8))) & 0x7;
		if (m_winner < 0 || gp < bestGroup || (gp == bestGroup && sp < bestSrc))
		{
			m_winner  = (int)s;
			bestGroup = gp;
			bestSrc   = sp;
		}
	}
}

uint32_t IRQController::read32(uint32_t addr) const
{
	switch (addr & 0xFFFFu)
	{
	case P_INTPND:   return (uint32_t)(pending() & 0xFFFFFFFFull);
	case P_INTPND_H: return (uint32_t)((pending() >> 32) & 0xFFu);
	case P_I_PMST:   return m_pmst;
	case P_I_GPUSWI: return m_swi ? 1u : 0u;
	case P_I_PSLV0:  return m_pslv[0];
	case P_I_PSLV1:  return m_pslv[1];
	case P_I_PSLV2:  return m_pslv[2];
	case P_I_PSLV3:  return m_pslv[3];
	case P_I_MASKL:  return (uint32_t)(m_mask & 0xFFFFFFFFull);
	case P_I_MASKH:  return (uint32_t)((m_mask >> 32) & 0xFFu);
	default:         return 0;
	}
}

void IRQController::write32(uint32_t addr, uint32_t data)
{
	switch (addr & 0xFFFFu)
	{
	case P_I_PMST:   m_pmst = data & 0xFFu; break;
	case P_I_GPUSWI:
		if (data & 0x1u)   m_swi = true;
		if (data & 0x100u) m_swi = false;
		break;
	case P_I_PSLV0:  m_pslv[0] = data & 0xFFFFFFu; break;
	case P_I_PSLV1:  m_pslv[1] = data & 0xFFFFFFu; break;
	case P_I_PSLV2:  m_pslv[2] = data & 0xFFFFFFu; break;
	case P_I_PSLV3:  m_pslv[3] = data & 0xFFFFFFu; break;
	case P_I_MASKL:  m_mask = (m_mask & ~0xFFFFFFFFull) | data; break;
	case P_I_MASKH:  m_mask = (m_mask & 0xFFFFFFFFull) | ((uint64_t)(data & 0xFFu) << 32); break;
	default:         return; // INTPND/INTPND_H are read-only
	}
	arbitrate();
}
