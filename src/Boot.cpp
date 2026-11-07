#include "Boot.h"

#include <fstream>
#include <iterator>
#include <vector>

#include "Bus.h"

bool bootExecutable(Bus& bus, const uint8_t* data, size_t size, const BootParams& params)
{
	if (size == 0 || !bus.loadImage(data, size, params.loadAddr))
		return false;

	bus.setLittleEndian(params.littleEndian);
	bus.cpu.pc = params.entryPc;
	if (params.cr3)
		bus.cpu.cr[3] = *params.cr3;
	if (params.sp)
		bus.cpu.regs[0] = *params.sp;
	return true;
}

bool bootExecutableFile(Bus& bus, const std::string& path, const BootParams& params)
{
	std::ifstream f(path, std::ios::binary);
	if (!f)
		return false;
	std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	return bootExecutable(bus, data.data(), data.size(), params);
}
