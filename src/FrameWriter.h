#pragma once

// ============================================================================
// Dependency-free frame writer. Serialises a PPU RGB frame buffer to a 24-bit
// BMP file. Used by the command-line driver and the PPU test artifacts.
// ============================================================================

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "ppu.h"

namespace frame_io
{

// Write a 24-bit, bottom-up BMP. Rows are padded to a 4-byte boundary.
// The parent directory is created if needed. Returns false when the buffer is
// too small or the file cannot be created/written.
inline bool writeBMP(const std::string &path, const std::vector<PPU::Color> &fb, int w, int h)
{
	if (w <= 0 || h <= 0 || fb.size() < (size_t)w * (size_t)h) return false;

	std::filesystem::path p(path);
	if (p.has_parent_path())
	{
		std::error_code ec;
		std::filesystem::create_directories(p.parent_path(), ec);
	}

	std::ofstream f(path, std::ios::binary);
	if (!f) return false;

	const uint32_t rowSize = ((uint32_t)w * 3 + 3) & ~3u;
	const uint32_t pixSize = rowSize * (uint32_t)h;
	const uint32_t dataOff = 14 + 40;
	const uint32_t fileSize = dataOff + pixSize;

	auto put16 = [&](uint16_t v) { f.put((char)(v & 0xFF)); f.put((char)((v >> 8) & 0xFF)); };
	auto put32 = [&](uint32_t v) {
		f.put((char)(v & 0xFF)); f.put((char)((v >> 8) & 0xFF));
		f.put((char)((v >> 16) & 0xFF)); f.put((char)((v >> 24) & 0xFF));
	};

	// BITMAPFILEHEADER
	f.put('B'); f.put('M');
	put32(fileSize); put16(0); put16(0); put32(dataOff);
	// BITMAPINFOHEADER
	put32(40); put32((uint32_t)w); put32((uint32_t)h);
	put16(1); put16(24); put32(0); put32(pixSize);
	put32(2835); put32(2835); put32(0); put32(0);

	std::vector<uint8_t> row(rowSize, 0);
	for (int y = h - 1; y >= 0; y--)
	{
		for (int x = 0; x < w; x++)
		{
			const PPU::Color &c = fb[(size_t)y * w + x];
			row[x * 3 + 0] = c.b;
			row[x * 3 + 1] = c.g;
			row[x * 3 + 2] = c.r;
		}
		f.write((const char *)row.data(), (std::streamsize)rowSize);
	}
	return (bool)f;
}

} // namespace frame_io
