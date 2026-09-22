// GameSettingsBackend_PS4.cpp — DualShock 4 default bindings; otherwise the
// desktop option set (the PS4 has no need for the PS2's fixed render distance).
#include "platform/GameSettingsBackend.h"

#include <ostream>

#include "lwjgl/Keyboard.h"
#include "net/minecraft/src/GameSettings.h"
#include "net/minecraft/src/KeyBinding.h"
#include "ps4/input/Ps4PadKeyCodes.h"

namespace
{
void applyControllerDefaults(GameSettings& settings)
{
	settings.keyBindForward->keyCode = PS4_KEY_DPAD_UP;
	settings.keyBindLeft->keyCode = PS4_KEY_DPAD_LEFT;
	settings.keyBindBack->keyCode = PS4_KEY_DPAD_DOWN;
	settings.keyBindRight->keyCode = PS4_KEY_DPAD_RIGHT;
	settings.keyBindJump->keyCode = PS4_KEY_CROSS;
	settings.keyBindInventory->keyCode = PS4_KEY_SQUARE;
	settings.keyBindDrop->keyCode = PS4_KEY_TRIANGLE;
	settings.keyBindSneak->keyCode = PS4_KEY_CIRCLE;
}

// A keyboard key in options.txt (a file copied from a PC install, or written
// before controller bindings existed) is unreachable on a pad: fall back.
void migrateKey(KeyBinding* binding, int_t fallback)
{
	if (binding->keyCode < lwjgl::Keyboard::KEY_MAX)
		binding->keyCode = fallback;
}
}

void platformGameSettingsInitialize(GameSettings& settings)
{
	applyControllerDefaults(settings);
}

void platformGameSettingsResetControlBindings(GameSettings& settings)
{
	applyControllerDefaults(settings);
}

int_t platformGameSettingsDefaultChunkUpdates() { return 1; }
int_t platformGameSettingsDefaultConnectedTextures() { return 2; }
int_t platformGameSettingsCycleRenderDistance(int_t current, int_t delta) { return (current + delta) & 3; }
int_t platformGameSettingsClampRenderDistance(int_t value) { return value; }
int_t platformGameSettingsClampFineRenderDistance(int_t value) { return value; }

void platformGameSettingsUpdateRenderDistanceFromFine(int_t fineDistance, int_t& renderDistance)
{
	renderDistance = 3;
	if (fineDistance > 32) renderDistance = 2;
	if (fineDistance > 64) renderDistance = 1;
	if (fineDistance > 128) renderDistance = 0;
}

bool platformGameSettingsAnaglyphValue(bool, bool requested) { return requested; }
bool platformGameSettingsLoadOption(GameSettings&, const std::string&, const std::string&) { return false; }

void platformGameSettingsFinalizeLoad(GameSettings& settings)
{
	migrateKey(settings.keyBindForward, PS4_KEY_DPAD_UP);
	migrateKey(settings.keyBindLeft, PS4_KEY_DPAD_LEFT);
	migrateKey(settings.keyBindBack, PS4_KEY_DPAD_DOWN);
	migrateKey(settings.keyBindRight, PS4_KEY_DPAD_RIGHT);
	migrateKey(settings.keyBindJump, PS4_KEY_CROSS);
	migrateKey(settings.keyBindInventory, PS4_KEY_SQUARE);
	migrateKey(settings.keyBindDrop, PS4_KEY_TRIANGLE);
	migrateKey(settings.keyBindSneak, PS4_KEY_CIRCLE);
}

void platformGameSettingsSyncControllerBindings(const GameSettings&) {}
void platformGameSettingsAddKnownKeys(std::unordered_set<std::string>&) {}
void platformGameSettingsWriteOptions(const GameSettings&, std::ostream&) {}
