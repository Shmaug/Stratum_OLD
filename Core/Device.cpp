#include <cstring>

#include <Core/Device.hpp>
#include <Core/DescriptorPool.hpp>
#include <Core/CommandBuffer.hpp>
#include <Core/Window.hpp>
#include <Util/Util.hpp>

using namespace std;

Device::Device(VkInstance instance, vector<const char*> deviceExtensions, vector<const char*> validationLayers, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, uint32_t physicalDeviceIndex)
	: mInstance(instance), mGraphicsQueueFamily(0), mPresentQueueFamily(0) {

	#ifdef ENABLE_DEBUG_LAYERS
	SetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
	CmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
	CmdEndDebugUtilsLabelEXT   = (PFN_vkCmdEndDebugUtilsLabelEXT)  vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
	#endif

	mPhysicalDevice = physicalDevice;
	mMaxMSAASamples = GetMaxUsableSampleCount(mPhysicalDevice);
	mPhysicalDeviceIndex = physicalDeviceIndex;

	#pragma region get queue info
	if (!FindQueueFamilies(mPhysicalDevice, surface, mGraphicsQueueFamily, mPresentQueueFamily)){
		cerr << "Failed to find queue families!" << endl;
		throw runtime_error("Failed to find queue families!");
	}
	set<uint32_t> uniqueQueueFamilies{ mGraphicsQueueFamily, mPresentQueueFamily };

	vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}
	#pragma endregion

	#pragma region create logical device and queues
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();
	ThrowIfFailed(vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice), "vkCreateDevice failed");

	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
	string name = "Device " + to_string(properties.deviceID) + ": " + properties.deviceName;
	SetObjectName(mDevice, name);
	mLimits = properties.limits;

	vkGetDeviceQueue(mDevice, mGraphicsQueueFamily, 0, &mGraphicsQueue);
	vkGetDeviceQueue(mDevice, mPresentQueueFamily, 0, &mPresentQueue);
	SetObjectName(mGraphicsQueue, name + " Graphics Queue");
	SetObjectName(mPresentQueue, name + " Present Queue");
	#pragma endregion

	VkPipelineCacheCreateInfo cache = {};
	cache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	vkCreatePipelineCache(mDevice, &cache, nullptr, &mPipelineCache);

	mDescriptorPool = new ::DescriptorPool(name + " Descriptor Pool", this);
}
Device::~Device() {
	FlushCommandBuffers();
	safe_delete(mDescriptorPool);
	vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
	for (auto& p : mCommandBuffers)
		vkDestroyCommandPool(mDevice, p.first, nullptr);
	vkDestroyDevice(mDevice, nullptr);
}

void Device::FlushCommandBuffers() {
	lock_guard lock(mCommandPoolMutex);
	for (auto& p : mCommandBuffers) {
		while (p.second.size()) {
			p.second.front()->mCompletionFence->Wait();
			p.second.pop();
		}
	}
}

void Device::SetObjectName(void* object, string name) const {
	#ifdef ENABLE_DEBUG_LAYERS
	VkDebugUtilsObjectNameInfoEXT info = {};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	info.objectHandle = (uint64_t)object;
	info.objectType = VK_OBJECT_TYPE_UNKNOWN;
	info.pObjectName = name.c_str();
	SetDebugUtilsObjectNameEXT(mDevice, &info);
	#endif
}

uint32_t Device::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			return i;

	throw runtime_error("failed to find suitable memory type!");
}

shared_ptr<CommandBuffer> Device::GetCommandBuffer(const std::string& name) {
	lock_guard lock(mCommandPoolMutex);
	VkCommandPool& commandPool = mCommandPools[this_thread::get_id()];

	if (!commandPool) {
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = mGraphicsQueueFamily;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		ThrowIfFailed(vkCreateCommandPool(mDevice, &poolInfo, nullptr, &commandPool), "vkCreateCommandPool failed");
		SetObjectName(commandPool, name + " Graphics Command Pool");
	}

	auto& commandBuffers = mCommandBuffers[commandPool];

	shared_ptr<CommandBuffer> commandBuffer;
	// see if the command buffer at the front of the queue is done
	if (commandBuffers.size() > 0) {
		commandBuffer = commandBuffers.front();
		if (commandBuffer->mCompletionFence->Signaled()) {
			// reset and reuse the command buffer at the front of the queue
			commandBuffers.pop();
			commandBuffer->Reset(name);
		} else
			commandBuffer = shared_ptr<CommandBuffer>(new CommandBuffer(this, commandPool, name));
	} else
		commandBuffer = shared_ptr<CommandBuffer>(new CommandBuffer(this, commandPool, name));

	// begin recording commands
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr; // Optional
	ThrowIfFailed(vkBeginCommandBuffer(commandBuffer->mCommandBuffer, &beginInfo), "vkBeginCommandBuffer failed");

	return commandBuffer;
}

shared_ptr<Fence> Device::Execute(shared_ptr<CommandBuffer> commandBuffer) {
	lock_guard lock(mCommandPoolMutex);
	ThrowIfFailed(vkEndCommandBuffer(commandBuffer->mCommandBuffer), "vkEndCommandBuffer failed");

	VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pWaitDstStageMask = &waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer->mCommandBuffer;
	vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, *commandBuffer->mCompletionFence);
	
	// store the command buffer in the queue
	mCommandBuffers[commandBuffer->mCommandPool].push(commandBuffer);
	return commandBuffer->mCompletionFence;
}