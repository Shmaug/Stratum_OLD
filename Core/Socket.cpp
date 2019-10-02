#include <Core/Socket.hpp>

#include <iostream>
#include <sstream>

#include <errno.h>
#include <cstdio>
#include <cstring>

#ifndef WINDOWS
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#else
#include <winsock2.h>
#include <stdlib.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

using namespace std;

Socket::Socket(int socket) : mSocket(socket), mType(SocketType::Connect), mBlockingState(true), mFamily(1), mSocketType(0), mPort(0) {}

Socket::Socket(SocketType type, string host, uint32_t port, uint32_t family, uint32_t socketType)
	: mType(type), mFamily(family), mSocketType(socketType), mHost(host), mPort(port), mBlockingState(true), mSocket(-1) {

	struct ::addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = mFamily;
	hints.ai_socktype = mSocketType;

	if (getaddrinfo(mHost.c_str(), to_string(port).c_str(), &hints, &mRes) == 0)
		mSocket = (int)socket(mRes->ai_family, mRes->ai_socktype, mRes->ai_protocol);
	else
		cerr << "Failed to get address info of " << mHost << ":" << to_string(port) << endl;
}

Socket::~Socket() {
	if (Valid())
#ifdef WINDOWS
		closesocket(mSocket);
#else
		close(mSocket);
#endif
}

bool Socket::Bind() {
	if (mType != SocketType::Listen || !Valid())
		return false;

	if (::bind(mSocket, mRes->ai_addr, (int)mRes->ai_addrlen) == -1)
		return false;

	return true;
}

bool Socket::Listen(int backlog) {
	if (mType != SocketType::Listen || !Valid())
		return false;

	if (::listen(mSocket, backlog) == -1)
		return false;

	return true;
}

bool Socket::Accept() {
	if (mType != SocketType::Listen || !Valid())
		return false;

	struct sockaddr_storage node_addr;
	socklen_t addr_size;
	addr_size = sizeof(node_addr);
	int tmpSock;
	if ((tmpSock = (int)::accept(mSocket, (struct sockaddr*) & node_addr, &addr_size)) == -1) {
#ifdef WINDOWS
		closesocket(mSocket);
#else
		close(mSocket);
#endif
		mSocket = -1;
		return false;
	}
#ifdef WINDOWS
	closesocket(mSocket);
#else
	close(mSocket);
#endif
	mSocket = tmpSock;
	return true;
}

bool Socket::Connect(int timeout) {
	if (mType != SocketType::Connect || !Valid())
		return false;

	bool resetBlocking = false;
	if (timeout > 0) {
		if (mBlockingState) {
			Blocking(false);
			resetBlocking = true;
		}
	}

	int currentTimeout = timeout;

	while (::connect(mSocket, mRes->ai_addr, (int)mRes->ai_addrlen) == -1) {
#ifdef WINDOWS
		if (WSAGetLastError() == WSAEISCONN)
			break;
#endif

		if (timeout == 0) 
			return false;
		else {
			if (currentTimeout <= 0)
				return false;
#ifndef WINDOWS
			sleep(1);
#else
			Sleep(1000);
#endif
			currentTimeout--;
		}
	}

	if (resetBlocking)
		Blocking(true);

	return true;
}

int Socket::SetSockopt(int level, int optname, void* val, socklen_t len) {
	return ::setsockopt(mSocket, level, optname, (const char*)val, len);
}

bool Socket::NoDelay(bool b) {
	if (!Valid()) return false;

	int yes = b ? 1 : 0;
	if (::setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)& yes, sizeof(int)) == -1)
		return false;
	return true;
}

bool Socket::ReuseAddress(bool b) {
	if (!Valid()) return false;

	int yes = b ? 1 : 0;

	if (::setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(int)) == -1)
		return false;
	
	return true;
}

bool Socket::Blocking(bool b) {
	if (!Valid()) return false;

	int res;

#ifndef WINDOWS
	int flags = fcntl(mSocket, F_GETFL, 0);
	if (b) {
		res = fcntl(mSocket, F_SETFL, flags & ~(O_NONBLOCK));
	} else {
		res = fcntl(mSocket, F_SETFL, flags | O_NONBLOCK);
	}
#else
	u_long val = b ? 0 : 1;
	res = ioctlsocket(mSocket, FIONBIO, &val);
#endif

	if (res < 0)
		return false;
	else
		mBlockingState = b;
	return true;
}

bool Socket::Send(const void* buf, size_t len, int flags) {
	if (!buf || !Valid())
		return false;

	int bytesToSend = (int)len;
	int sent;
	char* data = (char*)buf;
	while (bytesToSend > 0) {
		if ((sent = ::send(mSocket, (const char*)data, bytesToSend, flags)) <= 0)
			break;
		
		bytesToSend -= sent;
		data += sent;
	}

	return !bytesToSend;
}

int Socket::Receive(void* buf, size_t len, int flags) {
	if (!buf || !Valid()) return 0;
	return ::recv(mSocket, (char*)buf, (int)len, flags);
}