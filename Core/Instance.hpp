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
class PluginManager;

class Instance {
public:
	ENGINE_EXPORT ~Instance();

	inline ::Device* Device() const { return mDevice; }
	inline ::Window* Window() const { return mWindow; }

	inline uint64_t FrameCount() const { return mFrameCount; }

	inline uint32_t MaxFramesInFlight() const { return mMaxFramesInFlight; }

	inline const std::vector<std::string>& CommandLineArguments() const { return mCmdArguments; }

	inline operator VkInstance() const { return mInstance; }

	inline void RequestInstanceExtension(const std::string& name) { mInstanceExtensions.emplace(name); }
	inline void RequestDeviceExtension(const std::string& name) { mDeviceExtensions.emplace(name); }

private:
	friend class Stratum;
	ENGINE_EXPORT Instance(int argc, char** argv, PluginManager* pluginManager);

	ENGINE_EXPORT bool PollEvents();
	ENGINE_EXPORT void AdvanceFrame();

	MouseKeyboardInput* mWindowInput;

	std::set<std::string> mInstanceExtensions;
	std::set<std::string> mDeviceExtensions;

	::Device* mDevice;
	::Window* mWindow;
	uint32_t mMaxFramesInFlight;
	uint64_t mFrameCount;

	VkInstance mInstance;

	#ifdef ENABLE_DEBUG_LAYERS
	VkDebugUtilsMessengerEXT mDebugMessenger;
	#endif

	std::vector<std::string> mCmdArguments;

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