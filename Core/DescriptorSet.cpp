#include <Core/DescriptorSet.hpp>
#include <Core/Device.hpp>
#include <Core/Buffer.hpp>
#include <Content/Texture.hpp>

using namespace std;

DescriptorSet::DescriptorSet(const string& name, Device* device, VkDescriptorSetLayout layout) : mDevice(device), mLayout(layout) {
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = mDevice->mDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;
	ThrowIfFailed(vkAllocateDescriptorSets(*mDevice, &allocInfo, &mDescriptorSet), "vkAllocateDescriptorSets failed");
	mDevice->SetObjectName(mDescriptorSet, name, VK_OBJECT_TYPE_DESCRIPTOR_SET);
}
DescriptorSet::~DescriptorSet() {
	for (void*& d : mPendingData)
		safe_delete(d);
	ThrowIfFailed(vkFreeDescriptorSets(*mDevice, mDevice->mDescriptorPool, 1, &mDescriptorSet), "vkFreeDescriptorSets failed");
}

void DescriptorSet::CreateStorageBufferDescriptor(Buffer* buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding) {
	VkDescriptorBufferInfo* info = new VkDescriptorBufferInfo();
	info->buffer = *buffer;
	info->offset = offset;
	info->range = range;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pBufferInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingData.push_back(info);
}
void DescriptorSet::CreateStorageBufferDescriptor(Buffer* buffer, uint32_t binding) {
	CreateStorageBufferDescriptor(buffer, 0, buffer->Size(), binding);
}

void DescriptorSet::CreateUniformBufferDescriptor(Buffer* buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding) {
	VkDescriptorBufferInfo* info = new VkDescriptorBufferInfo();
	info->buffer = *buffer;
	info->offset = offset;
	info->range = range;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pBufferInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingData.push_back(info);
}
void DescriptorSet::CreateUniformBufferDescriptor(Buffer* buffer, uint32_t binding) {
	CreateUniformBufferDescriptor(buffer, 0, buffer->Size(), binding);
}

void DescriptorSet::CreateStorageTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout) {
	VkDescriptorImageInfo* info = new VkDescriptorImageInfo();
	info->imageLayout = layout;
	info->imageView = texture->View(mDevice);
	info->sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingData.push_back(info);
}
void DescriptorSet::CreateStorageTextureDescriptor(Texture* texture, uint32_t index, uint32_t binding, VkImageLayout layout) {
	VkDescriptorImageInfo* info = new VkDescriptorImageInfo();
	info->imageLayout = layout;
	info->imageView = texture->View(mDevice);
	info->sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = index;
	write.pImageInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingData.push_back(info);
}
void DescriptorSet::CreateSampledTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout) {
	VkDescriptorImageInfo* info = new VkDescriptorImageInfo();
	info->imageLayout = layout;
	info->imageView = texture->View(mDevice);
	info->sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingData.push_back(info);
}
void DescriptorSet::CreateSampledTextureDescriptor(Texture* texture, uint32_t index, uint32_t binding, VkImageLayout layout) {
	VkDescriptorImageInfo* info = new VkDescriptorImageInfo();
	info->imageLayout = layout;
	info->imageView = texture->View(mDevice);
	info->sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = index;
	write.pImageInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingData.push_back(info);
}

void DescriptorSet::CreateSamplerDescriptor(Sampler* sampler, uint32_t binding) {
	VkDescriptorImageInfo* info = new VkDescriptorImageInfo();
	info->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info->imageView = VK_NULL_HANDLE;
	info->sampler = *sampler;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	write.descriptorCount = 1;
	mPending.push_back(write);
	mPendingData.push_back(info);
}

void DescriptorSet::FlushWrites() {
	vkUpdateDescriptorSets(*mDevice, (uint32_t)mPending.size(), mPending.data(), 0, nullptr);
	mPending.clear();
	for (void*& d : mPendingData)
		safe_delete(d);
	mPendingData.clear();
}