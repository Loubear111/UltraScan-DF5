#include "System.h"

void System::reset(uint32_t entryPc)
{
	bus.cpu.reset(entryPc);
	bus.ppu.reset();
	bus.irqc.reset();
	bus.tve.reset();
	bus.video.reset();
	bus.frameReady = false;
	bus.updateIRQ();
}

bool System::boot(const uint8_t* data, size_t size, const BootParams& params)
{
	// loadImage is all-or-nothing, so a failed load leaves the machine untouched.
	// The reset that follows does not clear RAM; bootExecutable then copies the
	// image again (cheap) and applies the PC, byte order and register overrides
	// on top of the freshly reset CPU.
	if (size == 0 || !bus.loadImage(data, size, params.loadAddr))
		return false;
	reset(params.entryPc);
	return bootExecutable(bus, data, size, params);
}

System::RunStats System::run(uint64_t maxFrames, uint64_t maxInstructions, const FrameCallback& onFrame)
{
	if (maxFrames == 0 && maxInstructions == 0)
		maxInstructions = DEFAULT_INSTRUCTION_CAP;

	RunStats s;
	for (;;)
	{
		bus.cpu.clock();
		bus.tick(CYCLES_PER_INSTRUCTION);
		++s.instructions;
		s.cpuCycles += CYCLES_PER_INSTRUCTION;

		if (bus.cpu.stopRequested())
		{
			s.reason = bus.cpu.hangDetected() ? StopReason::Hang : StopReason::CpuFault;
			break;
		}

		if (bus.takeFrameReady())
		{
			std::vector<PPU::Color> ppuFrame;
			const std::vector<PPU::Color>* pf = nullptr;
			if (bus.ppu.ppuEnabled())
			{
				ppuFrame = bus.ppu.renderFrame(BusMemory(bus));
				pf = &ppuFrame;
			}
			int w = 0, h = 0;
			const std::vector<PPU::Color> fb = bus.tve.compose(bus, pf, w, h);
			if (onFrame)
				onFrame(fb, w, h, s.frames);
			++s.frames;
			if (maxFrames && s.frames >= maxFrames)
			{
				s.reason = StopReason::FrameLimit;
				break;
			}
		}

		if (maxInstructions && s.instructions >= maxInstructions)
		{
			s.reason = StopReason::InstructionLimit;
			break;
		}
	}
	return s;
}

const char* System::stopReasonName(StopReason r)
{
	switch (r)
	{
	case StopReason::FrameLimit:       return "frame limit";
	case StopReason::InstructionLimit: return "instruction limit";
	case StopReason::CpuFault:         return "CPU fault";
	case StopReason::Hang:             return "hang";
	}
	return "unknown";
}
