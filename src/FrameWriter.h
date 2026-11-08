#pragma once

// ============================================================================
// Dependency-free frame writer. Serialises a PPU RGB frame buffer to a 24-bit
// BMP file. Used by the command-line driver and the PPU test artifacts.
// ============================================================================

#include <algorithm>
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

// ---- PNG (dependency-free: uncompressed "stored" deflate blocks) -------------------
// Bigger than a compressed PNG (about the size of a BMP) but every viewer, and the
// tools that read images for debugging, open it.
namespace detail
{
inline uint32_t crc32(const uint8_t *d, size_t n, uint32_t crc = 0)
{
	static uint32_t table[256];
	static bool init = false;
	if (!init)
	{
		for (uint32_t i = 0; i < 256; ++i)
		{
			uint32_t c = i;
			for (int k = 0; k < 8; ++k)
				c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : (c >> 1);
			table[i] = c;
		}
		init = true;
	}
	crc = ~crc;
	for (size_t i = 0; i < n; ++i)
		crc = table[(crc ^ d[i]) & 0xFF] ^ (crc >> 8);
	return ~crc;
}
inline void be32(std::vector<uint8_t> &v, uint32_t x)
{
	v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
	v.push_back((uint8_t)(x >> 8));  v.push_back((uint8_t)x);
}
inline void pngChunk(std::vector<uint8_t> &out, const char *type, const std::vector<uint8_t> &data)
{
	be32(out, (uint32_t)data.size());
	std::vector<uint8_t> body(type, type + 4);
	body.insert(body.end(), data.begin(), data.end());
	out.insert(out.end(), body.begin(), body.end());
	be32(out, crc32(body.data(), body.size()));
}
} // namespace detail

// Encode an RGB frame as a PNG (8-bit RGB). Empty on a bad size.
inline std::vector<uint8_t> encodePNG(const std::vector<PPU::Color> &fb, int w, int h)
{
	std::vector<uint8_t> out;
	if (w <= 0 || h <= 0 || fb.size() < (size_t)w * (size_t)h) return out;

	// Raw scanlines, each prefixed with filter type 0.
	std::vector<uint8_t> raw;
	raw.reserve((size_t)h * ((size_t)w * 3 + 1));
	for (int y = 0; y < h; ++y)
	{
		raw.push_back(0);
		for (int x = 0; x < w; ++x)
		{
			const PPU::Color &c = fb[(size_t)y * w + x];
			raw.push_back(c.r); raw.push_back(c.g); raw.push_back(c.b);
		}
	}

	// zlib stream made of stored blocks (max 65535 bytes each).
	std::vector<uint8_t> z = { 0x78, 0x01 };
	uint32_t a = 1, b = 0;
	for (uint8_t v : raw) { a = (a + v) % 65521u; b = (b + a) % 65521u; }
	size_t pos = 0;
	do
	{
		const size_t n = std::min<size_t>(65535, raw.size() - pos);
		const bool last = pos + n >= raw.size();
		z.push_back(last ? 1 : 0);
		z.push_back((uint8_t)(n & 0xFF)); z.push_back((uint8_t)(n >> 8));
		z.push_back((uint8_t)(~n & 0xFF)); z.push_back((uint8_t)((~n >> 8) & 0xFF));
		z.insert(z.end(), raw.begin() + (std::ptrdiff_t)pos, raw.begin() + (std::ptrdiff_t)(pos + n));
		pos += n;
	} while (pos < raw.size());
	detail::be32(z, (b << 16) | a);

	const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
	out.assign(sig, sig + 8);
	std::vector<uint8_t> ihdr;
	detail::be32(ihdr, (uint32_t)w); detail::be32(ihdr, (uint32_t)h);
	ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
	detail::pngChunk(out, "IHDR", ihdr);
	detail::pngChunk(out, "IDAT", z);
	detail::pngChunk(out, "IEND", {});
	return out;
}

inline bool writePNG(const std::string &path, const std::vector<PPU::Color> &fb, int w, int h)
{
	const std::vector<uint8_t> png = encodePNG(fb, w, h);
	if (png.empty()) return false;
	std::filesystem::path p(path);
	if (p.has_parent_path())
	{
		std::error_code ec;
		std::filesystem::create_directories(p.parent_path(), ec);
	}
	std::ofstream f(path, std::ios::binary);
	if (!f) return false;
	f.write((const char *)png.data(), (std::streamsize)png.size());
	return (bool)f;
}

} // namespace frame_io
