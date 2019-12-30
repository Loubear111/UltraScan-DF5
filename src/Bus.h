#pragma once
#include <cstdint>
#include <array>

#include "spg290.h"

class Bus
{
public:
	Bus();
	~Bus();

public: // Devices on bus
	spg290 cpu;

	// Fake RAM for testing of CPU -> Bus functionality
	std::array<uint32_t, 1 * 1024> ram;

public: // Bus Read and Write
	void write(uint16_t addr, uint32_t data);
	uint32_t read(uint16_t addr, bool bReadOnly = false);
};
