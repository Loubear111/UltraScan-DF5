#include "Viewer.h"

#include <cstdio>

#include "FrameWriter.h"
#include "raylib.h"

struct Viewer::Impl
{
	Texture2D tex{};
	bool hasTex = false;
};

namespace {
struct KeyMap { int key; const char* name; int gpButton; };
// Held-state slots in Viewer::m_held follow this order.
const KeyMap KEYS[] = {
	{ KEY_UP, "up", GAMEPAD_BUTTON_LEFT_FACE_UP },
	{ KEY_DOWN, "down", GAMEPAD_BUTTON_LEFT_FACE_DOWN },
	{ KEY_LEFT, "left", GAMEPAD_BUTTON_LEFT_FACE_LEFT },
	{ KEY_RIGHT, "right", GAMEPAD_BUTTON_LEFT_FACE_RIGHT },
	{ KEY_ENTER, "start", GAMEPAD_BUTTON_MIDDLE_RIGHT },
	{ KEY_BACKSPACE, "select", GAMEPAD_BUTTON_MIDDLE_LEFT },
	{ KEY_Z, "red", GAMEPAD_BUTTON_RIGHT_FACE_RIGHT },
	{ KEY_X, "green", GAMEPAD_BUTTON_RIGHT_FACE_DOWN },
	{ KEY_C, "blue", GAMEPAD_BUTTON_RIGHT_FACE_LEFT },
	{ KEY_V, "yellow", GAMEPAD_BUTTON_RIGHT_FACE_UP },
	{ KEY_A, "lshoulder", GAMEPAD_BUTTON_LEFT_TRIGGER_1 },
	{ KEY_S, "rshoulder", GAMEPAD_BUTTON_RIGHT_TRIGGER_1 },
	{ KEY_Q, "ltrigger", GAMEPAD_BUTTON_LEFT_TRIGGER_2 },
	{ KEY_W, "rtrigger", GAMEPAD_BUTTON_RIGHT_TRIGGER_2 },
};
const int NKEYS = (int)(sizeof(KEYS) / sizeof(KEYS[0]));
const char* const SPEED_NAMES[] = { "1x", "2x", "max" };
}

Viewer::Viewer(System& sys, const std::string& snapshotDir, int initialSpeed)
    : m_sys(sys), m_snapDir(snapshotDir), m_speed(initialSpeed < 0 ? 0 : (initialSpeed > 2 ? 2 : initialSpeed))
{
	SetTraceLogLevel(LOG_WARNING);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
	InitWindow(960, 720, "UltraScan DF5");
	if (!IsWindowReady())
		return;
	m_impl = new Impl();
	m_ok = true;
	applySpeed();
}

Viewer::~Viewer()
{
	if (m_impl)
	{
		if (m_impl->hasTex)
			UnloadTexture(m_impl->tex);
		delete m_impl;
	}
	if (m_ok)
		CloseWindow();
}

void Viewer::applySpeed()
{
	SetTargetFPS(m_speed == 0 ? 60 : (m_speed == 1 ? 120 : 0));
}

void Viewer::upload(const std::vector<PPU::Color>& fb, int w, int h)
{
	if (w != m_w || h != m_h || !m_impl->hasTex)
	{
		if (m_impl->hasTex)
			UnloadTexture(m_impl->tex);
		Image img = GenImageColor(w, h, BLACK);
		ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
		m_impl->tex = LoadTextureFromImage(img);
		UnloadImage(img);
		SetTextureFilter(m_impl->tex, TEXTURE_FILTER_POINT);
		m_impl->hasTex = true;
		m_w = w;
		m_h = h;
	}
	m_rgba.resize((size_t)w * (size_t)h * 4);
	for (size_t i = 0; i < (size_t)w * (size_t)h && i < fb.size(); ++i)
	{
		m_rgba[i * 4 + 0] = fb[i].r;
		m_rgba[i * 4 + 1] = fb[i].g;
		m_rgba[i * 4 + 2] = fb[i].b;
		m_rgba[i * 4 + 3] = 255;
	}
	UpdateTexture(m_impl->tex, m_rgba.data());
}

void Viewer::handleKeys()
{
	// Controller: push only the changes.
	for (int i = 0; i < NKEYS; ++i)
	{
		bool down = IsKeyDown(KEYS[i].key);
		if (IsGamepadAvailable(0))
			down = down || IsGamepadButtonDown(0, KEYS[i].gpButton);
		if (down != m_held[i])
		{
			m_held[i] = down;
			m_sys.setLiveButton(0, KEYS[i].name, down);
		}
	}

	if (IsKeyPressed(KEY_SPACE)) m_paused = !m_paused;
	if (IsKeyPressed(KEY_PERIOD)) { m_paused = true; m_step = true; }
	if (IsKeyPressed(KEY_TAB)) { m_speed = (m_speed + 1) % 3; applySpeed(); }
	if (IsKeyPressed(KEY_F5)) m_overlay = !m_overlay;
	if (IsKeyPressed(KEY_F6)) m_help = !m_help;

	bool changedLayers = false;
	const int fk[4] = { KEY_F1, KEY_F2, KEY_F3, KEY_F4 };
	for (int i = 0; i < 4; ++i)
		if (IsKeyPressed(fk[i]))
		{
			m_sys.bus.ppu.debugHide ^= (1u << i);
			changedLayers = true;
		}
	if (changedLayers && m_impl->hasTex)
	{
		// Re-render the current state so the toggle shows even while paused.
		int w = 0, h = 0;
		m_last = m_sys.composeFrame(w, h);
		upload(m_last, w, h);
	}

	if (IsKeyPressed(KEY_P) && !m_last.empty())
	{
		char name[96];
		std::snprintf(name, sizeof(name), "/snap_%06llu_%02u.png", (unsigned long long)m_frameNo, m_snaps++);
		const std::string path = m_snapDir + name;
		if (frame_io::writePNG(path, m_last, m_w, m_h))
			std::printf("saved %s (%dx%d)\n", path.c_str(), m_w, m_h);
		else
			std::fprintf(stderr, "could not save %s\n", path.c_str());
	}
}

