#pragma once

#include <Content/Texture.hpp>
#include <Core/RenderPass.hpp>
#include <Util/Util.hpp>

class CommandBuffer;

class Framebuffer {
public:
	const std::string mName;

	ENGINE_EXPORT Framebuffer(const std::string& name, ::Device* device, uint32_t width, uint32_t height,
		const std::vector<VkFormat>& colorFormats, VkFormat depthFormat, VkSampleCountFlagBits sampleCount,
		const std::vector<VkSubpassDependency>& dependencies, VkAttachmentLoadOp loadOp);
	ENGINE_EXPORT ~Framebuffer();

	inline void Width(uint32_t w) { mWidth = w; }
	inline void Height(uint32_t h) { mHeight = h; }
	inline void SampleCount(VkSampleCountFlagBits s) { mSampleCount = s; }

	inline uint32_t Width() const { return mWidth; }
	inline uint32_t Height() const { return mHeight; }
	inline VkSampleCountFlagBits SampleCount() const { return mSampleCount; }

	inline void ClearValue(uint32_t i, const VkClearValue& value) { mClearValues[i] = value; }

	inline Texture* ColorBuffer(uint32_t i) { return mColorBuffers[mDevice->FrameContextIndex()][i]; }
	inline Texture* DepthBuffer() { return mDepthBuffers[mDevice->FrameContextIndex()]; }

	ENGINE_EXPORT void ResolveColor(CommandBuffer* commandBuffer, uint32_t index, VkImage destination);
	ENGINE_EXPORT void ResolveDepth(CommandBuffer* commandBuffer, VkImage destination);

	inline uint32_t ColorBufferCount() const { return mColorBuffers ? (uint32_t)mColorBuffers[mDevice->FrameContextIndex()].size() : 0; }

	ENGINE_EXPORT void Clear(CommandBuffer* commandBuffer);
	ENGINE_EXPORT void BeginRenderPass(CommandBuffer* commandBuffer);
	inline ::RenderPass* RenderPass() const { return mRenderPass; }
	inline ::Device* Device() const { return mDevice; }

private:
	::Device* mDevice;
	std::vector<Texture*>* mColorBuffers;
	Texture** mDepthBuffers;
	VkFramebuffer* mFramebuffers;
	::RenderPass* mRenderPass;

	std::vector<VkSubpassDependency> mSubpassDependencies;
	VkAttachmentLoadOp mLoadOp;

	uint32_t mWidth;
	uint32_t mHeight;
	VkSampleCountFlagBits mSampleCount;
	std::vector<VkFormat> mColorFormats;
	std::vector<VkClearValue> mClearValues;
	VkFormat mDepthFormat;

	ENGINE_EXPORT void CreateRenderPass();
	ENGINE_EXPORT bool UpdateBuffers();
};
