// Diagnostics_PS4.cpp — memory diagnostics from the kernel's flexible memory
// counter, which is where the game heap is allocated from.
#include "platform/Diagnostics.h"
#include "platform/Log.h"

#include <stdint.h>
#include <orbis/libkernel.h>

#include <cstdio>

namespace
{
char s_badAllocLine[128] = "";
}

const char* platformOomDiagnosticLine(int index)
{
    return index == 0 ? s_badAllocLine : "";
}

long platformHeapFreeKb()
{
    size_t available = 0;
    if (sceKernelAvailableFlexibleMemorySize(&available) < 0)
        return -1;
    return static_cast<long>(available / 1024u);
}

void platformMemoryCheckpoint(const char* tag)
{
    MC_LOG_DEBUG("mem", "[PS4] %s: %ld KB flexible memory free\n", tag != nullptr ? tag : "", platformHeapFreeKb());
    (void)tag;
}

void platformHardwareCheckpoint(const char* tag)
{
    (void)tag;
}

void platformCaptureBadAlloc()
{
    std::snprintf(s_badAllocLine, sizeof(s_badAllocLine), "Flexible memory free at failure: %ld KB",
                  platformHeapFreeKb());
}
