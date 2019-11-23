#include <Core/Buffer.hpp>
#include <Core/Device.hpp>
#include <Core/Instance.hpp>
#include <Core/CommandBuffer.hpp>
#include <Core/Window.hpp>
#include <Util/Util.hpp>

using namespace std;

void Device::FrameContext::Reset() {
	for (auto f : mFences)
		f->Wait();
	mFences.clear();

	for (uint32_t i = 0; i < mSemaphores.size(); i++)
		mSemaphores[i].clear();

	for (Buffer* b : mTempBuffersInUse)
		mTempBuffers.push_back(b);
	for (DescriptorSet* ds : mTempDescriptorSetsInUse)
		mTempDescriptorSets[ds->Layout()].push(ds);

	mTempBuffersInUse.clear();
	mTempDescriptorSetsInUse.clear();
}
Device::FrameContext::~FrameContext() {
	Reset();
	for (Buffer* b : mTempBuffers)
		safe_delete(b);
	for (auto kp : mTempDescriptorSets)
		while (kp.second.size()) {
			safe_delete(kp.second.front());
			kp.second.pop();
		}
}

bool Device::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface, uint32_t& graphicsFamily, uint32_t& presentFamily) {
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	bool g = false;
	bool p = false;

	uint32_t i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsFamily = i;
			g = true;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (queueFamily.queueCount > 0 && presentSupport) {
			presentFamily = i;
			p = true;
		}

		i++;
	}

	return g && p;
}

Device::Device(::Instance* instance, VkPhysicalDevice physicalDevice, uint32_t physicalDeviceIndex, uint32_t graphicsQueueFamily, uint32_t presentQueueFamily, vector<const char*> deviceExtensions, vector<const char*> validationLayers)
	: mInstance(instance), mWindowCount(0), mGraphicsQueueFamily(graphicsQueueFamily), mPresentQueueFamily(presentQueueFamily), mFrameContextIndex(0) {

	#ifdef ENABLE_DEBUG_LAYERS
	SetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(*instance, "vkSetDebugUtilsObjectNameEXT");
	CmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(*instance, "vkCmdBeginDebugUtilsLabelEXT");
	CmdEndDebugUtilsLabelEXT   = (PFN_vkCmdEndDebugUtilsLabelEXT)  vkGetInstanceProcAddr(*instance, "vkCmdEndDebugUtilsLabelEXT");
	#endif

	mPhysicalDevice = physicalDevice;
	mMaxMSAASamples = GetMaxUsableSampleCount();
	mPhysicalDeviceIndex = physicalDeviceIndex;

	#pragma region get queue info
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

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures = {};
	indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
	indexingFeatures.pNext = nullptr;
	indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
	indexingFeatures.runtimeDescriptorArray = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();
	createInfo.pNext = &indexingFeatures;
	ThrowIfFailed(vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice), "vkCreateDevice failed");

	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
	string name = "Device " + to_string(properties.deviceID) + ": " + properties.deviceName;
	SetObjectName(mDevice, name, VK_OBJECT_TYPE_DEVICE);
	mLimits = properties.limits;

	vkGetDeviceQueue(mDevice, mGraphicsQueueFamily, 0, &mGraphicsQueue);
	vkGetDeviceQueue(mDevice, mPresentQueueFamily, 0, &mPresentQueue);
	SetObjectName(mGraphicsQueue, name + " Graphics Queue", VK_OBJECT_TYPE_QUEUE);
	SetObjectName(mPresentQueue, name + " Present Queue", VK_OBJECT_TYPE_QUEUE);
	#pragma endregion

	#pragma region PipelineCache and DesriptorPool
	VkPipelineCacheCreateInfo cache = {};
	cache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	vkCreatePipelineCache(mDevice, &cache, nullptr, &mPipelineCache);
	
	VkDescriptorPoolSize type_count[5] {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,			mLimits.maxDescriptorSetUniformBuffers },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	mLimits.maxDescriptorSetSampledImages },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				mLimits.maxDescriptorSetSampledImages },
		{ VK_DESCRIPTOR_TYPE_SAMPLER,					mLimits.maxDescriptorSetSamplers },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			mLimits.maxDescriptorSetStorageBuffers },
	};

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.poolSizeCount = 5;
	poolInfo.pPoolSizes = type_count;
	poolInfo.maxSets = 10000;

	ThrowIfFailed(vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool), "vkCreateDescriptorPool failed");
	SetObjectName(mDescriptorPool, name, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
	#pragma endregion
}
Device::~Device() {
	FlushFrames();
	mFrameContexts.clear();
	vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
	vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
	for (auto& p : mCommandBuffers)
		vkDestroyCommandPool(mDevice, p.first, nullptr);
	vkDestroyDevice(mDevice, nullptr);
}

