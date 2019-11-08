#pragma once

#include <Content/Texture.hpp>
#include <Core/RenderPass.hpp>
#include <Util/Util.hpp>

class CommandBuffer;

class Framebuffer {
public:
	const std::string mName;

	ENGINE_EXPORT Framebuffer(const std::string& name, ::Device* device, uint32_t width, uint32_t height, const std::vector<VkFormat>& colorFormats, VkFormat depthFormat, VkSampleCountFlagBits sampleCount);
	ENGINE_EXPORT ~Framebuffer();

	inline void Width(uint32_t w) { mWidth = w; }
	inline void Height(uint32_t h) { mHeight = h; }
	inline void BufferUsage(VkImageUsageFlags u) { mUsage = u; }

	inline uint32_t Width() const { return mWidth; }
	inline uint32_t Height() const { return mHeight; }
	inline VkSampleCountFlagBits SampleCount() const { return mSampleCount; }
	inline VkImageUsageFlags BufferUsage() const { return mUsage; }

	inline Texture* ColorBuffer(uint32_t backBufferIndex, uint32_t i) { UpdateBuffers(backBufferIndex);  return mColorBuffers[backBufferIndex][i]; }
	inline Texture* DepthBuffer(uint32_t backBufferIndex) { UpdateBuffers(backBufferIndex); return mDepthBuffers[backBufferIndex]; }

	ENGINE_EXPORT void BeginRenderPass(CommandBuffer* commandBuffer, uint32_t backBufferIndex);
	inline ::RenderPass* RenderPass() const { return mRenderPass; }
	inline ::Device* Device() const { return mDevice; }

private:
	::Device* mDevice;
	std::vector<Texture*>* mColorBuffers;
	Texture** mDepthBuffers;
	VkFramebuffer* mFramebuffers;
	::RenderPass* mRenderPass;

	uint32_t mWidth;
	uint32_t mHeight;
	VkImageUsageFlags mUsage;
	VkSampleCountFlagBits mSampleCount;
	std::vector<VkFormat> mColorFormats;
	VkFormat mDepthFormat;

	ENGINE_EXPORT bool UpdateBuffers(uint32_t backBufferIndex);
};