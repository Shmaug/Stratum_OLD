#pragma once

#include <Content/AssetDatabase.hpp>
#include <Core/Device.hpp>
#include <Core/Window.hpp>
#include <Core/Input.hpp>

using namespace glm;

class VkCAVE;

class DeviceManager {
public:
	struct DisplayCreateInfo {
		VkRect2D mWindowPosition;
		int mMonitor;
		int mDevice;

		vec3 mCameraPos;
		vec3 mCameraRot;
		float mCameraFov;
		float mCameraNear;
		float mCameraFar;
	};

	ENGINE_EXPORT ~DeviceManager();

	// Creates the VkInstance
	ENGINE_EXPORT void CreateInstance();
	// Initializes devices and windows according to the DisplayCreateInfos passed in
	ENGINE_EXPORT void Initialize(const std::vector<DisplayCreateInfo>& displays);

	// Get a physical device
	// Here, index represents the index of all SUITABLE devices
	ENGINE_EXPORT VkPhysicalDevice GetPhysicalDevice(uint32_t index, const std::vector<const char*>& extensions) const;

	// Asks GLFW if any windows should be closed, and polls GLFW events
	ENGINE_EXPORT bool PollEvents() const;

	inline VkInstance Instance() const { return mInstance; }

	inline ::AssetDatabase* AssetDatabase() const { return mAssetDatabase; }

	inline uint32_t DeviceCount() const { return (uint32_t)mDevices.size(); }
	inline Device* GetDevice(uint32_t index) const { return mDevices[index]; }

	inline uint32_t WindowCount() const { return (uint32_t)mWindows.size(); }
	inline Window* GetWindow(uint32_t i) const { return mWindows[i]; }

private:
	friend class VkCAVE;
	ENGINE_EXPORT DeviceManager();
	std::vector<Device*> mDevices;
	std::vector<Window*> mWindows;

	::AssetDatabase* mAssetDatabase;

	uint32_t mMaxFramesInFlight;

	VkInstance mInstance;

	bool mGLFWInitialized;
};