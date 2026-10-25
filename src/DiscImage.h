#pragma once

// ============================================================================
// Disc image / executable reader. No dependency on Bus.
//
// Accepts:
//   .cue  first FILE entry (resolved relative to the .cue's folder) and first
//         TRACK. MODE1/2352 -> 2352-byte sectors, user data at offset 16;
//         MODE1/2048 -> 2048-byte sectors, offset 0.
//   .iso  2048-byte sectors, offset 0.
//   other any other extension is treated as a bare file (isBare()).
//
// The image is never loaded whole: a stream stays open and each sector is
// read with a seek. Only the ISO9660 root directory is searched.
// ============================================================================

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

class DiscImage
{
public:
	// Largest file readFile() will return (also bounds a bare file).
	static constexpr uint64_t MAX_FILE_BYTES = 256ull * 1024 * 1024;

	// Returns false and sets error() on failure.
	bool open(const std::string& path);

	bool isBare() const { return bare; }

	// Read a root-directory file by name (case-insensitive, ";1" version
	// suffix ignored). For a bare file the whole file is returned regardless
	// of `name`. Returns false and sets error() on failure.
	bool readFile(const std::string& name, std::vector<uint8_t>& out);

	// Last failure reason (empty after a successful call).
	std::string error() const { return err; }

private:
	static constexpr uint32_t USER_DATA = 2048;

	std::ifstream file;
	uint64_t fileSize   = 0;
	uint32_t sectorSize = USER_DATA;
	uint32_t dataOffset = 0;
	bool     opened     = false;
	bool     bare       = false;
	std::string err;

	bool fail(const std::string& msg);
	bool parseCue(const std::string& cuePath, std::string& binPath);
	bool openStream(const std::string& path);
	bool readSector(uint32_t lba, uint8_t* buf);
	bool readExtent(uint32_t lba, uint32_t bytes, std::vector<uint8_t>& out);
};
