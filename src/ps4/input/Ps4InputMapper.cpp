// Ps4InputMapper.cpp — DualShock 4 -> lwjgl Keyboard/Mouse events.
//
// A port of the PS2 mapper (ps2/input/Ps2InputMapper.cpp) so both consoles
// share one control scheme and the same shared-GUI behaviour
// (PLATFORM_STICK_POINTER_UI):
//
//   Gameplay   left stick / D-pad  -> the four movement bindings
//              right stick         -> camera (Mouse motion)
//              R2 / L2             -> attack / use (mouse buttons)
//              R1 / L1             -> next / previous hotbar slot (wheel)
//              R3                  -> toggle perspective (F5)
//              OPTIONS             -> pause (Escape)
//              touchpad click      -> debug overlay (F3)
//              every face/shoulder/stick button also raises its own
//              PS4_KEY_* code, so Jump/Sneak/Inventory/Drop follow whatever
//              the player bound in Controls.
//
//   Menus      left stick / D-pad  -> software cursor
//              Cross / Square      -> left / right click
//              Circle / Triangle   -> back (Escape)
//              OPTIONS             -> Enter
//              right stick, R3+D-pad -> scroll wheel
//              touchpad click      -> Tab
#ifdef PS4_PLATFORM

#include "ps4/input/Ps4InputMapper.h"

#include "lwjgl/Keyboard.h"
#include "lwjgl/Mouse.h"
#include "platform/Input.h"
#include "platform/time.h"
#include "ps4/input/Ps4AnalogFilter.h"
#include "ps4/input/Ps4PadKeyCodes.h"
#include "ps4/input/Ps4PadState.h"
#include "ps4/input/Ps4Pointer.h"

