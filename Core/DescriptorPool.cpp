#include <Core/DescriptorPool.hpp>

using namespace std;

DescriptorPool::DescriptorPool(const string& name, ::Device* device) : mDevice(device), mAllocatedDescriptorCount(0) {
	const VkPhysicalDeviceLimits& limits = device->Limits();

	VkDescriptorPoolSize type_count[] {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				limits.maxDescriptorSetUniformBuffers },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		limits.maxDescriptorSetSampledImages },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,					limits.maxDescriptorSetSampledImages },
		{ VK_DESCRIPTOR_TYPE_SAMPLER,						limits.maxDescriptorSetSamplers },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,				limits.maxDescriptorSetStorageBuffers },
	};

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.poolSizeCount = 5;
	poolInfo.pPoolSizes = type_count;
	poolInfo.maxSets = 10000;

	ThrowIfFailed(vkCreateDescriptorPool(*mDevice, &poolInfo, nullptr, &mDescriptorPool), "vkCreateDescriptorPool failed");
	mDevice->SetObjectName(mDescriptorPool, name, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
}

DescriptorPool::~DescriptorPool() {
	vkDestroyDescriptorPool(*mDevice, mDescriptorPool, nullptr);
}

VkDescriptorSet DescriptorPool::AllocateDescriptorSet(VkDescriptorSetLayout layout) {
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = mDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;
	VkDescriptorSet descriptorSet;
	ThrowIfFailed(vkAllocateDescriptorSets(*mDevice, &allocInfo, &descriptorSet), "vkAllocateDescriptorSets failed");
	mAllocatedDescriptorCount++;
	return descriptorSet;
}
void DescriptorPool::FreeDescriptorSet(VkDescriptorSet descriptorSet) {
	ThrowIfFailed(vkFreeDescriptorSets(*mDevice, mDescriptorPool, 1, &descriptorSet), "vkFreeDescriptorSets failed");
	mAllocatedDescriptorCount--;
}