#pragma once

#include <variant>

#include <Content/Texture.hpp>
#include <Core/Buffer.hpp>
#include <Core/Sampler.hpp>
#include <Core/DescriptorPool.hpp>
#include <Util/Util.hpp>

class DescriptorPool;

class DescriptorSet {
public:
	ENGINE_EXPORT DescriptorSet(const std::string& name, DescriptorPool* pool, VkDescriptorSetLayout layout);
	ENGINE_EXPORT ~DescriptorSet();

	ENGINE_EXPORT void CreateStorageBufferDescriptor(Buffer* buffer, uint32_t binding);
	
	ENGINE_EXPORT void CreateUniformBufferDescriptor(Buffer* buffer, uint32_t binding);
	
	ENGINE_EXPORT void CreateStorageTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
	ENGINE_EXPORT void CreateStorageTextureDescriptor(Texture** textures, uint32_t count, uint32_t arraySize, uint32_t binding, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
	
	ENGINE_EXPORT void CreateSampledTextureDescriptor(Texture* texture, uint32_t binding, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	ENGINE_EXPORT void CreateSampledTextureDescriptor(Texture** textures, uint32_t count, uint32_t arraySize, uint32_t binding, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	ENGINE_EXPORT void CreateSamplerDescriptor(Sampler* sampler, uint32_t binding);

	inline VkDescriptorSetLayout Layout() const { return mLayout; }
	inline operator VkDescriptorSet() const { return mDescriptorSet; }

private:
	DescriptorPool* mDescriptorPool;
	VkDescriptorSet mDescriptorSet;
	VkDescriptorSetLayout mLayout;
};