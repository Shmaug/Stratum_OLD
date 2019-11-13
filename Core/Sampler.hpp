#pragma once

#include <Util/Util.hpp>

class Device;

class Sampler {
public:
	const std::string mName;

	ENGINE_EXPORT Sampler(const std::string& name, Device* device, const VkSamplerCreateInfo& samplerInfo);
	ENGINE_EXPORT Sampler(const std::string& name, Device* device, float maxLod, VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, float maxAnisotropy = 16);
	ENGINE_EXPORT ~Sampler();

	inline const ::VkSampler& VkSampler() const { return mSampler; }
	inline operator ::VkSampler() const { return mSampler; }

private:
	Device* mDevice;
	::VkSampler mSampler;
};