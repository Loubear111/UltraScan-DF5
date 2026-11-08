#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "System.h"

// ============================================================================
// Live display and debug viewer (Phase 5, story 39). An optional raylib window,
// compiled in only with `make VIEWER=1` (defines HAVE_VIEWER), so the default build
// and the tests need no graphics library.
//
// The viewer is driven from System::run's frame callback: onFrame() uploads the frame,
// handles keys, draws, and paces the run. Closing the window asks System to stop.
//
//   Controller   arrows = analog stick, Enter = start, Backspace = select,
//                Z/X/C/V = red/green/blue/yellow, A/S = left/right shoulder,
//                Q/W = left/right trigger (gamepad 0: d-pad, face buttons, bumpers,
//                Start/Back do the same). The bit layout is MAME's (see System.h).
//   Run          Space = pause/resume, '.' = step one frame while paused,
//                Tab = speed 1x / 2x / max (max draws every 4th frame)
//   Debug        F1/F2/F3 = hide text layer 0/1/2, F4 = hide sprites,
//                F5 = overlay on/off, F6 = key help on/off, P = save a PNG snapshot
//                Esc = quit
// ============================================================================
class Viewer
{
public:
	Viewer(System& sys, const std::string& snapshotDir, int initialSpeed = 1);
	~Viewer();
	Viewer(const Viewer&) = delete;
	Viewer& operator=(const Viewer&) = delete;

	bool ok() const { return m_ok; }

	// Frame callback for System::run.
	void onFrame(const std::vector<PPU::Color>& fb, int w, int h, uint64_t frameNo);

private:
	void handleKeys();
	void draw();
	void upload(const std::vector<PPU::Color>& fb, int w, int h);
	void applySpeed();

	System& m_sys;
	std::string m_snapDir;
	bool m_ok = false;
	bool m_paused = false;
	bool m_step = false;
	bool m_overlay = true;
	bool m_help = false;
	int  m_speed = 1;              // 0 = 1x, 1 = 2x, 2 = max
	uint64_t m_frameNo = 0;
	int m_w = 0, m_h = 0;
	const std::vector<PPU::Color>* m_cur = nullptr;   // the frame being shown (valid only inside onFrame)
	std::vector<PPU::Color> m_own;                    // a re-rendered frame after a layer toggle
	std::vector<uint8_t> m_rgba;
	unsigned m_snaps = 0;
	bool m_held[32] = {};
	struct Impl;
	Impl* m_impl = nullptr;
};
