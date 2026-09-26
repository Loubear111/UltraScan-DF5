#pragma once

// ============================================================================
// Tiny dependency-free image writer for PPU render-test artifacts.
//
// The PPU renderer returns an RGB frame buffer (std::vector<PPU::Color>). This
// helper serialises that buffer to a 24-bit BMP file under a test-artifact
// directory so the visual end result of each render test can be inspected.
//
// The artifact directory defaults to "../tests/artifacts" (the test runner is
// launched from src/ by build.mk) and can be overridden with the
// PPU_TEST_ARTIFACT_DIR environment variable. Failures to create the directory
// or open the file are swallowed so artifact writing never fails a test.
// ============================================================================

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>
#include <filesystem>

#include "ppu.h"

namespace ppu_artifacts
{

inline std::string artifactDir()
{
	const char *env = std::getenv("PPU_TEST_ARTIFACT_DIR");
	std::string dir = env ? env : "../tests/artifacts";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	return dir;
}

// Write a 24-bit, bottom-up BMP. Rows are padded to a 4-byte boundary.
inline void writeBMP(const std::string &path, const std::vector<PPU::Color> &fb, int w, int h)
{
	if (w <= 0 || h <= 0 || fb.size() < (size_t)w * h) return;

	std::ofstream f(path, std::ios::binary);
	if (!f) return;

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
}

// Convenience: dump a frame buffer named "<name>.bmp" into the artifact dir.
inline void dump(const std::string &name, const std::vector<PPU::Color> &fb, int w, int h)
{
	writeBMP(artifactDir() + "/" + name + ".bmp", fb, w, h);
}

// Nearest-neighbour upscale of a cropped region of the frame buffer. Produces
// a (cropW*scale) x (cropH*scale) image so tiny scenes become human readable.
// Out-of-bounds source pixels are filled with `fill`.
inline std::vector<PPU::Color> scaleCrop(const std::vector<PPU::Color> &fb, int w, int h,
                                         int cropX, int cropY, int cropW, int cropH,
                                         int scale, PPU::Color fill = PPU::Color{})
{
	if (scale < 1) scale = 1;
	const int ow = cropW * scale, oh = cropH * scale;
	std::vector<PPU::Color> out((size_t)ow * oh, fill);
	for (int y = 0; y < oh; y++)
	{
		int sy = cropY + y / scale;
		for (int x = 0; x < ow; x++)
		{
			int sx = cropX + x / scale;
			if (sx >= 0 && sx < w && sy >= 0 && sy < h)
				out[(size_t)y * ow + x] = fb[(size_t)sy * w + sx];
		}
	}
	return out;
}

// Dump a zoomed-in, "giant" variant. Defaults crop the top-left 40x30 corner
// (where the small render-test scenes draw) and scale 16x -> 640x480.
inline void dumpGiant(const std::string &name, const std::vector<PPU::Color> &fb, int w, int h,
                      int cropX = 0, int cropY = 0, int cropW = 40, int cropH = 30, int scale = 16)
{
	auto big = scaleCrop(fb, w, h, cropX, cropY, cropW, cropH, scale);
	writeBMP(artifactDir() + "/" + name + ".bmp", big, cropW * scale, cropH * scale);
}

} // namespace ppu_artifacts
