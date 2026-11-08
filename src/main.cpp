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
		"       %s --bios <bios.bin> [disc.cue] [options]   (boot from a BIOS dump)\n"
		"  --bios FILE         boot from this BIOS image, mapped at 0x10000000, little-endian\n"
		"  --reset-pc ADDR     with --bios: start PC and cr3 (default 0x9F000000, as in MAME)\n"
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
		"  --cycles-per-instr N  CPU cycles charged per instruction (default 6; MAME charges 1)\n"
		"  --log-io N          keep and print the first N unmapped accesses (address, value, PC)\n"
		"  --log-calls         list calls the program makes into the BIOS area 0x800-0x8FFFF\n"
		"  --stop-on-call      stop at the first such call (implies --log-calls)\n"
		"  --stub-calls        answer such calls with r4 = 0 and carry on (implies --log-calls)\n"
		"  --stub-ramp N       with --stub-calls: answer the k-th call to a target with k*N (default 0)\n"
		"  --press NAME[:PORT]@FRAME[+COUNT]  hold a controller button from FRAME for COUNT frames (default 6); repeatable.\n"
		"                      names: blue start select lshoulder rshoulder ltrigger rtrigger yellow red green up down left right\n"
		"ADDR/VALUE accept 0x hex or decimal.\n",
		argv0, argv0);
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
	bool logCalls = false;
	bool stopOnCall = false;
	bool stubCalls = false;
	uint32_t stubRamp = 0;
	std::vector<std::string> presses;
	size_t logIo = 0;
	uint32_t cyclesPerInstr = System::CYCLES_PER_INSTRUCTION;
	std::string bios;
	uint32_t resetPc = System::BIOS_RESET_PC;
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
		else if (a == "--bios")
		{
			if (i + 1 >= argc) { std::fprintf(stderr, "--bios needs a file\n"); return false; }
			o.bios = argv[++i];
		}
		else if (a == "--cycles-per-instr") { if (!value(v) || v == 0) return false; o.cyclesPerInstr = (uint32_t)v; }
		else if (a == "--log-io")         { if (!value(v)) return false; o.logIo = (size_t)v; }
		else if (a == "--reset-pc")       { if (!value(v)) return false; o.resetPc = (uint32_t)v; }
		else if (a == "--raw")            o.raw = true;
		else if (a == "--little-endian")  o.boot.littleEndian = true;
		else if (a == "--big-endian")     o.boot.littleEndian = false;
		else if (a == "--stop-on-unimpl") o.stopOnUnimpl = true;
		else if (a == "--stop-on-hang")   o.stopOnHang = true;
		else if (a == "--log-calls")      o.logCalls = true;
		else if (a == "--stop-on-call")   o.stopOnCall = true;
		else if (a == "--stub-calls")     o.stubCalls = true;
		else if (a == "--press")          { if (i + 1 >= argc) { std::fprintf(stderr, "--press needs NAME@FRAME[+COUNT]\n"); return false; } o.presses.push_back(argv[++i]); }
		else if (a == "--stub-ramp")      { if (!value(v)) return false; o.stubRamp = (uint32_t)v; o.stubCalls = true; }
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
	if (o.input.empty() && o.bios.empty())
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

bool addPresses(System& sys, const Options& o)
{
	for (const std::string& spec : o.presses)
	{
		std::string err;
		if (!sys.addPress(spec, &err))
		{
			std::fprintf(stderr, "--press: %s\n", err.c_str());
			return false;
		}
	}
	return true;
}

