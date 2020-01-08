#pragma once

#ifdef __linux
#include <xcb/xcb.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>
#endif

#include <Util/Util.hpp>

class Window;
class Device;

class Instance {
public:
	struct DisplayCreateInfo {
		int mDevice;
		VkRect2D mWindowPosition;
		std::string mXDisplay;
		int mXScreen;
	};

	ENGINE_EXPORT ~Instance();

	// Initializes devices and windows according to the DisplayCreateInfos passed in
	ENGINE_EXPORT void CreateDevicesAndWindows(const std::vector<DisplayCreateInfo>& displays);

	// Get a physical device
	// Here, index represents the index of all SUITABLE devices
	ENGINE_EXPORT VkPhysicalDevice GetPhysicalDevice(uint32_t index, const std::vector<const char*>& extensions) const;

	inline uint32_t DeviceCount() const { return (uint32_t)mDevices.size(); }
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
	std::unordered_map<std::string, xcb_connection_t*> mXCBConnections;
	#endif
};