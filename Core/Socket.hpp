#pragma once

#include <Util/Util.hpp>

#ifndef WINDOWS
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#else
#include <WS2tcpip.h>
#endif

enum SocketType {
	Listen, Connect
};

class Socket {
public:
	ENGINE_EXPORT Socket(int socket);
	ENGINE_EXPORT Socket(SocketType type, std::string host, uint32_t port, uint32_t family = AF_INET, uint32_t sockType = SOCK_STREAM);
	ENGINE_EXPORT ~Socket();

	ENGINE_EXPORT int SetSockopt(int level, int optname, void* val, socklen_t len);
	
	ENGINE_EXPORT bool NoDelay(bool b);
	ENGINE_EXPORT bool ReuseAddress(bool b);
	ENGINE_EXPORT bool Blocking(bool b);

	ENGINE_EXPORT bool Bind(); // Binds Listen socket
	ENGINE_EXPORT bool Accept(); // Accept Listen socket
	ENGINE_EXPORT bool Listen(int backlog = 5); // Listen Listen socket
	ENGINE_EXPORT bool Connect(int timeout = 0); // Connect Connect socket
	ENGINE_EXPORT bool Send(const void* buf, std::size_t len, int flags = 0);
	ENGINE_EXPORT int Receive(void* buf, std::size_t len, int flags = 0);

	inline bool Valid() const { return mSocket >= 0; }
	inline void SocketFD(int socket) { mSocket = socket; }
	inline int SocketFD() const { return mSocket; }

protected:
	int mSocket;
	SocketType mType;
	uint32_t mFamily;
	uint32_t mSocketType;
	std::string mHost;
	uint32_t mPort;
	struct addrinfo* mRes;

	bool mBlockingState;
};
