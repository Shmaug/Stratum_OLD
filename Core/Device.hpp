#pragma once

#include <queue>
#include <mutex>
#include <unordered_map>
#include <thread>

#include <Core/CommandBuffer.hpp>
#include <Util/Util.hpp>

class CommandBuffer;
class DescriptorPool;
class DeviceManager;
class Fence;
class Window;

class Device {
public:
	ENGINE_EXPORT ~Device();

	inline uint32_t MaxFramesInFlight() const { return mMaxFramesInFlight; }
	inline VkPhysicalDevice PhysicalDevice() const { return mPhysicalDevice; }
	inline uint32_t PhysicalDeviceIndex() const { return mPhysicalDeviceIndex; }
	inline VkQueue GraphicsQueue() const { return mGraphicsQueue; };
	inline VkQueue PresentQueue() const { return mPresentQueue; };

	ENGINE_EXPORT void SetObjectName(void* object, std::string name) const;

	ENGINE_EXPORT uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	
	ENGINE_EXPORT std::shared_ptr<CommandBuffer> GetCommandBuffer(const std::string& name = "Command Buffer");
	ENGINE_EXPORT std::shared_ptr<Fence> Execute(std::shared_ptr<CommandBuffer> commandBuffer);
	ENGINE_EXPORT void FlushCommandBuffers();

	inline const VkPhysicalDeviceLimits& Limits() const { return mLimits; }
	inline VkInstance Instance() const { return mInstance; }
	inline ::DescriptorPool* DescriptorPool() const { return mDescriptorPool; }
	inline VkPipelineCache PipelineCache() const { return mPipelineCache; }

	inline operator VkDevice() const { return mDevice; }

private:
	friend class CommandBuffer;
	friend class DeviceManager;
	ENGINE_EXPORT Device(VkInstance instance, std::vector<const char*> extensions, std::vector<const char*> validationLayers, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, uint32_t physicalDeviceIndex);

	VkInstance mInstance;
	VkPhysicalDeviceLimits mLimits;

	uint32_t mMaxFramesInFlight;
	uint32_t mMaxMSAASamples;
	uint32_t mPhysicalDeviceIndex;
	VkPhysicalDevice mPhysicalDevice;
	VkDevice mDevice;
	VkPipelineCache mPipelineCache;

	uint32_t mGraphicsQueueFamily;
	uint32_t mPresentQueueFamily;

	VkQueue mGraphicsQueue;
	VkQueue mPresentQueue;

	::DescriptorPool* mDescriptorPool;

	std::mutex mCommandPoolMutex;
	std::unordered_map<std::thread::id, VkCommandPool> mCommandPools;
	std::unordered_map<VkCommandPool, std::queue<std::shared_ptr<CommandBuffer>>> mCommandBuffers;

	#ifdef _DEBUG
	PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;
	PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginDebugUtilsLabelEXT;
	PFN_vkCmdEndDebugUtilsLabelEXT CmdEndDebugUtilsLabelEXT;
	#endif
};