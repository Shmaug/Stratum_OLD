#pragma once

#include <Core/Device.hpp>
#include <Util/Util.hpp>

class Camera;
class Framebuffer;

class RenderPass {
public:
	const std::string mName;

	ENGINE_EXPORT RenderPass(const std::string& name, ::Device* device,
		const std::vector<VkAttachmentDescription>& attachments,
		const std::vector<VkSubpassDescription>& subpasses,
		const std::vector<VkSubpassDependency>& dependencies);
	ENGINE_EXPORT RenderPass(const std::string& name, ::Framebuffer* frameBuffer,
		const std::vector<VkAttachmentDescription>& attachments,
		const std::vector<VkSubpassDescription>& subpasses,
		const std::vector<VkSubpassDependency>& dependencies);
	ENGINE_EXPORT ~RenderPass();

	inline uint32_t ColorAttachmentCount() const { return mColorAttachmentCount; }
	inline VkSampleCountFlagBits RasterizationSamples() const { return mRasterizationSamples; }
	inline ::Device* Device() const { return mDevice; }
	inline ::Framebuffer* Framebuffer() const { return mFramebuffer; }

	inline operator VkRenderPass() const { return mRenderPass; }

private:
	::Device* mDevice;
	::Framebuffer* mFramebuffer;
	VkRenderPass mRenderPass;
	VkSampleCountFlagBits mRasterizationSamples;
	uint32_t mColorAttachmentCount;
};