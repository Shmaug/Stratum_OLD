#pragma once

#include <Core/Device.hpp>
#include <Util/Util.hpp>

class DescriptorSet;

class DescriptorPool {
public:
	ENGINE_EXPORT DescriptorPool(const std::string& name, ::Device* device);
	ENGINE_EXPORT ~DescriptorPool();

	ENGINE_EXPORT VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);
	ENGINE_EXPORT void FreeDescriptorSet(VkDescriptorSet descriptorSet);

	inline uint32_t AllocatedDescriptorCount() const { return mAllocatedDescriptorCount; }

	inline ::Device* Device() const { return mDevice; }

private:
	::Device* mDevice;
	VkDescriptorPool mDescriptorPool;
	uint32_t mAllocatedDescriptorCount;
};