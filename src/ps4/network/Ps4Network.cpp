#ifdef PS4_PLATFORM

#include "ps4/network/Ps4Network.h"
#include "platform/Log.h"

#include <stdint.h>
#include <orbis/Net.h>

#include <mutex>

namespace
{
constexpr int kAfInet = 2;
// The resolver allocates its query state from this pool; 16 KB is ample for
// one lookup at a time, which is all the game ever does.
constexpr int kPoolBytes = 16 * 1024;

std::mutex s_mutex;
bool s_initialized = false;
int s_pool = -1;
}

namespace Ps4Network
{
bool initialize()
{
	std::lock_guard<std::mutex> lock(s_mutex);
	if (s_initialized)
		return true;

	// Another module (or the system) may already have initialised libSceNet;
	// that is not an error for us, so only the pool creation is decisive.
	sceNetInit();
	s_pool = sceNetPoolCreate("OptiCraftNet", kPoolBytes, 0);
	if (s_pool < 0)
	{
		MC_LOG_ERROR("net", "[PS4] sceNetPoolCreate failed: 0x%08X\n", static_cast<unsigned>(s_pool));
		return false;
	}
	s_initialized = true;
	return true;
}

bool resolveIPv4(const std::string& host, std::uint32_t& addressNetworkOrder)
{
	if (host.empty())
		return false;

	std::uint32_t numeric = 0;
	if (sceNetInetPton(kAfInet, host.c_str(), &numeric) == 1)
	{
		addressNetworkOrder = numeric;
		return true;
	}

	if (!initialize())
		return false;

	std::lock_guard<std::mutex> lock(s_mutex);
	const OrbisNetId resolver = sceNetResolverCreate("OptiCraftResolver", s_pool, 0);
	if (resolver < 0)
	{
		MC_LOG_ERROR("net", "[PS4] sceNetResolverCreate failed: 0x%08X\n", static_cast<unsigned>(resolver));
		return false;
	}
	OrbisNetInAddr address{};
	// 0 timeout / 0 retries select the resolver's own defaults.
	const int rc = sceNetResolverStartNtoa(resolver, host.c_str(), &address, 0, 0, 0);
	sceNetResolverDestroy(resolver);
	if (rc < 0)
	{
		MC_LOG_INFO("net", "[PS4] cannot resolve '%s': 0x%08X\n", host.c_str(), static_cast<unsigned>(rc));
		return false;
	}
	addressNetworkOrder = address.s_addr;
	return true;
}
}

#endif // PS4_PLATFORM
