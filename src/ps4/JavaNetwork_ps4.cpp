// JavaNetwork_ps4.cpp — multiplayer TCP for the PS4 port.
//
// Connections use the BSD socket API OpenOrbis exposes (socket/connect/recv/
// send, as in the toolchain's networking sample); only hostname lookup goes
// through libSceNet (Ps4Network). The stream adapters are the Wii backend's.
#include "java/JavaNetwork.h"

#ifdef PS4_PLATFORM

#include <atomic>
#include <cstring>
#include <istream>
#include <ostream>
#include <streambuf>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ps4/network/Ps4Network.h"

namespace JavaNetwork
{
namespace
{

class Ps4Socket final : public Socket
{
public:
	~Ps4Socket() override { releaseSocket(); }

	bool connect(const std::string &host, int port) override
	{
		releaseSocket();
		closing.store(false, std::memory_order_release);
		receivedBytes.store(0, std::memory_order_release);
		sentBytes.store(0, std::memory_order_release);
		remoteAddress = host + ":" + std::to_string(port);
		if (port < 1 || port > 65535)
			return false;

		std::uint32_t address = 0;
		if (!Ps4Network::resolveIPv4(host, address))
			return false;

		const int socketFd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (socketFd < 0)
			return false;
		fd.store(socketFd, std::memory_order_release);

		// The protocol sends many small packets (movement, keep-alives);
		// Nagle would hold each one back waiting for the previous ACK.
		int noDelay = 1;
		::setsockopt(socketFd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));

		sockaddr_in target;
		std::memset(&target, 0, sizeof(target));
		target.sin_family = AF_INET;
		target.sin_port = htons(static_cast<std::uint16_t>(port));
		target.sin_addr.s_addr = address;

		if (::connect(socketFd, reinterpret_cast<sockaddr *>(&target), sizeof(target)) < 0)
		{
			close();
			return false;
		}
		return true;
	}

	int read(char *buffer, int length) override
	{
		const int socketFd = fd.load(std::memory_order_acquire);
		if (socketFd < 0 || buffer == nullptr || length <= 0 ||
		    closing.load(std::memory_order_acquire))
			return -1;
		const ssize_t count = ::recv(socketFd, buffer, static_cast<std::size_t>(length), 0);
		if (count > 0)
			receivedBytes.fetch_add(static_cast<std::size_t>(count), std::memory_order_relaxed);
		return static_cast<int>(count);
	}

	bool write(const char *buffer, int length) override
	{
		const int socketFd = fd.load(std::memory_order_acquire);
		if (socketFd < 0 || buffer == nullptr || closing.load(std::memory_order_acquire))
			return false;
		int offset = 0;
		while (offset < length)
		{
			if (closing.load(std::memory_order_acquire))
				return false;
			const ssize_t count = ::send(socketFd, buffer + offset, static_cast<std::size_t>(length - offset), 0);
			if (count <= 0)
				return false;
			sentBytes.fetch_add(static_cast<std::size_t>(count), std::memory_order_relaxed);
			offset += static_cast<int>(count);
		}
		return true;
	}

	bool flush() override
	{
		return fd.load(std::memory_order_acquire) >= 0 &&
		       !closing.load(std::memory_order_acquire);
	}

	// Wakes a reader blocked in recv() on the network thread.
	void interruptRead() override
	{
		const int socketFd = fd.load(std::memory_order_acquire);
		if (socketFd >= 0)
			::shutdown(socketFd, SHUT_RD);
	}

	void close() override
	{
		closing.store(true, std::memory_order_release);
		const int socketFd = fd.load(std::memory_order_acquire);
		if (socketFd >= 0)
			::shutdown(socketFd, SHUT_RDWR);
	}

	std::string getRemoteSocketAddress() const override { return remoteAddress; }
	std::size_t getReceivedByteCount() const override { return receivedBytes.load(std::memory_order_relaxed); }
	std::size_t getSentByteCount() const override { return sentBytes.load(std::memory_order_relaxed); }

private:
	void releaseSocket()
	{
		closing.store(true, std::memory_order_release);
		const int socketFd = fd.exchange(-1, std::memory_order_acq_rel);
		if (socketFd >= 0)
		{
			::shutdown(socketFd, SHUT_RDWR);
			::close(socketFd);
		}
	}

