#pragma once

#include <glm/glm.hpp>
#include <Util/Util.hpp>

class Device;
class Material;
class RenderPass;

class Fence {
public:
	ENGINE_EXPORT Fence(Device* device);
	ENGINE_EXPORT ~Fence();
	ENGINE_EXPORT void Wait();
	ENGINE_EXPORT bool Signaled();
	ENGINE_EXPORT void Reset();
	inline operator VkFence() const { return mFence; }
private:
	Device* mDevice;
	VkFence mFence;
};

class CommandBuffer {
public:
	ENGINE_EXPORT ~CommandBuffer();
	inline operator VkCommandBuffer() const { return mCommandBuffer; }

	ENGINE_EXPORT void Reset(const std::string& name = "Command Buffer");

	inline RenderPass* CurrentRenderPass() const { return mCurrentRenderPass; }

	ENGINE_EXPORT VkPipelineLayout BindMaterial(Material* material, uint32_t backBufferIndex, const VertexInput* input);
	ENGINE_EXPORT void BeginRenderPass(RenderPass* renderPass, const VkExtent2D& bufferSize, VkFramebuffer frameBuffer, VkClearValue* clearValues, uint32_t clearValueCount);
	ENGINE_EXPORT void EndRenderPass();

	inline ::Device* Device() const { return mDevice; }

private:
	friend class Device;
	ENGINE_EXPORT CommandBuffer(::Device* device, VkCommandPool commandPool, const std::string& name = "Command Buffer");
	::Device* mDevice;
	VkCommandBuffer mCommandBuffer;
	VkCommandPool mCommandPool;
	std::shared_ptr<Fence> mCompletionFence;

	RenderPass* mCurrentRenderPass;
	Material* mCurrentMaterial;
};