#pragma once
#include <cstdint>

// ============================================================================
// SPG290 timer block (Phase 5, story 23). Behaviour follows MAME's
// spg290_timer_device and the clock wiring in its spg29x driver (BSD-3-Clause,
// Sandro Ronco); only the timer-mode counting is modelled, as in MAME.
//
// Six channels, 0x1000 apart from 0x0816_0000. Registers per channel:
//   +0x00  control 1   bit 31 enable, bit 27 interrupt enable, bit 26 interrupt
//                      flag (write 1 to clear)
//   +0x04  control 2   bits 31:30 mode (0 = timer; the others are not emulated)
//   +0x08  preload     low 16 bits
//   +0x0C  CCP         stored
//   +0x10  upcount     stored
// The counter counts up one step per timer clock; at 0xFFFF the next step
// reloads it from the preload and, with bit 27 set, raises the flag and the
// interrupt line. A channel only runs while its clock is enabled by a write to
// the clock generator at 0x0821_006C + 4 * channel (bit 0 enable, bit 1 load the
// counter from the preload). The shared clock source is selected at
// 0x0821_00E4: 27 MHz / ((value & 0xFF) + 1), or 32.768 kHz for the channels
// whose bit (8 + channel) is set. The CPU runs at 4 x 27 MHz = 108 MHz.
// All six channels drive interrupt line 56.
// ============================================================================
class TimerBlock
{
public:
	static constexpr uint32_t BASE     = 0x08160000u;   // physical
	static constexpr uint32_t CHANNELS = 6;
	static constexpr uint32_t STRIDE   = 0x1000u;
	static constexpr uint32_t REGS     = 0x14u;         // bytes of registers per channel

	static constexpr uint32_t CKG_TIMER_CTRL = 0x0821006Cu;   // + 4 * channel, write only
	static constexpr uint32_t CKG_CLK_SEL    = 0x082100E4u;

	static constexpr uint32_t CTRL_ENABLE  = 1u << 31;
	static constexpr uint32_t CTRL_IRQ_EN  = 1u << 27;
	static constexpr uint32_t CTRL_FLAG    = 1u << 26;

	static constexpr uint32_t CPU_HZ       = 108000000u;
	static constexpr uint32_t XTAL_HZ      = 27000000u;
	static constexpr uint32_t SLOW_HZ      = 32768u;

	TimerBlock() { reset(); }
	void reset();

	static bool owns(uint32_t physWord)
	{
		if (physWord >= BASE && physWord < BASE + CHANNELS * STRIDE && (physWord & (STRIDE - 1)) < REGS)
			return true;
		return (physWord >= CKG_TIMER_CTRL && physWord < CKG_TIMER_CTRL + 4 * CHANNELS) || physWord == CKG_CLK_SEL;
	}
	uint32_t read32(uint32_t physWord) const;
	void     write32(uint32_t physWord, uint32_t data);

	// Advance every running channel.
	void tick(uint32_t cpuCycles);

	// True while any channel has its flag set with the interrupt enabled.
	bool irq() const;

	// CPU cycles until the next tick() could change irq(): the soonest counter wrap of a
	// running channel that has the interrupt enabled and its flag clear (~0 when none).
	// Counters are not readable, so nothing else about the block is observable between
	// events; tick() calls compose, which lets the Bus batch them.
	uint64_t cyclesToEvent() const;

	// CPU cycles per timer clock for a channel under the current clock selection.
	uint32_t cyclesPerTick(unsigned channel) const;

private:
	struct Channel
	{
		uint32_t control = 0;
		uint32_t control2 = 0;
		uint32_t preload = 0;
		uint32_t ccp = 0;
		uint32_t upcount = 0;
		uint32_t counter = 0;
		bool     clockOn = false;   // from the clock generator
		uint64_t acc = 0;           // CPU cycles carried towards the next timer clock
	};
	Channel  m_ch[CHANNELS];
	uint32_t m_clkSel = 0;
};
