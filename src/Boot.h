#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

class Bus;

// ============================================================================
// Fake-BIOS boot of a HyperScan HYPER.EXE image.
//
// Defaults are MAME's quickload values, confirmed by the demos' disassembly:
// the file is copied to 0xA00901FC (the exception vector table at its start
// lands on physical 0x901FC) and execution starts at 0xA0091000.
//
// The boot touches no registers except the PC: the program's own startup code
// sets cr0, cr3 and r0 (mtcr cr0, cr3 = 0xA0000000, r0 = 0x08000000). The
// cr3 and sp members are optional overrides, applied only when given.
// What the real BIOS leaves at physical 0x1FC-0x2FC (where the vectors land
// with cr3 = 0xA0000000) is unknown, so it stays zero [inferred].
// ============================================================================
struct BootParams
{
	uint32_t loadAddr = 0xA00901FCu;
	uint32_t entryPc  = 0xA0091000u;

	// Real images are little-endian; set false only for big-endian test ROMs.
	bool littleEndian = true;

	std::optional<uint32_t> cr3;   // EXCPvec override
	std::optional<uint32_t> sp;    // r0 override
};

// Copy `data` to params.loadAddr and point the PC at params.entryPc. Returns
// false, changing nothing, when the image does not fit.
bool bootExecutable(Bus& bus, const uint8_t* data, size_t size, const BootParams& params = BootParams());

// Same, reading the image from a file (false if it cannot be read or does not fit).
bool bootExecutableFile(Bus& bus, const std::string& path, const BootParams& params = BootParams());
