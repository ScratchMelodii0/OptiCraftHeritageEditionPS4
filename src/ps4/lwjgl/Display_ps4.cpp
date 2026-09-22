// Display_ps4.cpp — PlayStation 4 implementation of lwjgl::Display.
//
// Ps4Bootstrap has already created the Piglet surface and the GLES pipeline
// before Minecraft::start() runs (a failure there must end at the loader, not
// halfway into the game), so create() only hooks up input. The resolution is
// the Piglet surface's: 1920x1080; the system scaler handles 720p/4K outputs.
//
// There is no window to close either: the PS button suspends the process and
// "Close Application" kills it outright, so isCloseRequested() is always false
// and saving follows the game's normal autosave/quit paths.
#ifdef PS4_PLATFORM

#include "lwjgl/Display.h"

#include "client/Minecraft.h"
#include "net/minecraft/src/GuiScreen.h"
#include "ps4/input/Ps4Input.h"
#include "ps4/render/Ps4GlesPipeline.h"
#include "ps4/system/Ps4Piglet.h"

namespace
{
bool g_created = false;
}

namespace lwjgl
{
namespace Display
{

void create()
{
	if (g_created)
		return;
	Ps4Input::initialize(Ps4Piglet::width(), Ps4Piglet::height());
	g_created = true;
}

void setDisplayMode(const DisplayMode &)
{
	// Fixed by the Piglet surface; see the header comment.
}

DisplayMode getDisplayMode()
{
	return DisplayMode(Ps4Piglet::width(), Ps4Piglet::height());
}

void setTitle(const jstring &) {}
void setFullscreen(bool) {}

bool isCloseRequested() { return false; }
bool isVisible() { return true; }
bool isActive() { return true; }

void processMessages()
{
	// Same menu/gameplay routing as the Wii and PS2 front ends: the input layer
	// is told whether a screen is open instead of guessing from mouse grab.
	Minecraft *mc = Minecraft::getMinecraft();
	const bool inMenu = (mc != nullptr && mc->currentScreen != nullptr);
	const bool specializedMenuNavigation = inMenu &&
		mc->currentScreen->usesSpecializedMenuNavigationForPlatform();

	// Recapture the camera once no menu is open and a world exists (see
	// Display_wii.cpp for why the world check matters).
	if (!inMenu && mc != nullptr && mc->theWorld != nullptr && !mc->inGameHasFocus)
		mc->setIngameFocus();

	Ps4Input::poll(inMenu, specializedMenuNavigation);
}

void swapBuffers()
{
	Ps4Gles::endFrame();
	Ps4Piglet::swapBuffers();
}

void update(bool doProcessMessages)
{
	swapBuffers();
	if (doProcessMessages)
		processMessages();
}

int_t getX() { return 0; }
int_t getY() { return 0; }
int_t getWidth() { return Ps4Piglet::width(); }
int_t getHeight() { return Ps4Piglet::height(); }

} // namespace Display
} // namespace lwjgl

#endif // PS4_PLATFORM