	std::atomic<int> fd{-1};
	std::atomic_bool closing{true};
	std::atomic<std::size_t> receivedBytes{0};
	std::atomic<std::size_t> sentBytes{0};
	std::string remoteAddress;
};

class SocketInputBuffer final : public std::streambuf
{
public:
	explicit SocketInputBuffer(Socket &value) : socket(value) { setg(buffer, buffer, buffer); }

protected:
	int_type underflow() override
	{
		if (gptr() < egptr())
			return traits_type::to_int_type(*gptr());
		int count = socket.read(buffer, sizeof(buffer));
		if (count <= 0)
			return traits_type::eof();
		setg(buffer, buffer, buffer + count);
		return traits_type::to_int_type(*gptr());
	}

private:
	Socket &socket;
	char buffer[512];
};

class SocketOutputBuffer final : public std::streambuf
{
public:
	explicit SocketOutputBuffer(Socket &value) : socket(value)
	{
		setp(buffer, buffer + sizeof(buffer));
	}
	~SocketOutputBuffer() override { sync(); }

protected:
	std::streamsize xsputn(const char *data, std::streamsize length) override
	{
		std::streamsize written = 0;
		while (written < length)
		{
			std::streamsize space = epptr() - pptr();
			if (space == 0)
			{
				if (!flushBuffer())
					return written;
				space = epptr() - pptr();
			}

			const std::streamsize remaining = length - written;
			const std::streamsize count = remaining < space ? remaining : space;
			std::memcpy(pptr(), data + written, static_cast<std::size_t>(count));
			pbump(static_cast<int>(count));
			written += count;
		}
		return written;
	}

	int_type overflow(int_type value) override
	{
		if (traits_type::eq_int_type(value, traits_type::eof()))
			return traits_type::not_eof(value);
		if (!flushBuffer())
			return traits_type::eof();
		*pptr() = traits_type::to_char_type(value);
		pbump(1);
		return value;
	}

	int sync() override
	{
		return flushBuffer() && socket.flush() ? 0 : -1;
	}

private:
	bool flushBuffer()
	{
		const std::streamsize count = pptr() - pbase();
		if (count > 0 && !socket.write(pbase(), static_cast<int>(count)))
			return false;
		pbump(-static_cast<int>(count));
		return true;
	}

	Socket &socket;
	char buffer[5120];
};

class SocketInputStream final : public std::istream
{
public:
	explicit SocketInputStream(Socket &socket) : std::istream(nullptr), buffer(socket)
	{
		rdbuf(&buffer);
	}
private:
	SocketInputBuffer buffer;
};

class SocketOutputStream final : public std::ostream
{
public:
	explicit SocketOutputStream(Socket &socket) : std::ostream(nullptr), buffer(socket)
	{
		rdbuf(&buffer);
	}
private:
	SocketOutputBuffer buffer;
};

}

std::unique_ptr<Socket> createSocket() { return std::make_unique<Ps4Socket>(); }
std::unique_ptr<std::istream> createInputStream(Socket &socket) { return std::make_unique<SocketInputStream>(socket); }
std::unique_ptr<std::ostream> createOutputStream(Socket &socket) { return std::make_unique<SocketOutputStream>(socket); }

// Multiplayer only needs the TCP socket API. HTTP(S) resource and auth
// requests stay disabled, as on the Wii: there is no TLS backend, and the
// port runs with PLATFORM_LOCAL_RESOURCES_ONLY.
bool readUrl(const std::string &, std::vector<unsigned char> &) { return false; }
int getResponseCode(const std::string &) { return -1; }
bool postUrl(const std::string &, const std::string &, const std::string &,
             std::vector<unsigned char> &) { return false; }

}

#endif // PS4_PLATFORM
