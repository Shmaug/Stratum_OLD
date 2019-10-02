#pragma once

#include <thread>

#include <Util/Util.hpp>
#include <Core/Socket.hpp>

class TelnetClient {
public:
	PLUGIN_EXPORT TelnetClient(const std::string& host, uint32_t port);
	PLUGIN_EXPORT ~TelnetClient();

	inline bool Connected() const { return mConnected; }

	inline void Clear() { mText.clear(); }
	inline std::string Text() const { return mText; }

	PLUGIN_EXPORT bool Send(const std::string& text);
	PLUGIN_EXPORT void WaitForText(const std::string& text);

private:
	std::thread mListenThread;
	bool mConnected;
	std::string mText;
	Socket* mSocket;

	PLUGIN_EXPORT bool Negotiate(uint8_t* buffer);
};