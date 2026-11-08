#include "System.h"

void System::reset(uint32_t entryPc)
{
	bus.cpu.reset(entryPc);
	bus.ppu.reset();
	bus.irqc.reset();
	bus.tve.reset();
	bus.timers.reset();
	bus.i2c.reset();
	bus.cdservo.reset();
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

bool System::bootBios(const uint8_t* data, size_t size, uint32_t resetPc)
{
	if (size == 0 || !bus.loadImage(data, size, Bus::ROM_PHYS))
		return false;
	bus.setLittleEndian(true);
	uint32_t mirror = 1;
	while (mirror < size)
		mirror <<= 1;                  // a 1 MiB BIOS repeats every 1 MiB
	bus.romMirror = mirror;
	reset(resetPc);
	bus.cpu.cr[3] = resetPc;                       // EXCPVEC = PC at reset (MAME)
	bus.cpu.cr[29] = DREG_JTAG_DISABLED;           // MAME's driver sets this at machine reset
	return true;
}

System::RunStats System::run(uint64_t maxFrames, uint64_t maxInstructions, const FrameCallback& onFrame)
{
	if (maxFrames == 0 && maxInstructions == 0)
		maxInstructions = DEFAULT_INSTRUCTION_CAP;

	RunStats s;
	const bool trackCalls = logCalls || stopOnCall || stubCalls;
	auto inCallRegion = [](uint32_t pc) {
		const uint32_t p = pc & 0x1FFFFFFFu;   // ignore the cached/uncached window bits
		return p >= CALL_REGION_LO && p < CALL_REGION_HI;
	};
	bool wasInRegion = trackCalls && inCallRegion(bus.cpu.pc);
	for (;;)
	{
		bool stubbed = false;
		if (trackCalls)
		{
			const bool in = inCallRegion(bus.cpu.pc);
			if (in && !wasInRegion)
			{
				CallEvent e;
				e.target = bus.cpu.pc;
				e.caller = bus.cpu.regs[3];
				for (int i = 0; i < 4; ++i)
					e.args[i] = bus.cpu.regs[4 + i];
				e.instruction = s.instructions;
				e.frame = s.frames;
				++m_totalCalls;
				if (m_callSequence.size() < CALL_SEQUENCE_CAP)
					m_callSequence.push_back(e);
				CallStat* stat = nullptr;
				for (CallStat& c : m_callStats)
					if (c.target == e.target) { stat = &c; break; }
				if (!stat)
				{
					CallStat c;
					c.target = e.target;
					c.first = e;
					m_callStats.push_back(c);
					stat = &m_callStats.back();
				}
				++stat->count;

				if (stubCalls)
				{
					// Return to the caller with r4 = 0. A link register that points back
					// into the region (or at 0) would loop forever, so stop instead.
					if (inCallRegion(e.caller) || (e.caller & 0x1FFFFFFFu) == 0)
					{
						s.reason = StopReason::BadCallReturn;
						break;
					}
					// r4 = 0, or with stubRamp the number of earlier calls to this same target
					// times stubRamp, which lets a poll for a growing tick count finish.
					bus.cpu.regs[4] = (uint32_t)((stat->count - 1) * stubRamp);
					bus.cpu.pc = e.caller;
					stubbed = true;
				}
				else if (stopOnCall)
				{
					s.reason = StopReason::BiosCall;
					break;
				}
			}
			wasInRegion = inCallRegion(bus.cpu.pc);
		}

		if (!stubbed)
			bus.cpu.clock();
		bus.tick(cyclesPerInstruction);
		++s.instructions;
		s.cpuCycles += cyclesPerInstruction;

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
	case StopReason::BiosCall:         return "BIOS call";
	case StopReason::BadCallReturn:    return "BIOS call with no usable return address";
	}
	return "unknown";
}
