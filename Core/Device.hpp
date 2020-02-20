#pragma once

#include <list>
#include <utility>

#include <Core/DescriptorSet.hpp>
#include <Core/CommandBuffer.hpp>
#include <Core/Instance.hpp>
#include <Util/Util.hpp>

class CommandBuffer;
class Fence;
class Window;

class Device {
public:
	struct FrameContext {
		std::vector<std::shared_ptr<Semaphore>> mSemaphores; // semaphores that signal when this frame is 'done'
		std::vector<std::shared_ptr<Fence>> mFences; // fences that signal when this frame is 'done'
		
		std::list<std::pair<Buffer*, uint32_t>> mTempBuffers;
		std::unordered_map<VkDescriptorSetLayout, std::list<std::pair<DescriptorSet*, uint32_t>>> mTempDescriptorSets;

		std::vector<Buffer*> mTempBuffersInUse;
		std::vector<DescriptorSet*> mTempDescriptorSetsInUse;

		Device* mDevice;

		inline FrameContext() : mFences({}), mSemaphores({}), mTempBuffers({}), mTempDescriptorSets({}), mTempBuffersInUse({}), mTempDescriptorSetsInUse({}) {};
		ENGINE_EXPORT ~FrameContext();
		ENGINE_EXPORT void Reset();
	};

	ENGINE_EXPORT static bool FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface, uint32_t& graphicsFamily, uint32_t& presentFamily);

	ENGINE_EXPORT ~Device();

	inline VkPhysicalDevice PhysicalDevice() const { return mPhysicalDevice; }
	inline uint32_t PhysicalDeviceIndex() const { return mPhysicalDeviceIndex; }
	inline VkQueue GraphicsQueue() const { return mGraphicsQueue; };
	inline VkQueue PresentQueue() const { return mPresentQueue; };
	inline uint32_t GraphicsQueueFamily() const { return mGraphicsQueueFamily; };
	inline uint32_t PresentQueueFamily() const { return mPresentQueueFamily; };

	ENGINE_EXPORT void SetObjectName(void* object, const std::string& name, VkObjectType type) const;

	ENGINE_EXPORT uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	
	ENGINE_EXPORT Buffer* GetTempBuffer(const std::string& name, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	ENGINE_EXPORT DescriptorSet* GetTempDescriptorSet(const std::string& name, VkDescriptorSetLayout layout);

	ENGINE_EXPORT std::shared_ptr<CommandBuffer> GetCommandBuffer(const std::string& name = "Command Buffer");
	ENGINE_EXPORT std::shared_ptr<Fence> Execute(std::shared_ptr<CommandBuffer> commandBuffer, bool frameContext = true);
	ENGINE_EXPORT void FlushFrames();

	ENGINE_EXPORT VkSampleCountFlagBits GetMaxUsableSampleCount();

	inline uint32_t MaxFramesInFlight() const { return mInstance->MaxFramesInFlight(); }
	inline uint32_t FrameContextIndex() const { return mFrameContextIndex; }
	inline FrameContext* CurrentFrameContext() { return &mFrameContexts[mFrameContextIndex]; }

	inline const VkPhysicalDeviceLimits& Limits() const { return mLimits; }
	inline ::Instance* Instance() const { return mInstance; }
	inline VkPipelineCache PipelineCache() const { return mPipelineCache; }

	inline operator VkDevice() const { return mDevice; }

private:
	friend class DescriptorSet;
	friend class CommandBuffer;
	friend class ::Instance;
	ENGINE_EXPORT Device(::Instance* instance, VkPhysicalDevice physicalDevice, uint32_t physicalDeviceIndex, uint32_t graphicsQueue, uint32_t presentQueue, const std::set<std::string>& deviceExtensions, std::vector<const char*> validationLayers);

	::Instance* mInstance;
	uint32_t mFrameContextIndex; // assigned by mInstance
	FrameContext* mFrameContexts;

	VkPhysicalDeviceLimits mLimits;

	uint32_t mMaxMSAASamples;
	uint32_t mPhysicalDeviceIndex;
	VkPhysicalDevice mPhysicalDevice;
	VkDevice mDevice;
	VkPipelineCache mPipelineCache;

	uint32_t mGraphicsQueueFamily;
	uint32_t mPresentQueueFamily;

	VkQueue mGraphicsQueue;
	VkQueue mPresentQueue;

	VkDescriptorPool mDescriptorPool;

	std::mutex mTmpDescriptorSetMutex;
	std::mutex mTmpBufferMutex;
	std::mutex mDescriptorPoolMutex;
	std::mutex mCommandPoolMutex;
	std::unordered_map<std::thread::id, VkCommandPool> mCommandPools;
	std::unordered_map<VkCommandPool, std::queue<std::shared_ptr<CommandBuffer>>> mCommandBuffers;

	#ifdef ENABLE_DEBUG_LAYERS
	PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;
	PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginDebugUtilsLabelEXT;
	PFN_vkCmdEndDebugUtilsLabelEXT CmdEndDebugUtilsLabelEXT;
	#endif
};