#include "TelnetClient.hpp"

using namespace std;

#define SE                  240		// End of subnegotiation parameters.
#define NOP                 241		// No operation.
#define DM					242		// The data stream portion of a Synch.
									// This should always be accompanied
									// by a TCP Urgent notification.
#define BRK					243     // NVT character BRK.
#define IP					244     // Interrupt Process
#define AO			        245     // Abort Output
#define AYT				    246		// Are You There
#define EC					247		// Erase Character
#define EL					248		// Erase Line
#define GA		            249		// Go Ahead
#define SB                  250		// Indicates that what follows is
									// subnegotiation of the indicated
									// option.
#define WILL				251		// Indicates the desire to begin
									// performing, or confirmation that
									// you are now performing, the
									// indicated option.
#define WONT				252		// Indicates the refusal to perform,
									// or continue performing, the
									// indicated option.
#define DO				    253		// Indicates the request that the
									// other party perform, or
									// confirmation that you are expecting
									// the other party to perform, the
									// indicated option.
#define DONT				254		// Indicates the demand that the
									// other party stop performing,
									// or confirmation that you are no
									// longer expecting the other party
									// to perform, the indicated option.
#define IAC                 255		// Data Byte 255

#define ECHO				1
#define CMD_WINDOW_SIZE 	31

TelnetClient::TelnetClient(const string& host, uint32_t port) : mSocket(nullptr) {
	mSocket = new Socket(Connect, host, port);
	mSocket->Connect();
	mConnected = mSocket->Valid();
	if (!mConnected) printf("Failed to connect to %s:%u", host.c_str(), port);

	mListenThread = thread([&]() {
		printf("Listening...\n");
		uint8_t buffer[1024];
		while (mConnected) {
			int len = mSocket->Receive((void*)buffer, 1024);
			if (len <= 0) { mConnected = false; break; }

			if (buffer[0] == IAC) {
				while (len < 3) {
					int len2 = mSocket->Receive((void*)(buffer + len), 1024 - (size_t)len);
					if (len2 <= 0) { mConnected = false; break; }
					len += len2;
					break;
				}
				if (!mConnected) break;
				Negotiate(buffer);
			} else
				mText.append((char*)buffer, len);
		}
		printf("Disconnected.\n");
	});
}
TelnetClient::~TelnetClient() {
	mConnected = false;
	mListenThread.join();
	safe_delete(mSocket);
}

bool TelnetClient::Send(const string& text) {
	if (!mSocket->Send((void*)text.data(), text.length())) {
		mConnected = false;
		return false;
	}
	return true;
}

void TelnetClient::WaitForText(const string& text) {
	string tmp = mText;
	while (mText.length() < text.length() || tmp.rfind(text) == string::npos) {
		this_thread::sleep_for(10ns);
		tmp = mText;
	}
}

bool TelnetClient::Negotiate(uint8_t* buffer) {
	if (buffer[1] == DO && buffer[2] == CMD_WINDOW_SIZE) {
		const uint8_t cmdWillWindowSize[3] { IAC, WILL, CMD_WINDOW_SIZE };
		const uint8_t cmdWindowSize[9] { IAC, SB, CMD_WINDOW_SIZE, 0, 0xFF, 0, 0xFF, IAC, SE };

		if (!mSocket->Send(cmdWillWindowSize, 3)) return false;
		if (!mSocket->Send(cmdWindowSize, 9)) return false;
		return true;
	}

	for (uint32_t i = 0; i < 3; i++) {
		if (buffer[i] == DO)
			buffer[i] = WONT;
		else if (buffer[i] == WILL)
			buffer[i] = DO;
	}

	return mSocket->Send(buffer, 3);
}