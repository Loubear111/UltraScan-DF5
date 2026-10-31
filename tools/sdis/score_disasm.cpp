#include "score_disasm.h"

#include <cstdarg>
#include <cstdio>
#include <sstream>

#include "emu.h"
#include "scoredsm.h"

namespace util {

void stream_format(std::ostream &stream, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	stream << buf;
}

} // namespace util

std::string score7Disasm(std::uint32_t pc, std::uint32_t word, int& size)
{
	const std::uint8_t bytes[4] = {
		static_cast<std::uint8_t>(word),
		static_cast<std::uint8_t>(word >> 8),
		static_cast<std::uint8_t>(word >> 16),
		static_cast<std::uint8_t>(word >> 24),
	};
	const util::disasm_interface::data_buffer buf(bytes, sizeof(bytes), pc);

	score7_disassembler dis;
	std::ostringstream out;
	size = static_cast<int>(dis.disassemble(out, pc, buf, buf));
	return out.str();
}
