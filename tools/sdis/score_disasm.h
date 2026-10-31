// S+core7 disassembly entry point built on MAME's scoredsm.cpp.
#ifndef SDIS_SCORE_DISASM_H
#define SDIS_SCORE_DISASM_H

#include <cstdint>
#include <string>

// Disassembles the instruction at `pc`. `word` is the 32-bit little-endian word read at `pc`
// (so for a halfword-aligned pc whose low half is the instruction, pass the word starting there).
// `size` receives the instruction length in bytes (2 or 4).
std::string score7Disasm(std::uint32_t pc, std::uint32_t word, int& size);

#endif
