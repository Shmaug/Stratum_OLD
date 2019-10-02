#pragma once

#include <Core/Device.hpp>
#include <Util/Util.hpp>

class Sampler {
public:
	const std::string mName;

	ENGINE_EXPORT Sampler(const std::string& name, Device* device, float maxLod, VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, float maxAnisotropy = 16);
	ENGINE_EXPORT ~Sampler();

	inline operator VkSampler() { return mSampler; }

private:
	float mMaxLod;
	VkFilter mFilter;
	VkSamplerAddressMode mAddressMode;
	float mMaxAnisotropy;

	Device* mDevice;
	VkSampler mSampler;
};