void Viewer::draw()
{
	BeginDrawing();
	ClearBackground(Color{ 16, 16, 16, 255 });
	if (m_impl->hasTex)
	{
		// 640x240 (HVGA) pixels are twice as tall as wide on a TV: stretch it to 4:3.
		const float aw = (float)m_w;
		const float ah = (m_w == 640 && m_h == 240) ? 480.0f : (float)m_h;
		const float sx = (float)GetScreenWidth() / aw, sy = (float)GetScreenHeight() / ah;
		const float k = sx < sy ? sx : sy;
		const float dw = aw * k, dh = ah * k;
		DrawTexturePro(m_impl->tex, Rectangle{ 0, 0, (float)m_w, (float)m_h },
		               Rectangle{ ((float)GetScreenWidth() - dw) / 2, ((float)GetScreenHeight() - dh) / 2, dw, dh },
		               Vector2{ 0, 0 }, 0.0f, WHITE);
	}
	if (m_overlay)
	{
		const Bus& b = m_sys.bus;
		const PPU& p = b.ppu;
		char l[4][200];
		std::snprintf(l[0], sizeof(l[0]), "frame %llu  %dx%d  speed %s%s", (unsigned long long)m_frameNo, m_w, m_h,
		              SPEED_NAMES[m_speed], m_paused ? "  PAUSED" : "");
		std::snprintf(l[1], sizeof(l[1]), "PPU %s res %d  sprites %s (max %u)%s", p.ppuEnabled() ? "on" : "OFF", (int)p.resolution(),
		              p.spriteEnabled() ? "on" : "off", p.spriteMax(), (p.debugHide & PPU::HIDE_SPRITES) ? " HIDDEN" : "");
		std::snprintf(l[2], sizeof(l[2]), "text0 %s%s  text1 %s%s  text2 %s%s", p.textEnabled(0) ? "on" : "off", (p.debugHide & 1) ? " HIDDEN" : "",
		              p.textEnabled(1) ? "on" : "off", (p.debugHide & 2) ? " HIDDEN" : "",
		              p.textEnabled(2) ? "on" : "off", (p.debugHide & 4) ? " HIDDEN" : "");
		std::snprintf(l[3], sizeof(l[3]), "TVE ctl %08X fade %02X sel %u  pc %08X", b.tve.controlReg(), b.tve.fade(), b.tve.bufferSelect(), b.cpu.pc);
		for (int i = 0; i < 4; ++i)
		{
			DrawText(l[i], 9, 9 + i * 18, 16, BLACK);
			DrawText(l[i], 8, 8 + i * 18, 16, Color{ 255, 255, 80, 255 });
		}
	}
	if (m_help)
	{
		static const char* const H[] = {
			"arrows stick  Enter start  Bksp select  Z red  X green  C blue  V yellow",
			"A/S shoulders  Q/W triggers   Space pause  . step  Tab speed",
			"F1-F3 text layers  F4 sprites  F5 overlay  F6 help  P snapshot  Esc quit",
		};
		const int y0 = GetScreenHeight() - 3 * 18 - 8;
		for (int i = 0; i < 3; ++i)
		{
			DrawText(H[i], 9, y0 + i * 18 + 1, 16, BLACK);
			DrawText(H[i], 8, y0 + i * 18, 16, Color{ 160, 255, 160, 255 });
		}
	}
	EndDrawing();
}

void Viewer::onFrame(const std::vector<PPU::Color>& fb, int w, int h, uint64_t frameNo)
{
	if (!m_ok)
		return;
	m_frameNo = frameNo;
	m_last = fb;
	upload(m_last, w, h);

	// "max" speed draws one frame in four so the emulation is not held up by the display.
	const bool skip = (m_speed == 2) && !m_paused && (frameNo % 4 != 0);
	if (!skip)
	{
		draw();
		handleKeys();
	}
	if (WindowShouldClose())
	{
		m_sys.requestStop();
		return;
	}

	while (m_paused && !m_step && !WindowShouldClose())
	{
		draw();
		handleKeys();
	}
	m_step = false;
	if (WindowShouldClose())
		m_sys.requestStop();
}
