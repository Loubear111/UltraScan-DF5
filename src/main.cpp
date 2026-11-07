// Command-line driver: boots a HyperScan executable (HYPER.EXE from a disc image,
// a bare HYPER.EXE, or a raw binary), runs it against the video clock and writes
// the frames it produces as BMP files.
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "Boot.h"
#include "DiscImage.h"
#include "FaultReport.h"
#include "FrameWriter.h"
#include "System.h"

namespace {

void usage(const char* argv0)
{
	std::fprintf(stderr,
		"usage: %s <disc.cue|disc.iso|HYPER.EXE|image.bin> [options]\n"
		"  --exe NAME          file to run from a disc image (default HYPER.EXE)\n"
		"  --raw               load the file as a flat binary instead of reading a disc image\n"
		"  --load ADDR         load address (default 0xA00901FC)\n"
		"  --entry ADDR        entry PC (default 0xA0091000; the load address with --raw)\n"
		"  --cr3 VALUE         set EXCPvec before the run (default: left to the program)\n"
		"  --sp VALUE          set r0 before the run (default: left to the program)\n"
		"  --ram-mib N         RAM size in MiB, a power of two (default 16)\n"
		"  --frames N          stop after N frames (default 60)\n"
		"  --max-instr N       stop after N instructions (default: no limit beyond --frames)\n"
		"  --out DIR           folder for frame_NNNNNN.bmp (default frames)\n"
		"  --every K           write every Kth frame (default 1)\n"
		"  --little-endian | --big-endian   byte order of the image (default little-endian)\n"
		"  --trace N           print the last N executed instructions at the end\n"
		"  --stop-on-unimpl    stop at the first undefined/unimplemented instruction\n"
		"  --stop-on-hang      stop when the program branches to itself in a loop\n"
		"ADDR/VALUE accept 0x hex or decimal.\n",
		argv0);
}

struct Options
{
	std::string input;
	std::string exeName = "HYPER.EXE";
	bool raw = false;
	BootParams boot;
	bool entryGiven = false;
	uint32_t ramMiB = 16;
	uint64_t frames = 60;
	uint64_t maxInstr = 0;
	std::string outDir = "frames";
	uint64_t every = 1;
	size_t trace = 0;
	bool stopOnUnimpl = false;
	bool stopOnHang = false;
};

bool parseNumber(const std::string& s, unsigned long long& out)
{
	try
	{
		size_t used = 0;
		out = std::stoull(s, &used, 0);
		return used == s.size();
	}
	catch (const std::exception&)
	{
		return false;
	}
}

// Returns false (after printing why) on any unusable argument.
bool parseArgs(int argc, char** argv, Options& o)
{
	for (int i = 1; i < argc; ++i)
	{
		const std::string a = argv[i];
		auto value = [&](unsigned long long& v) -> bool {
			if (i + 1 >= argc || !parseNumber(argv[i + 1], v))
			{
				std::fprintf(stderr, "%s needs a numeric argument\n", a.c_str());
				return false;
			}
			++i;
			return true;
		};
		unsigned long long v = 0;

		if (a == "--exe")
		{
			if (i + 1 >= argc) { std::fprintf(stderr, "--exe needs a name\n"); return false; }
			o.exeName = argv[++i];
		}
		else if (a == "--out")
		{
			if (i + 1 >= argc) { std::fprintf(stderr, "--out needs a folder\n"); return false; }
			o.outDir = argv[++i];
		}
		else if (a == "--raw")            o.raw = true;
		else if (a == "--little-endian")  o.boot.littleEndian = true;
		else if (a == "--big-endian")     o.boot.littleEndian = false;
		else if (a == "--stop-on-unimpl") o.stopOnUnimpl = true;
		else if (a == "--stop-on-hang")   o.stopOnHang = true;
		else if (a == "--load")           { if (!value(v)) return false; o.boot.loadAddr = (uint32_t)v; }
		else if (a == "--entry")          { if (!value(v)) return false; o.boot.entryPc = (uint32_t)v; o.entryGiven = true; }
		else if (a == "--cr3")            { if (!value(v)) return false; o.boot.cr3 = (uint32_t)v; }
		else if (a == "--sp")             { if (!value(v)) return false; o.boot.sp = (uint32_t)v; }
		else if (a == "--ram-mib")        { if (!value(v)) return false; o.ramMiB = (uint32_t)v; }
		else if (a == "--frames")         { if (!value(v)) return false; o.frames = v; }
		else if (a == "--max-instr")      { if (!value(v)) return false; o.maxInstr = v; }
		else if (a == "--every")          { if (!value(v)) return false; o.every = v ? v : 1; }
		else if (a == "--trace")          { if (!value(v)) return false; o.trace = (size_t)v; }
		else if (!a.empty() && a[0] == '-')
		{
			std::fprintf(stderr, "unknown option %s\n", a.c_str());
			return false;
		}
		else if (o.input.empty())
			o.input = a;
		else
		{
			std::fprintf(stderr, "unexpected argument %s\n", a.c_str());
			return false;
		}
	}
	if (o.input.empty())
		return false;
	if (o.ramMiB == 0 || (o.ramMiB & (o.ramMiB - 1)) != 0 || o.ramMiB > 128)
	{
		std::fprintf(stderr, "--ram-mib must be a power of two between 1 and 128\n");
		return false;
	}
	return true;
}

bool readWholeFile(const std::string& path, std::vector<uint8_t>& out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f)
		return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return !out.empty();
}

} // namespace

