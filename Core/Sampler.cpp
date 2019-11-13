#include <Core/Sampler.hpp>
#include <Core/Device.hpp>

using namespace std;

Sampler::Sampler(const string& name, Device* device, const VkSamplerCreateInfo& samplerInfo)
	: mName(name), mDevice(device) {
	ThrowIfFailed(vkCreateSampler(*mDevice, &samplerInfo, nullptr, &mSampler), "vkCreateSampler failed");
	mDevice->SetObjectName(mSampler, mName, VK_OBJECT_TYPE_SAMPLER);
}
Sampler::Sampler(const string& name, Device* device, float maxLod, VkFilter filter, VkSamplerAddressMode addressMode, float maxAnisotropy)
	: mName(name), mDevice(device) {
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = filter;
	samplerInfo.minFilter = filter;
	samplerInfo.addressModeU = addressMode;
	samplerInfo.addressModeV = addressMode;
	samplerInfo.addressModeW = addressMode;
	samplerInfo.anisotropyEnable = maxAnisotropy > 0 ? VK_TRUE : VK_FALSE;
	samplerInfo.maxAnisotropy = maxAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = (float)maxLod;
	samplerInfo.mipLodBias = 0;

	ThrowIfFailed(vkCreateSampler(*mDevice, &samplerInfo, nullptr, &mSampler), "vkCreateSampler failed");
	mDevice->SetObjectName(mSampler, mName, VK_OBJECT_TYPE_SAMPLER);
}
Sampler::~Sampler() {
	vkDestroySampler(*mDevice, mSampler, nullptr);
}