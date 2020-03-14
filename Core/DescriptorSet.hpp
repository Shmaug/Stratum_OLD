#pragma once

#include <Util/Util.hpp>

class Device;
class Texture;
class Buffer;
class Sampler;

class DescriptorSet {
public:
	ENGINE_EXPORT DescriptorSet(const std::string& name, Device* device, VkDescriptorSetLayout layout);
	ENGINE_EXPORT ~DescriptorSet();

	ENGINE_EXPORT void CreateStorageBufferDescriptor(Buffer* buffer, uint32_t index, VkDeviceSize offset, VkDeviceSize range, uint32_t binding);
	ENGINE_EXPORT void CreateStorageBufferDescriptor(Buffer* buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding);
	ENGINE_EXPORT void CreateStorageTexelBufferDescriptor(Buffer* view, uint32_t binding);
	ENGINE_EXPORT void CreateUniformBufferDescriptor(Buffer* buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding);
	
	ENGINE_EXPORT void CreateStorageTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
	ENGINE_EXPORT void CreateStorageTextureDescriptor(Texture* texture, uint32_t index, uint32_t binding, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
	
	ENGINE_EXPORT void CreateSampledTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	ENGINE_EXPORT void CreateSampledTextureDescriptor(Texture* texture, uint32_t index, uint32_t binding, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	ENGINE_EXPORT void CreateSamplerDescriptor(Sampler* sampler, uint32_t binding);

	ENGINE_EXPORT void FlushWrites();

	inline VkDescriptorSetLayout Layout() const { return mLayout; }
	inline operator const VkDescriptorSet*() const { return &mDescriptorSet; }
	inline operator VkDescriptorSet() const { return mDescriptorSet; }

private:
	std::unordered_map<uint64_t, VkWriteDescriptorSet> mCurrent;

	std::vector<VkWriteDescriptorSet> mPending;
	std::queue<VkDescriptorBufferInfo*> mBufferInfoPool;
	std::queue<VkDescriptorImageInfo*> mImageInfoPool;
	std::vector<VkDescriptorBufferInfo*> mPendingBuffers;
	std::vector<VkDescriptorImageInfo*>  mPendingImages;

	Device* mDevice;
	VkDescriptorSet mDescriptorSet;
	VkDescriptorSetLayout mLayout;
};