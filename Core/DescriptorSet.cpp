#include <Core/DescriptorSet.hpp>
#include <Core/DescriptorPool.hpp>

using namespace std;

DescriptorSet::DescriptorSet(const string& name, DescriptorPool* pool, VkDescriptorSetLayout layout) : mDescriptorPool(pool), mLayout(layout) {
	mDescriptorSet = pool->AllocateDescriptorSet(layout);
	pool->Device()->SetObjectName(mDescriptorSet, name, VK_OBJECT_TYPE_DESCRIPTOR_SET);
}
DescriptorSet::~DescriptorSet() {
	mDescriptorPool->FreeDescriptorSet(mDescriptorSet);
}

void DescriptorSet::CreateStorageBufferDescriptor(Buffer* buffer, uint32_t binding) {
	VkDescriptorBufferInfo info = {};
	info.buffer = *buffer;
	info.offset = 0;
	info.range = buffer->Size();

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pBufferInfo = &info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.descriptorCount = 1;
	vkUpdateDescriptorSets(*mDescriptorPool->Device(), 1, &write, 0, nullptr);
}
void DescriptorSet::CreateUniformBufferDescriptor(Buffer* buffer, uint32_t binding) {
	VkDescriptorBufferInfo info = {};
	info.buffer = *buffer;
	info.offset = 0;
	info.range = buffer->Size();

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pBufferInfo = &info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.descriptorCount = 1;
	vkUpdateDescriptorSets(*mDescriptorPool->Device(), 1, &write, 0, nullptr);
}
void DescriptorSet::CreateStorageTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout) {
	VkDescriptorImageInfo info = {};
	info.imageLayout = layout;
	info.imageView = texture->View(mDescriptorPool->Device());
	info.sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = &info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.descriptorCount = 1;
	vkUpdateDescriptorSets(*mDescriptorPool->Device(), 1, &write, 0, nullptr);
}
void DescriptorSet::CreateStorageTextureDescriptor(Texture** textures, uint32_t count, uint32_t arraySize, uint32_t binding, VkImageLayout layout) {
	vector<VkDescriptorImageInfo> infos(arraySize);
	for (uint32_t i = 0; i < arraySize; i++) {
		infos[i].imageLayout = layout;
		infos[i].sampler = VK_NULL_HANDLE;
		if (i >= count)
			infos[i].imageView = textures[0]->View(mDescriptorPool->Device());
		else
			infos[i].imageView = textures[i]->View(mDescriptorPool->Device());
	}

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = infos.data();
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.descriptorCount = arraySize;
	vkUpdateDescriptorSets(*mDescriptorPool->Device(), 1, &write, 0, nullptr);
}
void DescriptorSet::CreateSampledTextureDescriptor(Texture** textures, uint32_t count, uint32_t arraySize, uint32_t binding, VkImageLayout layout) {
	vector<VkDescriptorImageInfo> infos(arraySize);
	for (uint32_t i = 0; i < arraySize; i++){
		infos[i].imageLayout = layout;
		infos[i].sampler = VK_NULL_HANDLE;
		if (i >= count)
			infos[i].imageView = textures[0]->View(mDescriptorPool->Device());
		else
			infos[i].imageView = textures[i]->View(mDescriptorPool->Device());
	}

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = infos.data();
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.descriptorCount = arraySize;
	vkUpdateDescriptorSets(*mDescriptorPool->Device(), 1, &write, 0, nullptr);
}
void DescriptorSet::CreateSampledTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout) {
	VkDescriptorImageInfo info = {};
	info.imageLayout = layout;
	info.imageView = texture->View(mDescriptorPool->Device());
	info.sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = mDescriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = &info;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.descriptorCount = 1;
	vkUpdateDescriptorSets(*mDescriptorPool->Device(), 1, &write, 0, nullptr);
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
	vkUpdateDescriptorSets(*mDescriptorPool->Device(), 1, &write, 0, nullptr);
}