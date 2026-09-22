// Host shim for src/ps4/tools/Ps4GlesPipelineTests.cpp: Ps4DebugLog to stderr.
#include "ps4/system/Ps4DebugLog.h"

#include <cstdarg>
#include <cstdio>

namespace Ps4DebugLog
{
bool openFile(const char*) { return true; }
void closeFile() {}
void printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
}
}
