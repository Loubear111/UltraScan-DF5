#include "DiscImage.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace
{

std::string toLower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
	               [](unsigned char c) { return (char)std::tolower(c); });
	return s;
}

std::string toUpper(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
	               [](unsigned char c) { return (char)std::toupper(c); });
	return s;
}

std::string trim(const std::string& s)
{
	size_t b = 0, e = s.size();
	while (b < e && std::isspace((unsigned char)s[b])) ++b;
	while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
	return s.substr(b, e - b);
}

// Directory-record / lookup name: lower-case, version suffix (";1") removed.
std::string normaliseName(const std::string& n)
{
	std::string s = toLower(n);
	size_t semi = s.find(';');
	if (semi != std::string::npos)
		s.erase(semi);
	return s;
}

uint32_t le32(const uint8_t* p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

} // namespace

bool DiscImage::fail(const std::string& msg)
{
	err = msg;
	return false;
}

bool DiscImage::openStream(const std::string& path)
{
	file.close();
	file.clear();
	file.open(std::filesystem::path(path), std::ios::binary);
	if (!file)
		return fail("cannot open '" + path + "'");

	file.seekg(0, std::ios::end);
	std::streamoff end = file.tellg();
	if (end < 0)
		return fail("cannot determine size of '" + path + "'");
	fileSize = (uint64_t)end;
	file.seekg(0, std::ios::beg);
	return true;
}

// Reads the first FILE and the first TRACK of a cue sheet. Sets sectorSize and
// dataOffset, and returns the data file path (relative names are resolved
// against the cue file's folder).
bool DiscImage::parseCue(const std::string& cuePath, std::string& binPath)
{
	std::ifstream cue{std::filesystem::path(cuePath)};
	if (!cue)
		return fail("cannot open '" + cuePath + "'");

	std::string fileName;
	bool haveFile = false, haveTrack = false;

	std::string raw;
	while (std::getline(cue, raw) && !(haveFile && haveTrack))
	{
		std::string line = trim(raw);
		std::istringstream is(line);
		std::string kw;
		is >> kw;
		kw = toUpper(kw);

		if (kw == "FILE" && !haveFile)
		{
			std::string rest;
			std::getline(is, rest);
			rest = trim(rest);
			std::string tail;
			if (!rest.empty() && rest[0] == '"')
			{
				size_t close = rest.find('"', 1);
				if (close == std::string::npos)
					return fail("cue: unterminated quoted FILE name");
				fileName = rest.substr(1, close - 1);
				tail = rest.substr(close + 1);
			}
			else
			{
				size_t sp = rest.find_first_of(" \t");
				fileName = rest.substr(0, sp);
				tail = (sp == std::string::npos) ? "" : rest.substr(sp);
			}
			if (fileName.empty())
				return fail("cue: FILE entry has no name");
			if (toUpper(trim(tail)) != "BINARY")
				return fail("cue: only FILE ... BINARY is supported");
			haveFile = true;
		}
		else if (kw == "TRACK" && !haveTrack)
		{
			if (!haveFile)
				return fail("cue: TRACK before FILE");
			std::string num, mode;
			is >> num >> mode;
			mode = toUpper(mode);
			if (mode == "AUDIO")
				return fail("cue: audio tracks are not supported");
			if (mode == "MODE1/2352")      { sectorSize = 2352; dataOffset = 16; }
			else if (mode == "MODE1/2048") { sectorSize = 2048; dataOffset = 0;  }
			else return fail("cue: unsupported track mode '" + mode + "'");
			haveTrack = true;
		}
	}

	if (!haveFile)  return fail("cue: no FILE entry");
	if (!haveTrack) return fail("cue: no TRACK entry");

	std::filesystem::path p(fileName);
	if (p.is_relative())
		p = std::filesystem::path(cuePath).parent_path() / p;
	binPath = p.string();
	return true;
}

bool DiscImage::open(const std::string& path)
{
	file.close();
	file.clear();
	opened = bare = false;
	fileSize = 0;
	sectorSize = USER_DATA;
	dataOffset = 0;
	err.clear();

	std::string ext = toLower(std::filesystem::path(path).extension().string());

	if (ext == ".cue")
	{
		std::string binPath;
		if (!parseCue(path, binPath) || !openStream(binPath))
			return false;
	}
	else if (ext == ".iso")
	{
		if (!openStream(path))
			return false;
	}
	else
	{
		if (!openStream(path))
			return false;
		bare = true;
	}

	opened = true;
	return true;
}