namespace
{
// Camera: full deflection turns at this many Mouse units per second. The PS2
// port emits 14 units per frame; expressed per second here so the feel does
// not change with frame rate (the PS4 runs at 60 Hz, the PS2 well below).
constexpr float kCameraUnitsPerSecond = 840.0f;
constexpr int kCameraWarmupFrames = 18;

// Menu cursor speeds in framebuffer pixels per second, scaled from the PS2
// values for a 1920-wide framebuffer instead of 640.
constexpr float kDpadCursorSpeed = 800.0f;
constexpr float kStickCursorSpeed = 1150.0f;

constexpr float kMenuScrollThreshold = 0.18f;
constexpr float kMenuNavEnter = 0.60f;
constexpr float kMenuNavRelease = 0.35f;
constexpr float kMenuNavRepeatDelay = 0.30f;
constexpr float kMenuNavRepeatInterval = 0.11f;

bool s_gameplayKeyDown[256] = {};
bool s_previousMenu = false;
int s_cameraWarmup = 0;
float s_cameraRemainderX = 0.0f;
float s_cameraRemainderY = 0.0f;
float s_lastGameplayTime = 0.0f;
int s_menuAnalogDirection = 0;
float s_menuAnalogRepeat = 0.0f;
float s_buttonScrollRepeat = 0.0f;
float s_stickScrollRepeat = 0.0f;

void setKey(int key, bool down)
{
	if (key < 0 || key >= 256 || s_gameplayKeyDown[key] == down)
		return;
	s_gameplayKeyDown[key] = down;
	lwjgl::Keyboard::detail::pushKey(key, down);
}

void releaseGameplayKeys()
{
	for (int key = PS4_KEY_CROSS; key < PS4_KEY_SENTINEL_END; ++key)
		setKey(key, false);
}

void edgeKey(const Ps4PadSnapshot &p, std::uint32_t mask, int key)
{
	if (p.pressed & mask) lwjgl::Keyboard::detail::pushKey(key, true);
	if (p.released & mask) lwjgl::Keyboard::detail::pushKey(key, false);
}

void edgeButton(const Ps4PadSnapshot &p, std::uint32_t mask, int button, int x, int y)
{
	if (p.pressed & mask) lwjgl::Mouse::detail::pushButton(button, true, x, y);
	if (p.released & mask) lwjgl::Mouse::detail::pushButton(button, false, x, y);
}

float absoluteValue(float value)
{
	return value < 0.0f ? -value : value;
}

std::uint32_t directionMask(int direction)
{
	switch (direction)
	{
		case 1: return PS4_PAD_UP;
		case 2: return PS4_PAD_DOWN;
		case 3: return PS4_PAD_LEFT;
		case 4: return PS4_PAD_RIGHT;
		default: return 0;
	}
}

// Screens with their own D-pad navigation also accept the left stick: a
// deflection past kMenuNavEnter is latched as a D-pad press, repeating while
// held, until it falls back under kMenuNavRelease.
std::uint32_t updateMenuAnalogNavigation(const Ps4PadSnapshot &p, float dt, bool active)
{
	if (!active)
	{
		s_menuAnalogDirection = 0;
		s_menuAnalogRepeat = 0.0f;
		return 0;
	}

	const float x = Ps4AnalogFilter::apply(p.leftX);
	const float y = Ps4AnalogFilter::apply(p.leftY);

	if (s_menuAnalogDirection != 0)
	{
		const bool vertical = s_menuAnalogDirection <= 2;
		if ((vertical ? absoluteValue(y) : absoluteValue(x)) <= kMenuNavRelease)
		{
			s_menuAnalogDirection = 0;
			s_menuAnalogRepeat = 0.0f;
		}
	}

	if (s_menuAnalogDirection == 0)
	{
		if (absoluteValue(x) < kMenuNavEnter && absoluteValue(y) < kMenuNavEnter)
			return 0;
		if (absoluteValue(y) >= absoluteValue(x))
			s_menuAnalogDirection = y < 0.0f ? 1 : 2;
		else
			s_menuAnalogDirection = x < 0.0f ? 3 : 4;
		s_menuAnalogRepeat = kMenuNavRepeatDelay;
		return directionMask(s_menuAnalogDirection);
	}

	s_menuAnalogRepeat -= dt;
	if (s_menuAnalogRepeat > 0.0f)
		return 0;
	s_menuAnalogRepeat = kMenuNavRepeatInterval;
	return directionMask(s_menuAnalogDirection);
}

// Buttons a Controls-menu binding can learn. Circle is the rebind cancel (it
// arrives as Escape, which LegacyControlsScreen treats as "back"), and
// OPTIONS / the touchpad keep their system roles.
const struct { std::uint32_t mask; int code; } kRemappableButtons[] = {
	{ PS4_PAD_CROSS, PS4_KEY_CROSS },       { PS4_PAD_TRIANGLE, PS4_KEY_TRIANGLE },
	{ PS4_PAD_SQUARE, PS4_KEY_SQUARE },     { PS4_PAD_L1, PS4_KEY_L1 },
	{ PS4_PAD_R1, PS4_KEY_R1 },             { PS4_PAD_L2, PS4_KEY_L2 },
	{ PS4_PAD_R2, PS4_KEY_R2 },             { PS4_PAD_L3, PS4_KEY_L3 },
	{ PS4_PAD_R3, PS4_KEY_R3 },             { PS4_PAD_UP, PS4_KEY_DPAD_UP },
	{ PS4_PAD_DOWN, PS4_KEY_DPAD_DOWN },    { PS4_PAD_LEFT, PS4_KEY_DPAD_LEFT },
	{ PS4_PAD_RIGHT, PS4_KEY_DPAD_RIGHT },
};

void updateMenu(const Ps4PadSnapshot &p, bool specializedMenuNavigation)
{
	// The virtual keyboard reads the pad itself while a text field is focused.
	if (platformTextInputExclusive())
		return;

	if (platformPadRebindExclusive())
	{
		if (p.pressed & PS4_PAD_CIRCLE)
		{
			lwjgl::Keyboard::detail::pushKey(lwjgl::Keyboard::KEY_ESCAPE, true);
			lwjgl::Keyboard::detail::pushKey(lwjgl::Keyboard::KEY_ESCAPE, false);
			return;
		}
		for (const auto &entry : kRemappableButtons)
		{
			if (p.pressed & entry.mask)
			{
				lwjgl::Keyboard::detail::pushKey(entry.code, true);
				lwjgl::Keyboard::detail::pushKey(entry.code, false);
				break;
			}
		}
		return;
	}

	const float dt = Ps4Pointer::beginMenuFrame();
	const bool r3 = (p.held & PS4_PAD_R3) != 0;
	const bool slotNav = platformContainerNavigationActive();

	if (specializedMenuNavigation)
	{
		const std::uint32_t analogPressed = updateMenuAnalogNavigation(p, dt, true);
		if (analogPressed != 0)
			ps4PadLatchPressed(analogPressed);
	}
	else
	{
		updateMenuAnalogNavigation(p, dt, false);
		float dx = 0.0f, dy = 0.0f;
		if (!r3 && !slotNav && (p.held & PS4_PAD_UP)) dy -= kDpadCursorSpeed * dt;
		if (!r3 && !slotNav && (p.held & PS4_PAD_DOWN)) dy += kDpadCursorSpeed * dt;
		if (!slotNav && (p.held & PS4_PAD_LEFT)) dx -= kDpadCursorSpeed * dt;
		if (!slotNav && (p.held & PS4_PAD_RIGHT)) dx += kDpadCursorSpeed * dt;
		dx += Ps4AnalogFilter::apply(p.leftX) * kStickCursorSpeed * dt;
		dy += Ps4AnalogFilter::apply(p.leftY) * kStickCursorSpeed * dt;
		Ps4Pointer::move(dx, dy);
		Ps4Pointer::publish();
	}
	const int cx = Ps4Pointer::x();
	const int cy = Ps4Pointer::y();

	// R3 + D-pad up/down scrolls, repeating while held.
	if (r3 && (p.held & (PS4_PAD_UP | PS4_PAD_DOWN)))
	{
		bool fire = (p.pressed & (PS4_PAD_UP | PS4_PAD_DOWN)) != 0;
		s_buttonScrollRepeat -= dt;
		if (s_buttonScrollRepeat <= 0.0f) { fire = true; s_buttonScrollRepeat = 0.12f; }
		if (fire) lwjgl::Mouse::detail::pushWheel((p.held & PS4_PAD_UP) ? 1 : -1, cx, cy);
	}
	else
	{
		s_buttonScrollRepeat = 0.0f;
	}

	// Right stick scrolls, faster the further it is pushed.
	const float scroll = Ps4AnalogFilter::apply(p.rightY, kMenuScrollThreshold);
	if (scroll != 0.0f)
	{
		const float interval = 0.20f - 0.16f * absoluteValue(scroll);
		s_stickScrollRepeat -= dt;
		if (s_stickScrollRepeat <= 0.0f)
		{
			s_stickScrollRepeat = interval;
			lwjgl::Mouse::detail::pushWheel(scroll < 0.0f ? 1 : -1, cx, cy);
		}
	}
	else
	{
		s_stickScrollRepeat = 0.0f;
	}

	if (!specializedMenuNavigation)
	{
		edgeButton(p, PS4_PAD_CROSS, 0, cx, cy);
		edgeButton(p, PS4_PAD_SQUARE, 1, cx, cy);
		edgeKey(p, PS4_PAD_CIRCLE | PS4_PAD_TRIANGLE, lwjgl::Keyboard::KEY_ESCAPE);
		edgeKey(p, PS4_PAD_OPTIONS, lwjgl::Keyboard::KEY_RETURN);
	}
	edgeKey(p, PS4_PAD_TOUCHPAD, lwjgl::Keyboard::KEY_TAB);
}

void updateCamera(const Ps4PadSnapshot &p)
{
	const float now = getTimeS();
	float dt = s_lastGameplayTime > 0.0f ? now - s_lastGameplayTime : 0.0f;
	s_lastGameplayTime = now;
	if (dt < 0.0f) dt = 0.0f;
	if (dt > 0.10f) dt = 0.10f;

	if (s_cameraWarmup > 0)
	{
		--s_cameraWarmup;
		s_cameraRemainderX = s_cameraRemainderY = 0.0f;
		lwjgl::Mouse::clearDeltas();
		return;
	}

	// Keep the fractional part so slow, fine aiming still moves the camera.
	s_cameraRemainderX += Ps4AnalogFilter::apply(p.rightX) * kCameraUnitsPerSecond * dt;
	s_cameraRemainderY += Ps4AnalogFilter::apply(p.rightY) * kCameraUnitsPerSecond * dt;
	const int dx = static_cast<int>(s_cameraRemainderX);
	const int dy = static_cast<int>(s_cameraRemainderY);
	s_cameraRemainderX -= static_cast<float>(dx);
	s_cameraRemainderY -= static_cast<float>(dy);
	if (dx != 0 || dy != 0)
		lwjgl::Mouse::detail::pushMotion(0, 0, dx, dy);
}

void updateGameplay(const Ps4PadSnapshot &p)
{
	updateCamera(p);

	const float moveX = Ps4AnalogFilter::apply(p.leftX);
	const float moveY = Ps4AnalogFilter::apply(p.leftY);
	setKey(PS4_KEY_DPAD_UP, moveY < -0.05f || (p.held & PS4_PAD_UP));
	setKey(PS4_KEY_DPAD_DOWN, moveY > 0.05f || (p.held & PS4_PAD_DOWN));
	setKey(PS4_KEY_DPAD_LEFT, moveX < -0.05f || (p.held & PS4_PAD_LEFT));
	setKey(PS4_KEY_DPAD_RIGHT, moveX > 0.05f || (p.held & PS4_PAD_RIGHT));
	setKey(PS4_KEY_CROSS, (p.held & PS4_PAD_CROSS) != 0);
	setKey(PS4_KEY_CIRCLE, (p.held & PS4_PAD_CIRCLE) != 0);
	setKey(PS4_KEY_TRIANGLE, (p.held & PS4_PAD_TRIANGLE) != 0);
	setKey(PS4_KEY_SQUARE, (p.held & PS4_PAD_SQUARE) != 0);
	setKey(PS4_KEY_L1, (p.held & PS4_PAD_L1) != 0);
	setKey(PS4_KEY_R1, (p.held & PS4_PAD_R1) != 0);
	setKey(PS4_KEY_L2, (p.held & PS4_PAD_L2) != 0);
	setKey(PS4_KEY_R2, (p.held & PS4_PAD_R2) != 0);
	setKey(PS4_KEY_L3, (p.held & PS4_PAD_L3) != 0);
	setKey(PS4_KEY_R3, (p.held & PS4_PAD_R3) != 0);

	edgeButton(p, PS4_PAD_R2, 0, 0, 0);
	edgeButton(p, PS4_PAD_L2, 1, 0, 0);
	if (p.pressed & PS4_PAD_R1) lwjgl::Mouse::detail::pushWheel(-1, 0, 0);
	if (p.pressed & PS4_PAD_L1) lwjgl::Mouse::detail::pushWheel(1, 0, 0);
	edgeKey(p, PS4_PAD_OPTIONS, lwjgl::Keyboard::KEY_ESCAPE);
	edgeKey(p, PS4_PAD_R3, lwjgl::Keyboard::KEY_F5);
	edgeKey(p, PS4_PAD_TOUCHPAD, lwjgl::Keyboard::KEY_F3);

	// Gameplay has no menu consumer; drop latched presses so they do not
	// replay into the next screen that opens.
	ps4PadClearLatchedPressed();
}
}

namespace Ps4InputMapper
{
void update(bool inMenu, bool specializedMenuNavigation)
{
	const Ps4PadSnapshot &pad = ps4PadGetSnapshot();
	if (inMenu && !s_previousMenu)
	{
		releaseGameplayKeys();
		ps4PadClearLatchedPressed();
		Ps4Pointer::enterMenu();
	}
	if (!inMenu && s_previousMenu)
	{
		// The camera would otherwise swallow deltas left over from the menu
		// cursor and snap on the first gameplay frames.
		s_cameraWarmup = kCameraWarmupFrames;
		s_lastGameplayTime = 0.0f;
		lwjgl::Mouse::clearDeltas();
		ps4PadClearLatchedPressed();
		Ps4Pointer::leaveMenu();
	}
	s_previousMenu = inMenu;

	if (!pad.connected)
	{
		if (!inMenu)
			releaseGameplayKeys();
		return;
	}
	if (inMenu)
		updateMenu(pad, specializedMenuNavigation);
	else
		updateGameplay(pad);
}
}

#endif // PS4_PLATFORM
