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
	ENGINE_EXPORT void CreateStorageTextureDescriptor(Texture* texture, uint32_t binding);
	ENGINE_EXPORT void CreateTextureDescriptor(Texture* texture, uint32_t binding);
	ENGINE_EXPORT void CreateSamplerDescriptor(Sampler* sampler, uint32_t binding);

	inline operator VkDescriptorSet() const { return mDescriptorSet; }

private:
	DescriptorPool* mDescriptorPool;
	VkDescriptorSet mDescriptorSet;
};