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
	std::array<uint8_t, 64 * 1024> ram;

public: // Bus Read and Write
	void write(uint16_t addr, uint8_t data);
	uint8_t read(uint16_t addr, bool bReadOnly = false);
};
