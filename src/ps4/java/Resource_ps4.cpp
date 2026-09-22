// Resource_ps4.cpp — PS4 implementation of Resource::getResource().
//
// Resources are read from the data directory Ps4Paths resolved at boot
// (/data/opticraft/data or the package's /app0/data), not embedded in the
// eboot, so they can be updated without rebuilding. Callers pass paths rooted
// at the resource directory ("/terrain.png"), matching the desktop contract.
//
// The returned stream is owned by the caller, as on every other platform.
#ifdef PS4_PLATFORM

#include "java/Resource.h"
#include "java/String.h"
#include "net/minecraft/src/GameResources.h"
#include "platform/storage/PathUtils.h"

#include <stdexcept>
#include <string>

namespace Resource
{

std::istream *getResource(const jstring &name)
{
	auto input = GameResources::open(static_cast<const std::string &>(name));
	if (!input)
	{
		const std::string path = PlatformStorage::join(
			GameResources::getAssetsDir(), static_cast<const std::string &>(name));
		throw std::runtime_error(
			"Missing game resource:\n" + path +
			"\n\nThe package must contain data/assets and data/resources "
			"(build the ps4-pkg target with data/ present), or copy a data/ "
			"folder to /data/opticraft/data over FTP.");
	}
	return input.release();
}

} // namespace Resource

#endif // PS4_PLATFORM
