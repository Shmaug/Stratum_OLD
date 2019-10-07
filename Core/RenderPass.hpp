#pragma once

#include <Core/Device.hpp>
#include <Util/Util.hpp>

class Camera;

class RenderPass {
public:
	const std::string mName;

	ENGINE_EXPORT RenderPass(const std::string& name, ::Device* device, const std::vector<VkAttachmentDescription>& attachments, const std::vector<VkSubpassDescription>& subpasses);
	ENGINE_EXPORT RenderPass(const std::string& name, ::Camera* device, const std::vector<VkAttachmentDescription>& attachments, const std::vector<VkSubpassDescription>& subpasses);
	ENGINE_EXPORT ~RenderPass();

	inline uint32_t ColorAttachmentCount() const { return mColorAttachmentCount; }
	inline VkSampleCountFlagBits RasterizationSamples() const { return mRasterizationSamples; }
	inline ::Device* Device() const { return mDevice; }
	inline ::Camera* Camera() const { return mCamera; }

	inline operator VkRenderPass() const { return mRenderPass; }

private:
	::Device* mDevice;
	::Camera* mCamera;
	VkRenderPass mRenderPass;
	VkSampleCountFlagBits mRasterizationSamples;
	uint32_t mColorAttachmentCount;
};