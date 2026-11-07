#include "TVE.h"

#include "Bus.h"

void TVE::reset()
{
	m_control = 0;
	m_fade    = 0;
	m_start[0] = m_start[1] = m_start[2] = 0;
	m_bufSel  = 3;   // framebuffer off
}

bool TVE::owns(uint32_t p)
{
	return p == CONTROL || p == FADE || p == START0 || p == START1 || p == START2 || p == BUFSEL;
}

uint32_t TVE::read32(uint32_t p) const
{
	switch (p)
	{
	case CONTROL: return m_control;
	case FADE:    return m_fade;
	case START0:  return m_start[0];
	case START1:  return m_start[1];
	case START2:  return m_start[2];
	case BUFSEL:  return m_bufSel;
	default:      return 0;
	}
}

void TVE::write32(uint32_t p, uint32_t data)
{
	switch (p)
	{
	case CONTROL: m_control = data; break;
	case FADE:    m_fade = data & 0xFFu; break;
	case START0:  m_start[0] = data; break;
	case START1:  m_start[1] = data; break;
	case START2:  m_start[2] = data; break;
	case BUFSEL:  m_bufSel = data & 3u; break;
	default: break;
	}
}

void TVE::size(int& w, int& h) const
{
	switch ((m_control >> 2) & 3u)
	{
	case 1:  w = 640; h = 480; break;   // VGA
	case 2:  w = 640; h = 240; break;   // HVGA
	default: w = 320; h = 240; break;   // QVGA (mode 3 is undefined)
	}
}

static uint8_t expand5(uint32_t v) { return (uint8_t)((v << 3) | (v >> 2)); }
static uint8_t expand6(uint32_t v) { return (uint8_t)((v << 2) | (v >> 4)); }

std::vector<PPU::Color> TVE::compose(const Bus& bus, const std::vector<PPU::Color>* ppuFrame,
                                     int& w, int& h) const
{
	size(w, h);
	std::vector<PPU::Color> out((size_t)w * (size_t)h);   // black

	if (ppuFrame)
	{
		int pw = 0, ph = 0;
		bus.ppu.screenSize(pw, ph);
		for (int y = 0; y < h && y < ph; ++y)
			for (int x = 0; x < w && x < pw; ++x)
			{
				const size_t si = (size_t)y * (size_t)pw + (size_t)x;
				if (si < ppuFrame->size())
					out[(size_t)y * (size_t)w + (size_t)x] = (*ppuFrame)[si];
			}
	}

	if (framebufferOn())
	{
		const uint32_t base = m_start[bufferSelect()];
		const bool doubling = progressiveLineDoubling && !interlaced();
		for (int y = 0; y < h; ++y)
		{
			const int srcRow = doubling ? (y & ~1) : y;
			for (int x = 0; x < w; ++x)
			{
				const uint32_t a = base + (uint32_t)((srcRow * w + x) * 2);
				const uint32_t v = (uint32_t)bus.read8(a) | ((uint32_t)bus.read8(a + 1) << 8);   // little-endian
				PPU::Color c;
				c.r = expand5((v >> 11) & 0x1F);
				c.g = expand6((v >> 5) & 0x3F);
				c.b = expand5(v & 0x1F);
				out[(size_t)y * (size_t)w + (size_t)x] = c;
			}
		}
	}

	const unsigned fadeAmt = fade();
	if (fadeAmt != 0)
	{
		const unsigned scale = 255u - fadeAmt;
		for (PPU::Color& c : out)
		{
			c.r = (uint8_t)(c.r * scale / 255u);
			c.g = (uint8_t)(c.g * scale / 255u);
			c.b = (uint8_t)(c.b * scale / 255u);
		}
	}
	return out;
}