// Reads the 2048 user-data bytes of sector `lba`.
bool DiscImage::readSector(uint32_t lba, uint8_t* buf)
{
	uint64_t off = (uint64_t)lba * sectorSize + dataOffset;
	if (off + USER_DATA > fileSize)
		return fail("image is truncated (sector " + std::to_string(lba) + " is past the end)");

	file.clear();
	file.seekg((std::streamoff)off, std::ios::beg);
	file.read((char*)buf, USER_DATA);
	if (file.gcount() != (std::streamsize)USER_DATA)
		return fail("read error at sector " + std::to_string(lba));
	return true;
}

// Reads `bytes` bytes starting at the first sector of an extent.
bool DiscImage::readExtent(uint32_t lba, uint32_t bytes, std::vector<uint8_t>& out)
{
	if (bytes > MAX_FILE_BYTES)
		return fail("file is too large");

	// Check the whole extent lies inside the image before allocating.
	if (bytes > 0)
	{
		uint64_t sectors = ((uint64_t)bytes + USER_DATA - 1) / USER_DATA;
		uint64_t end = ((uint64_t)lba + sectors - 1) * sectorSize + dataOffset + USER_DATA;
		if (end > fileSize)
			return fail("image is truncated (extent at sector " + std::to_string(lba) + " is past the end)");
	}

	out.assign(bytes, 0);
	uint8_t sector[USER_DATA];
	uint32_t done = 0;
	for (uint32_t s = 0; done < bytes; ++s)
	{
		if (!readSector(lba + s, sector))
		{
			out.clear();
			return false;
		}
		uint32_t n = std::min<uint32_t>(USER_DATA, bytes - done);
		std::memcpy(&out[done], sector, n);
		done += n;
	}
	return true;
}

bool DiscImage::readFile(const std::string& name, std::vector<uint8_t>& out)
{
	err.clear();
	out.clear();

	if (!opened)
		return fail("no image is open");

	if (bare)
	{
		if (fileSize == 0)
			return fail("file is empty");
		if (fileSize > MAX_FILE_BYTES)
			return fail("file is too large");
		out.resize((size_t)fileSize);
		file.clear();
		file.seekg(0, std::ios::beg);
		file.read((char*)out.data(), (std::streamsize)fileSize);
		if ((uint64_t)file.gcount() != fileSize)
		{
			out.clear();
			return fail("read error");
		}
		return true;
	}

	// Primary Volume Descriptor.
	uint8_t pvd[USER_DATA];
	if (!readSector(16, pvd))
		return false;
	if (pvd[0] != 1 || std::memcmp(pvd + 1, "CD001", 5) != 0)
		return fail("not an ISO9660 image (no CD001 volume descriptor at sector 16)");

	// Root directory record (34 bytes at PVD offset 156).
	const uint8_t* root = pvd + 156;
	if (root[0] < 34 || !(root[25] & 0x02))
		return fail("invalid root directory record");
	uint32_t rootLba  = le32(root + 2);
	uint32_t rootSize = le32(root + 10);
	if (rootSize == 0 || rootSize > 16u * 1024 * 1024)
		return fail("invalid root directory size");

	std::vector<uint8_t> dir;
	// Whole sectors: a directory is always sector-padded.
	uint32_t dirBytes = (rootSize + USER_DATA - 1) / USER_DATA * USER_DATA;
	if (!readExtent(rootLba, dirBytes, dir))
		return false;

	const std::string want = normaliseName(name);

	uint32_t pos = 0;
	while (pos < rootSize)
	{
		uint8_t len = dir[pos];
		if (len == 0)
		{
			// Records never cross a sector boundary; skip the padding.
			pos = (pos / USER_DATA + 1) * USER_DATA;
			continue;
		}
		if (len < 34 || pos + len > dirBytes)
			return fail("malformed directory record");

		const uint8_t* rec = &dir[pos];
		uint8_t nameLen = rec[32];
		if (33u + nameLen > len)
			return fail("malformed directory record");

		bool special = (nameLen == 1 && rec[33] <= 1);
		if (!special && !(rec[25] & 0x02))
		{
			std::string entry((const char*)rec + 33, nameLen);
			if (normaliseName(entry) == want)
			{
				uint32_t lba  = le32(rec + 2);
				uint32_t size = le32(rec + 10);
				if (size == 0)
					return fail("'" + name + "' is empty");
				return readExtent(lba, size, out);
			}
		}
		pos += len;
	}

	return fail("'" + name + "' not found in the root directory");
}
