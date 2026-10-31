// sdis: S+core7 disassembler CLI.
//   sdis <file> <base> [start] [end]
// <base> is the address the file's first byte is loaded at; [start, end) selects the range to
// disassemble (default: the whole file). Numbers accept 0x prefixes. Little-endian only.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

#include "score_disasm.h"

static bool parseAddr(const char* s, std::uint32_t& out)
{
	char* end = nullptr;
	const unsigned long long v = std::strtoull(s, &end, 0);
	if (end == s || *end != '\0' || v > 0xFFFFFFFFull) return false;
	out = static_cast<std::uint32_t>(v);
	return true;
}

int main(int argc, char** argv)
{
	if (argc < 3 || argc > 5) {
		std::fprintf(stderr, "usage: %s <file> <base> [start] [end]\n", argv[0]);
		return 2;
	}

	std::ifstream in(argv[1], std::ios::binary);
	if (!in) {
		std::fprintf(stderr, "sdis: cannot open %s\n", argv[1]);
		return 1;
	}
	const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

	std::uint32_t base = 0;
	if (!parseAddr(argv[2], base)) {
		std::fprintf(stderr, "sdis: bad base address '%s'\n", argv[2]);
		return 2;
	}
	const std::uint64_t fileEnd = static_cast<std::uint64_t>(base) + data.size();
	std::uint64_t start = base;
	std::uint64_t end = fileEnd;
	std::uint32_t v = 0;
	if (argc >= 4) {
		if (!parseAddr(argv[3], v)) { std::fprintf(stderr, "sdis: bad start '%s'\n", argv[3]); return 2; }
		start = v;
	}
	if (argc >= 5) {
		if (!parseAddr(argv[4], v)) { std::fprintf(stderr, "sdis: bad end '%s'\n", argv[4]); return 2; }
		end = v;
	}
	if (start < base || end > fileEnd || start > end) {
		std::fprintf(stderr, "sdis: range 0x%llx-0x%llx outside file 0x%llx-0x%llx\n",
		             (unsigned long long)start, (unsigned long long)end,
		             (unsigned long long)base, (unsigned long long)fileEnd);
		return 2;
	}

	for (std::uint64_t pc = start; pc < end;) {
		std::uint32_t word = 0;
		for (unsigned i = 0; i < 4; i++) {
			const std::uint64_t off = pc + i - base;
			if (off < data.size()) word |= static_cast<std::uint32_t>(data[off]) << (8 * i);
		}
		int size = 0;
		const std::string text = score7Disasm(static_cast<std::uint32_t>(pc), word, size);
		if (size == 4)
			std::printf("%08llx:  %08x  %s\n", (unsigned long long)pc, word, text.c_str());
		else
			std::printf("%08llx:  %04x      %s\n", (unsigned long long)pc, word & 0xFFFF, text.c_str());
		pc += size;
	}
	return 0;
}
