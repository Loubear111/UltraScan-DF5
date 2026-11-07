#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include "Boot.h"
#include "Bus.h"
#include "ppu.h"

// ============================================================================
// System: the machine plus its run loop.
//
// One loop iteration executes one CPU instruction (or an IRQ entry, or a halted
// no-op), then advances video time by CYCLES_PER_INSTRUCTION. Each vblank start
// builds a frame with TVE::compose (the PPU frame, when the PPU is enabled, with
// the TV encoder's RGB565 framebuffer on top) and hands it to the frame callback.
//
// The loop always stops: at a frame limit, an instruction limit, a CPU fault
// (stop-on-unimplemented) or a detected hang (stop-on-hang).
// ============================================================================
class System
{
public:
	// 6 cycles per instruction is the figure HyperScanEmu uses (MAME also uses a
	// placeholder). The frame period is not shortened to compensate.
	static constexpr uint32_t CYCLES_PER_INSTRUCTION = 6;

	// Applied when a run is asked for no limit at all.
	static constexpr uint64_t DEFAULT_INSTRUCTION_CAP = 100000000ull;

	enum class StopReason { FrameLimit, InstructionLimit, CpuFault, Hang };

	struct RunStats
	{
		uint64_t   frames       = 0;
		uint64_t   instructions = 0;
		uint64_t   cpuCycles    = 0;
		StopReason reason       = StopReason::InstructionLimit;
	};

	using FrameCallback = std::function<void(const std::vector<PPU::Color>& frame, int w, int h, uint64_t frameNo)>;

	explicit System(uint32_t ramBytes = Bus::SYSTEM_RAM_SIZE) : bus(ramBytes) {}

	Bus bus;

	// Power-on state for the CPU, PPU, interrupt controller, TV encoder and video
	// timing, then PC = entryPc. RAM is left alone (the image is loaded before reset).
	void reset(uint32_t entryPc);

	// Reset, then load an image with the fake BIOS (see Boot.h). Returns false,
	// leaving the machine as it was, when the image does not fit.
	bool boot(const uint8_t* data, size_t size, const BootParams& params = BootParams());

	// Run until maxFrames frames were produced or maxInstructions executed
	// (0 = no limit on that axis; with both 0 DEFAULT_INSTRUCTION_CAP applies).
	RunStats run(uint64_t maxFrames, uint64_t maxInstructions, const FrameCallback& onFrame = {});

	static const char* stopReasonName(StopReason r);
};
