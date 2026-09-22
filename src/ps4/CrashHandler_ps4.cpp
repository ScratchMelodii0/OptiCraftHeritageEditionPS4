// CrashHandler_ps4.cpp — PS4 implementation of CrashHandler::Crash().
//
// The message goes to the klog and /data/opticraft/log.txt (Ps4DebugLog), and
// a system notification tells the player the game stopped and where the log
// is, since the GL context may be the thing that failed and cannot be trusted
// to draw an error screen. The process then exits back to the home screen.
#ifdef PS4_PLATFORM

#include "pc/CrashHandler.h"
#include "ps4/system/Ps4DebugLog.h"

#include <stdint.h>
#include <orbis/libkernel.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace CrashHandler
{

void Crash(const std::string &message, const std::string &stackTrace)
{
	Ps4DebugLog::printf("[ps4] CRASH: %s\n", message.c_str());
	if (!stackTrace.empty())
		Ps4DebugLog::printf("[ps4] trace:\n%s\n", stackTrace.c_str());

	OrbisNotificationRequest request;
	std::memset(&request, 0, sizeof(request));
	request.type = NotificationRequest;
	request.targetId = -1;
	std::snprintf(request.message, sizeof(request.message),
	              "OptiCraft has crashed:\n%.300s\nSee /data/opticraft/log.txt", message.c_str());
	sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);

	Ps4DebugLog::closeFile();
	std::exit(1);
}

} // namespace CrashHandler

#endif // PS4_PLATFORM