int runAndReport(System& sys, const Options& o)
{
	sys.bus.ioLogLimit = o.logIo;
	sys.cyclesPerInstruction = o.cyclesPerInstr;
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

	if (!sys.bus.uartOutput.empty())
		std::printf("UART output (%zu bytes):\n%s\n---\n", sys.bus.uartOutput.size(), sys.bus.uartOutput.c_str());

	if (sys.logCalls)
	{
		std::printf("BIOS-area calls: %llu in total, %zu distinct targets\n",
		            (unsigned long long)sys.totalCalls(), sys.callStats().size());
		for (const System::CallStat& c : sys.callStats())
			std::printf("  0x%08X  x%-6llu first at instr %llu (frame %llu) from 0x%08X  r4-r7 = %08X %08X %08X %08X\n",
			            c.target, (unsigned long long)c.count, (unsigned long long)c.first.instruction,
			            (unsigned long long)c.first.frame, c.first.caller,
			            c.first.args[0], c.first.args[1], c.first.args[2], c.first.args[3]);
		const auto& seq = sys.callSequence();
		const size_t shown = seq.size() < 40 ? seq.size() : 40;
		if (shown)
			std::printf("call sequence (first %zu of %llu):\n", shown, (unsigned long long)sys.totalCalls());
		for (size_t i = 0; i < shown; ++i)
			std::printf("  #%-3zu 0x%08X from 0x%08X  r4=%08X r5=%08X  instr %llu\n", i, seq[i].target, seq[i].caller,
			            seq[i].args[0], seq[i].args[1], (unsigned long long)seq[i].instruction);
	}

	const std::string faults = formatFaultEvents(sys.bus.cpu);
	if (!faults.empty())
		std::printf("fault events:\n%s", faults.c_str());
	if (o.trace)
		std::printf("last instructions:\n%s", formatTrace(sys.bus.cpu).c_str());
	const std::string unmapped = sys.bus.unmappedSummary(10);
	if (!unmapped.empty())
		std::printf("unmapped accesses (busiest pages):\n%s", unmapped.c_str());
	if (!sys.bus.ioLog.empty())
	{
		std::printf("unmapped access log (%zu entries):\n", sys.bus.ioLog.size());
		for (const Bus::IoAccess& e : sys.bus.ioLog)
			std::printf("  %c%u 0x%08X %s 0x%08X  pc 0x%08X\n", e.write ? 'W' : 'R', (unsigned)e.size * 8u, e.addr,
			            e.write ? "<-" : "->", e.value, e.pc);
	}

	return (s.reason == System::StopReason::FrameLimit || s.reason == System::StopReason::InstructionLimit ||
	        s.reason == System::StopReason::BiosCall) ? 0 : 3;
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
	if (!o.bios.empty())
	{
		// BIOS boot: the disc image (if any) is for the CD controller, not loaded here.
		std::vector<uint8_t> bios;
		if (!readWholeFile(o.bios, bios))
		{
			std::fprintf(stderr, "cannot read %s\n", o.bios.c_str());
			return 1;
		}
		System sys(o.ramMiB << 20);
		sys.bus.cpu.setStopOnUnimplemented(o.stopOnUnimpl);
		sys.bus.cpu.setStopOnHang(o.stopOnHang);
		sys.bus.cpu.setTraceDepth(o.trace);
		sys.logCalls   = o.logCalls || o.stopOnCall || o.stubCalls;
		sys.stopOnCall = o.stopOnCall;
		sys.stubCalls  = o.stubCalls;
		sys.stubRamp   = o.stubRamp;
		if (!addPresses(sys, o)) return 2;
		DiscImage disc;
		if (!o.input.empty())
		{
			if (!disc.open(o.input) || disc.isBare())
			{
				std::fprintf(stderr, "cannot use %s as a disc: %s\n", o.input.c_str(),
				             disc.isBare() ? "not a disc image" : disc.error().c_str());
				return 1;
			}
		}
		if (!sys.bootBios(bios.data(), bios.size(), o.resetPc))
		{
			std::fprintf(stderr, "BIOS of %zu bytes does not fit the ROM window\n", bios.size());
			return 1;
		}
		std::printf("BIOS       : %s (%zu bytes at 0x%08X)\n", o.bios.c_str(), bios.size(), Bus::ROM_PHYS);
		std::printf("reset PC   : 0x%08X\n", o.resetPc);
		if (!o.input.empty())
			sys.attachDisc(&disc);                    // after the reset inside bootBios
		std::printf("disc       : %s\n", o.input.empty() ? "none"
		            : (o.input + " (" + std::to_string(disc.sectorCount()) + " sectors)").c_str());
		std::printf("RAM        : %u MiB, little-endian\n", o.ramMiB);
		return runAndReport(sys, o);
	}
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
	sys.logCalls   = o.logCalls || o.stopOnCall || o.stubCalls;
	sys.stopOnCall = o.stopOnCall;
	sys.stubCalls  = o.stubCalls;
	sys.stubRamp   = o.stubRamp;
	if (!addPresses(sys, o)) return 2;

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

	return runAndReport(sys, o);
}