int main(int argc, char** argv)
{
	Options o;
	if (argc < 2 || !parseArgs(argc, argv, o))
	{
		usage(argv[0]);
		return 2;
	}

	// Read the program. Nothing from the image contents is printed.
	std::vector<uint8_t> exe;
	const char* kind = "raw binary";
	if (o.raw)
	{
		if (!readWholeFile(o.input, exe))
		{
			std::fprintf(stderr, "cannot read %s\n", o.input.c_str());
			return 1;
		}
		if (!o.entryGiven)
			o.boot.entryPc = o.boot.loadAddr;
	}
	else
	{
		DiscImage disc;
		if (!disc.open(o.input) || !disc.readFile(o.exeName, exe))
		{
			std::fprintf(stderr, "%s\n", disc.error().c_str());
			return 1;
		}
		kind = disc.isBare() ? "bare executable" : "disc image";
	}

	System sys(o.ramMiB << 20);
	sys.bus.cpu.setStopOnUnimplemented(o.stopOnUnimpl);
	sys.bus.cpu.setStopOnHang(o.stopOnHang);
	sys.bus.cpu.setTraceDepth(o.trace);

	if (!sys.boot(exe.data(), exe.size(), o.boot))
	{
		std::fprintf(stderr, "image of %zu bytes does not fit at 0x%08X in %u MiB of RAM\n",
		             exe.size(), o.boot.loadAddr, o.ramMiB);
		return 1;
	}

	std::printf("input      : %s (%s)\n", o.input.c_str(), kind);
	std::printf("executable : %zu bytes\n", exe.size());
	std::printf("load       : 0x%08X\n", o.boot.loadAddr);
	std::printf("entry PC   : 0x%08X\n", o.boot.entryPc);
	if (o.boot.cr3) std::printf("cr3        : 0x%08X\n", *o.boot.cr3); else std::printf("cr3        : program-set\n");
	if (o.boot.sp)  std::printf("sp (r0)    : 0x%08X\n", *o.boot.sp);  else std::printf("sp (r0)    : program-set\n");
	std::printf("endianness : %s\n", o.boot.littleEndian ? "little" : "big");
	std::printf("RAM        : %u MiB\n", o.ramMiB);

	uint64_t written = 0;
	auto onFrame = [&](const std::vector<PPU::Color>& fb, int w, int h, uint64_t n) {
		if (n % o.every != 0)
			return;
		char name[64];
		std::snprintf(name, sizeof(name), "frame_%06llu.bmp", (unsigned long long)n);
		const std::string path = o.outDir + "/" + name;
		if (frame_io::writeBMP(path, fb, w, h))
		{
			std::printf("wrote %s (%dx%d)\n", path.c_str(), w, h);
			++written;
		}
		else
			std::fprintf(stderr, "could not write %s\n", path.c_str());
	};

	const System::RunStats s = sys.run(o.frames, o.maxInstr, onFrame);

	std::printf("frames %llu, instructions %llu, CPU cycles %llu, stopped by %s, final pc 0x%08X\n",
	            (unsigned long long)s.frames, (unsigned long long)s.instructions,
	            (unsigned long long)s.cpuCycles, System::stopReasonName(s.reason), sys.bus.cpu.pc);

	const std::string faults = formatFaultEvents(sys.bus.cpu);
	if (!faults.empty())
		std::printf("fault events:\n%s", faults.c_str());
	if (o.trace)
		std::printf("last instructions:\n%s", formatTrace(sys.bus.cpu).c_str());
	const std::string unmapped = sys.bus.unmappedSummary(10);
	if (!unmapped.empty())
		std::printf("unmapped accesses (busiest pages):\n%s", unmapped.c_str());

	return (s.reason == System::StopReason::FrameLimit || s.reason == System::StopReason::InstructionLimit) ? 0 : 3;
}
