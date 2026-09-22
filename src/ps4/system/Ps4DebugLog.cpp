#ifdef PS4_PLATFORM

#include "ps4/system/Ps4DebugLog.h"

#include <orbis/libkernel.h>

#include <cstdarg>
#include <cstdio>

namespace
{
FILE* s_file = nullptr;
}

namespace Ps4DebugLog
{
bool openFile(const char* path)
{
    if (s_file != nullptr)
        return true;
    s_file = std::fopen(path, "w");
    return s_file != nullptr;
}

void closeFile()
{
    if (s_file != nullptr)
    {
        std::fclose(s_file);
        s_file = nullptr;
    }
}

void printf(const char* format, ...)
{
    char line[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    // Channel 0 is the TTY the klog readers listen on. Passed through "%s"
    // so a '%' in the formatted text is never reinterpreted.
    sceKernelDebugOutText(0, "%s", line);
    if (s_file != nullptr)
    {
        std::fputs(line, s_file);
        std::fflush(s_file);
    }
}
}

#endif // PS4_PLATFORM
