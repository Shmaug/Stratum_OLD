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
	ThrowIfFailed(vkFreeDescriptorSets(*mDevice, mDevice->mDescriptorPool, 1, &mDescriptorSet), "vkFreeDescriptorSets failed");
}

void DescriptorSet::CreateStorageBufferDescriptor(Buffer* buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding) {
	VkDescriptorBufferInfo info = {};
	info.buffer = *buffer;
	info.offset = offset;
	info.range = range;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pBufferInfo = &info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.descriptorCount = 1;
	vkUpdateDescriptorSets(*mDevice, 1, &write, 0, nullptr);
}
void DescriptorSet::CreateUniformBufferDescriptor(Buffer* buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding) {
	VkDescriptorBufferInfo info = {};
	info.buffer = *buffer;
	info.offset = offset;
	info.range = range;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pBufferInfo = &info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.descriptorCount = 1;
	vkUpdateDescriptorSets(*mDevice, 1, &write, 0, nullptr);
}
void DescriptorSet::CreateStorageTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout) {
	VkDescriptorImageInfo info = {};
	info.imageLayout = layout;
	info.imageView = texture->View(mDevice);
	info.sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = &info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.descriptorCount = 1;
	vkUpdateDescriptorSets(*mDevice, 1, &write, 0, nullptr);
}
void DescriptorSet::CreateStorageTextureDescriptor(Texture** textures, uint32_t count, uint32_t arraySize, uint32_t binding, VkImageLayout layout) {
	vector<VkDescriptorImageInfo> infos(arraySize);
	for (uint32_t i = 0; i < arraySize; i++) {
		infos[i].imageLayout = layout;
		infos[i].sampler = VK_NULL_HANDLE;
		if (i >= count)
			infos[i].imageView = textures[0]->View(mDevice);
		else
			infos[i].imageView = textures[i]->View(mDevice);
	}

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = infos.data();
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.descriptorCount = arraySize;
	vkUpdateDescriptorSets(*mDevice, 1, &write, 0, nullptr);
}
void DescriptorSet::CreateSampledTextureDescriptor(Texture** textures, uint32_t count, uint32_t arraySize, uint32_t binding, VkImageLayout layout) {
	vector<VkDescriptorImageInfo> infos(arraySize);
	for (uint32_t i = 0; i < arraySize; i++){
		infos[i].imageLayout = layout;
		infos[i].sampler = VK_NULL_HANDLE;
		if (i >= count)
			infos[i].imageView = textures[0]->View(mDevice);
		else
			infos[i].imageView = textures[i]->View(mDevice);
	}

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = infos.data();
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.descriptorCount = arraySize;
	vkUpdateDescriptorSets(*mDevice, 1, &write, 0, nullptr);
}
void DescriptorSet::CreateSampledTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout) {
	VkDescriptorImageInfo info = {};
	info.imageLayout = layout;
	info.imageView = texture->View(mDevice);
	info.sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = &info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.descriptorCount = 1;
	vkUpdateDescriptorSets(*mDevice, 1, &write, 0, nullptr);
}
void DescriptorSet::CreateSamplerDescriptor(Sampler* sampler, uint32_t binding) {
	VkDescriptorImageInfo info = {};
	info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.imageView = VK_NULL_HANDLE;
	info.sampler = *sampler;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = &info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	write.descriptorCount = 1;
	vkUpdateDescriptorSets(*mDevice, 1, &write, 0, nullptr);
}