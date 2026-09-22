// Ps4Paths.h — where the PS4 port reads game data and writes saves.
//
//   /app0/            the installed package, read-only. Game data is packaged
//                     under /app0/data/ by the ps4-pkg target.
//   /data/opticraft/  homebrew-writable storage that survives reinstalls:
//                     options, worlds, logs. A data/ tree copied here over FTP
//                     overrides the packaged one, so assets can be updated
//                     without rebuilding the package.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4Paths
{
// Creates the writable root. Call once at boot, before logging to a file.
bool ensureWritableRoot();

const char* writableRoot();   // "/data/opticraft"
// Directory holding assets/ and resources/, or nullptr if none was found.
const char* gameDataDir();
// The directory gameDataDir() lives in ("/data/opticraft" or "/app0"): where
// an assets.pak is looked for and what GameResources::getExeDir() reports.
// Falls back to "/app0" when no data was found, so paths stay well-formed.
const char* installDir();
}

#endif
