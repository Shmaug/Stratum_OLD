#pragma once

#ifdef __linux
#include <vulkan/vulkan.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <vulkan/vulkan_xcb.h>
namespace x11{
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <vulkan/vulkan_xlib_xrandr.h>
};
#endif

#include <Input/MouseKeyboardInput.hpp>
#include <Util/Util.hpp>

class Window;
class Device;

class Instance {
public:
	struct DisplayCreateInfo {
		uint32_t mDeviceIndex;
		VkRect2D mWindowPosition;
		std::string mXDisplay;
		bool mXDirectDisplay;
	};

	ENGINE_EXPORT ~Instance();

	// Initializes devices and windows according to the DisplayCreateInfos passed in
	ENGINE_EXPORT void CreateDevicesAndWindows(const std::vector<DisplayCreateInfo>& displays);

	inline uint32_t DeviceCount() const { return (uint32_t)mDevices.size(); }
	/// Get a specific device. The index here does NOT correspond to the physical device index.
	/// To get the physical device index, call Device.PhysicalDeviceIndex()
	inline Device* GetDevice(uint32_t index) const { return mDevices[index]; }

	inline uint32_t WindowCount() const { return (uint32_t)mWindows.size(); }
	inline Window* GetWindow(uint32_t i) const { return mWindows[i]; }

	inline float TotalTime() const { return mTotalTime; }
	inline float DeltaTime() const { return mDeltaTime; }

	inline uint32_t MaxFramesInFlight() const { return mMaxFramesInFlight; }

	inline operator VkInstance() const { return mInstance; }

private:
	friend class Stratum;
	ENGINE_EXPORT Instance();

	ENGINE_EXPORT bool PollEvents();
	ENGINE_EXPORT void AdvanceFrame();

	MouseKeyboardInput* mWindowInput;

	std::vector<Device*> mDevices;
	std::vector<Window*> mWindows;
	uint32_t mMaxFramesInFlight;
	uint64_t mFrameCount;

	VkInstance mInstance;

	std::chrono::high_resolution_clock mClock;
	std::chrono::high_resolution_clock::time_point mStartTime;
	std::chrono::high_resolution_clock::time_point mLastFrame;
	float mTotalTime;
	float mDeltaTime;

	#ifdef __linux
	struct XCBConnection {
		xcb_connection_t* mConnection;
		xcb_key_symbols_t* mKeySymbols;

		inline XCBConnection() : mConnection(nullptr), mKeySymbols(nullptr) {}
		inline XCBConnection(xcb_connection_t* conn, xcb_key_symbols_t* symbols) : mConnection(conn), mKeySymbols(symbols) {}

	};
	ENGINE_EXPORT void ProcessEvent(XCBConnection* connection, xcb_generic_event_t* event);
	ENGINE_EXPORT xcb_generic_event_t* PollEvent(XCBConnection* conn);

	bool mDestroyPending;
	std::unordered_map<std::string, x11::Display*> mXDisplays;
	std::unordered_map<std::string, XCBConnection> mXCBConnections;
	#else
	void HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	bool mDestroyPending;
	static std::vector<Instance*> sInstances;
	#endif
};