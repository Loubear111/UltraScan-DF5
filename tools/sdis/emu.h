// Minimal stand-in for MAME's "emu.h", providing only what scoredsm.cpp uses.
// Written for this project (GPL-3.0); the MAME files in this folder are BSD-3-Clause.
#ifndef SDIS_EMU_H
#define SDIS_EMU_H

#include <cstdint>
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using offs_t = std::uint32_t;

namespace util {

// Sign-extend the low `bits` bits of `value`.
template <typename T>
constexpr std::int32_t sext(T value, unsigned bits)
{
	const std::uint32_t v = static_cast<std::uint32_t>(value);
	const std::uint32_t m = 1u << (bits - 1);
	return static_cast<std::int32_t>(((v & ((m << 1) - 1)) ^ m) - m);
}

// MAME's stream_format is a printf-style helper; this one takes the same format strings.
void stream_format(std::ostream &stream, const char *fmt, ...)
#if defined(__GNUC__)
	__attribute__((format(printf, 2, 3)))
#endif
	;

class disasm_interface
{
public:
	// Little-endian view of a byte range that starts at address `base`.
	// Reads past the end return zero.
	class data_buffer
	{
	public:
		data_buffer(const std::uint8_t *bytes, std::size_t size, offs_t base)
			: m_bytes(bytes), m_size(size), m_base(base) {}

		std::uint32_t r32(offs_t pc) const
		{
			std::uint32_t v = 0;
			for (unsigned i = 0; i < 4; i++)
				v |= static_cast<std::uint32_t>(byteAt(pc + i)) << (8 * i);
			return v;
		}

	private:
		std::uint8_t byteAt(offs_t addr) const
		{
			const std::uint64_t off = static_cast<std::uint64_t>(addr) - m_base;
			return (addr >= m_base && off < m_size) ? m_bytes[off] : 0;
		}

		const std::uint8_t *m_bytes;
		std::size_t m_size;
		offs_t m_base;
	};

	virtual ~disasm_interface() = default;
	virtual std::uint32_t opcode_alignment() const = 0;
	virtual offs_t disassemble(std::ostream &stream, offs_t pc, const data_buffer &opcodes, const data_buffer &params) = 0;
};

} // namespace util

#endif
