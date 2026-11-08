#pragma once
#include <cstdint>

// ============================================================================
// SPG290 I2C master at 0x0813_0000 with the two HyperScan controller ports behind
// it (Phase 5, story 23). Behaviour follows MAME's spg290_i2c_device and the
// controller and port wiring in its spg29x driver and hyperscan_ctrl (BSD-3-Clause,
// Sandro Ronco).
//
//   +0x20  configuration   bit 0 start 8-bit, bit 1 start 16-bit, bit 2 start N
//                          (repeating), bit 6 read (else write), bit 8 stop N.
//                          When a transfer finishes bits 2:0 are copied to 5:3.
//   +0x24  interrupt       bit 1 enable, bit 0 pending (write 1 to clear)
//   +0x28  clock setting   low 10 bits: divider
//   +0x2C  id, +0x30 port address, +0x34 write data, +0x38 read data
// A transfer takes 2 + 9*4 (start 8, start N) or 2 + 9*5 (start 16) bus clocks,
// one bus clock being 27 MHz / 4 / (divider + 1), i.e. 16 * (divider + 1) CPU
// cycles. A read fetches the controller byte at the port address:
//   port = (address >> 4) & 15, index = address & 15
//   ports 0 and 1: index 0..3 return the controller inputs; other indices return
//                  ((in0 + in1 + in2 + in3) << 2) | port, which the BIOS uses to
//                  validate the inputs; ports 2 and up return 0xFFFF.
// Interrupt line 39.
// ============================================================================
class I2CMaster
{
public:
	static constexpr uint32_t BASE   = 0x08130000u;   // physical
	static constexpr uint32_t WINDOW = 0x10000u;

	I2CMaster() { reset(); }
	void reset();

	static bool owns(uint32_t physWord) { return physWord >= BASE && physWord < BASE + WINDOW; }
	uint32_t read32(uint32_t physWord) const;
	void     write32(uint32_t physWord, uint32_t data);

	void tick(uint32_t cpuCycles);
	bool irq() const { return (m_irqControl & 1u) != 0; }

	// Controller inputs, as the controller reports them (active-high bits; the two
	// analog axes are 0..255 with 0x7F centred). Nothing pressed by default.
	static constexpr unsigned PORTS = 2;
	void    setControllerByte(unsigned port, unsigned index, uint8_t value) { if (port < PORTS && index < 4) m_ctrl[port][index] = value; }
	uint8_t controllerByte(unsigned port, unsigned index) const { return (port < PORTS && index < 4) ? m_ctrl[port][index] : 0; }

private:
	uint16_t readPort(uint32_t addr) const;

	uint32_t m_config = 0, m_irqControl = 0, m_clockConf = 0, m_id = 0;
	uint32_t m_portAddr = 0, m_wdata = 0, m_rdata = 0;
	bool     m_running = false;
	uint64_t m_left = 0;      // CPU cycles until the transfer completes
	uint64_t m_repeat = 0;    // period for "start N" (0 = one shot)
	uint8_t  m_ctrl[PORTS][4];
};
