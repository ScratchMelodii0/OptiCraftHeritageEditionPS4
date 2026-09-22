#ifdef PS4_PLATFORM

#include "ps4/system/Ps4Paths.h"

#include <sys/stat.h>

#include <cerrno>
#include <string>

namespace
{
const char* const kWritableRoot = "/data/opticraft";

bool isDirectory(const std::string& path)
{
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}
}

namespace Ps4Paths
{
bool ensureWritableRoot()
{
    if (mkdir(kWritableRoot, 0777) != 0 && errno != EEXIST)
        return false;
    return isDirectory(kWritableRoot);
}

const char* writableRoot()
{
    return kWritableRoot;
}

const char* gameDataDir()
{
    static std::string resolved;
    static bool searched = false;
    if (!searched)
    {
        searched = true;
        const char* const candidates[] = { "/data/opticraft/data", "/app0/data" };
        for (const char* candidate : candidates)
        {
            if (isDirectory(std::string(candidate) + "/assets"))
            {
                resolved = candidate;
                break;
            }
        }
    }
    return resolved.empty() ? nullptr : resolved.c_str();
}
}

#endif // PS4_PLATFORM
