// PlayStation 4 full-game entry point. Hardware bring-up lives in Ps4Bootstrap;
// game code only starts once modules, Piglet and the GLES pipeline are up.
#ifdef PS4_PLATFORM

#include "platform/Log.h"
#include "client/Minecraft.h"
#include "java/String.h"
#include "ps4/system/Ps4Bootstrap.h"

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	if (!Ps4Bootstrap::initialize())
		return 1;

	MC_LOG_INFO("ps4", "handing off to Minecraft::start()\n");
	jstring username = "Player";
	jstring auth = "-";
	Minecraft::start(&username, &auth);
	MC_LOG_INFO("ps4", "Minecraft::start returned; exiting\n");

	Ps4Bootstrap::shutdown();
	return 0;
}

#endif // PS4_PLATFORM