VkSampleCountFlagBits Device::GetMaxUsableSampleCount() {
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(mPhysicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts = std::min(physicalDeviceProperties.limits.framebufferColorSampleCounts, physicalDeviceProperties.limits.framebufferDepthSampleCounts);
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

	return VK_SAMPLE_COUNT_1_BIT;
}

void Device::FlushFrames() {
	lock_guard lock(mCommandPoolMutex);
	for (auto& p : mCommandBuffers) {
		while (p.second.size()) {
			p.second.front()->mSignalFence->Wait();
			p.second.pop();
		}
	}
	for (FrameContext& frame : mFrameContexts)
		frame.Reset();
}

void Device::SetObjectName(void* object, const string& name, VkObjectType type) const {
	#ifdef ENABLE_DEBUG_LAYERS
	VkDebugUtilsObjectNameInfoEXT info = {};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	info.objectHandle = (uint64_t)object;
	info.objectType = type;
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

	fprintf_color(BoldRed, stderr, "Failed to find suitable memory type!");
	throw;
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
		SetObjectName(commandPool, name + " Graphics Command Pool", VK_OBJECT_TYPE_COMMAND_POOL);
	}

	auto& commandBuffers = mCommandBuffers[commandPool];

	shared_ptr<CommandBuffer> commandBuffer;
	// see if the command buffer at the front of the queue is done
	if (commandBuffers.size() > 0) {
		commandBuffer = commandBuffers.front();
		if (commandBuffer->mSignalFence->Signaled()) {
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
	ThrowIfFailed(vkBeginCommandBuffer(commandBuffer->mCommandBuffer, &beginInfo), "vkBeginCommandBuffer failed");

	return commandBuffer;
}

shared_ptr<Fence> Device::Execute(shared_ptr<CommandBuffer> commandBuffer, bool frameContext) {
	lock_guard lock(mCommandPoolMutex);
	ThrowIfFailed(vkEndCommandBuffer(commandBuffer->mCommandBuffer), "vkEndCommandBuffer failed");

	vector<VkSemaphore> semaphores;
	vector<VkPipelineStageFlags> waitStages;
	if (frameContext) {		
		CurrentFrameContext()->mFences.push_back(commandBuffer->mSignalFence);
		semaphores.resize(commandBuffer->mSignalSemaphores.size());
		for (uint32_t i = 0; i < commandBuffer->mSignalSemaphores.size(); i++) {
			if (!commandBuffer->mSignalSemaphores[i]) commandBuffer->mSignalSemaphores[i] = make_shared<Semaphore>(this);
			semaphores.push_back(*commandBuffer->mSignalSemaphores[i]);
			CurrentFrameContext()->mSemaphores[i].push_back(commandBuffer->mSignalSemaphores[i]);
			waitStages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pWaitDstStageMask = waitStages.data();
	submitInfo.signalSemaphoreCount = (uint32_t)semaphores.size();
	submitInfo.pSignalSemaphores = semaphores.data();
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer->mCommandBuffer;
	vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, *commandBuffer->mSignalFence);

	// store the command buffer in the queue
	mCommandBuffers[commandBuffer->mCommandPool].push(commandBuffer);
	return commandBuffer->mSignalFence;
}

Buffer* Device::GetTempBuffer(const std::string& name, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
	FrameContext* frame = CurrentFrameContext();

	vector<Buffer*>::iterator closest = frame->mTempBuffers.end();
	for (auto it = frame->mTempBuffers.begin(); it != frame->mTempBuffers.end(); it++) {
		if ((*it)->Size() >= size) {
			if (closest == frame->mTempBuffers.end() || (*it)->Size() < (*closest)->Size())
				closest = it;
			if ((*closest)->Size() == size) break;
		}
	}

	Buffer* b;
	if (closest != frame->mTempBuffers.end()) {
		b = *closest;
		frame->mTempBuffers.erase(closest);
	} else {
		b = new Buffer(name, this, size, usage, properties);
		if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) b->Map();
	}
	frame->mTempBuffersInUse.push_back(b);
	return b;
}
DescriptorSet* Device::GetTempDescriptorSet(const std::string& name, VkDescriptorSetLayout layout) {
	FrameContext* frame = CurrentFrameContext();

	queue<DescriptorSet*>& sets = frame->mTempDescriptorSets[layout];

	DescriptorSet* ds;
	if (sets.size()) {
		ds = sets.front();
		sets.pop();
	} else
		ds = new DescriptorSet(name, this, layout);

	frame->mTempDescriptorSetsInUse.push_back(ds);
	return ds;
}