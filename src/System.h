#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include "Boot.h"
#include "Bus.h"
#include "DiscImage.h"
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

	// What run() charges per instruction; defaults to CYCLES_PER_INSTRUCTION. MAME's
	// score7 core charges 1, which makes the CD servo and timers run 6x slower relative
	// to the program than here (CLI: --cycles-per-instr).
	uint32_t cyclesPerInstruction = CYCLES_PER_INSTRUCTION;

	// Applied when a run is asked for no limit at all.
	static constexpr uint64_t DEFAULT_INSTRUCTION_CAP = 100000000ull;

	enum class StopReason { FrameLimit, InstructionLimit, CpuFault, Hang, BiosCall, BadCallReturn };

	// ---- BIOS-call tracking (Phase 5, stories 19 and 20) ---------------------------
	// A program that was loaded as a bare executable calls into the BIOS through
	// stubs in low RAM (Ben 10: a table of pointers at 0xA000080C, 12 bytes apart).
	// Nothing is loaded there, so we watch for the PC entering that region from
	// outside it. The region starts above the exception vectors (cr3 = 0xA0000000
	// puts them at 0x200-0x5FF) so interrupts are not mistaken for calls. [Ben 10
	// evidence only; the real BIOS layout is unverified.]
	static constexpr uint32_t CALL_REGION_LO = 0x00000800u;
	static constexpr uint32_t CALL_REGION_HI = 0x00090000u;
	static constexpr size_t   CALL_SEQUENCE_CAP = 4096;   // calls kept in order

	struct CallEvent
	{
		uint32_t target = 0;                // PC on entry
		uint32_t caller = 0;                // r3 on entry (the link register)
		uint32_t args[4] = { 0, 0, 0, 0 };  // r4..r7 on entry
		uint64_t instruction = 0;           // instruction count when it happened
		uint64_t frame = 0;                 // frames completed so far
	};

	struct CallStat
	{
		uint32_t target = 0;
		uint64_t count = 0;
		CallEvent first;
	};

	// Any one of these turns tracking on.
	bool logCalls    = false;   // record every entry into the region
	bool stopOnCall  = false;   // stop at the first entry (StopReason::BiosCall)
	bool stubCalls   = false;   // log, set r4 = 0 and return through r3 so the run goes on
	uint32_t stubRamp = 0;      // with stubCalls: r4 = (earlier calls to this target) * stubRamp

	const std::vector<CallStat>&  callStats() const    { return m_callStats; }     // by first appearance
	const std::vector<CallEvent>& callSequence() const { return m_callSequence; }  // first CALL_SEQUENCE_CAP
	uint64_t totalCalls() const                        { return m_totalCalls; }
	void clearCalls() { m_callStats.clear(); m_callSequence.clear(); m_totalCalls = 0; }

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

	// The disc the CD servo reads from (not owned; null = no disc).
	void attachDisc(DiscImage* disc) { bus.cdservo.attachDisc(disc); }

	// Boot from a BIOS image instead of the fake loader (Phase 5, story 22): the
	// image goes into the ROM window at Bus::ROM_PHYS (MAME puts the BIOS there),
	// the bus becomes little-endian (the BIOS dump is) and the CPU starts at
	// resetPc. MAME's score7 resets with PC = EXCPVEC (cr3) = 0x9F000000, which is the
	// start of the BIOS through its mirror at 0x1F000000; its spg29x driver also sets
	// cr29 (DREG) to 0x20000000 ("disable JTAG"), without which the BIOS spins forever
	// at 0x100001B0. Both are applied here (the PDF documents neither).
	// Returns false, leaving the machine as it was, when the image does not fit.
	static constexpr uint32_t BIOS_RESET_PC = 0x9F000000u;
	static constexpr uint32_t DREG_JTAG_DISABLED = 0x20000000u;
	bool bootBios(const uint8_t* data, size_t size, uint32_t resetPc = BIOS_RESET_PC);

	// Run until maxFrames frames were produced or maxInstructions executed
	// (0 = no limit on that axis; with both 0 DEFAULT_INSTRUCTION_CAP applies).
	RunStats run(uint64_t maxFrames, uint64_t maxInstructions, const FrameCallback& onFrame = {});

	static const char* stopReasonName(StopReason r);

private:
	std::vector<CallStat>  m_callStats;
	std::vector<CallEvent> m_callSequence;
	uint64_t               m_totalCalls = 0;
};
