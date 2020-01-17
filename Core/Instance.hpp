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
	};

	ENGINE_EXPORT ~Instance();

	inline ::Device* Device() const { return mDevice; }
	inline ::Window* Window() const { return mWindow; }

	inline float TotalTime() const { return mTotalTime; }
	inline float DeltaTime() const { return mDeltaTime; }

	inline uint32_t MaxFramesInFlight() const { return mMaxFramesInFlight; }

	inline operator VkInstance() const { return mInstance; }

private:
	friend class Stratum;
	ENGINE_EXPORT Instance(const DisplayCreateInfo& display);

	ENGINE_EXPORT bool PollEvents();
	ENGINE_EXPORT void AdvanceFrame();

	MouseKeyboardInput* mWindowInput;

	::Device* mDevice;
	::Window* mWindow;
	uint32_t mMaxFramesInFlight;
	uint64_t mFrameCount;

	VkInstance mInstance;

	std::chrono::high_resolution_clock mClock;
	std::chrono::high_resolution_clock::time_point mStartTime;
	std::chrono::high_resolution_clock::time_point mLastFrame;
	float mTotalTime;
	float mDeltaTime;

	bool mDestroyPending;
	#ifdef __linux
	ENGINE_EXPORT void ProcessEvent(xcb_generic_event_t* event);
	ENGINE_EXPORT xcb_generic_event_t* PollEvent();

	x11::Display* mXDisplay;
	xcb_connection_t* mXCBConnection;
	xcb_key_symbols_t* mXCBKeySymbols;
	#else
	void HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	static std::vector<Instance*> sInstances;
	#endif
};