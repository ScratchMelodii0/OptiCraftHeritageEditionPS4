// Ps4Network.h — libSceNet setup and hostname resolution for the PS4 port.
//
// TCP itself goes through the ordinary BSD socket calls OpenOrbis exposes (see
// JavaNetwork_ps4.cpp). What needs libSceNet is DNS: there is no getaddrinfo,
// so names are resolved with the system resolver, which requires sceNetInit
// and a memory pool created once per process.
#pragma once
#ifdef PS4_PLATFORM

#include <cstdint>
#include <string>

namespace Ps4Network
{
// Idempotent. False if the network library cannot be brought up.
bool initialize();

// IPv4 address of `host` in network byte order. Dotted quads are parsed
// directly; anything else goes through the system resolver.
bool resolveIPv4(const std::string& host, std::uint32_t& addressNetworkOrder);
}

#endif
