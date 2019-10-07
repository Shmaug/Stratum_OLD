#pragma once

#include <glm/glm.hpp>
#include <Util/Util.hpp>

#ifdef _DEBUG
#define ENABLE_CMD_REGION
#endif

#ifdef ENABLE_CMD_REGION
#define BEGIN_CMD_REGION(cmd, label) cmd->BeginLabel(label)
#define BEGIN_CMD_REGION_COLOR(cmd, label, color) cmd->BeginLabel(label, color)
#define END_CMD_REGION(cmd) cmd->EndLabel()
#else
#define BEGIN_CMD_REGION(cmd, label)
#define END_CMD_REGION(cmd)
#endif

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

	#ifdef ENABLE_CMD_REGION
	ENGINE_EXPORT void BeginLabel(const std::string& label, const vec4& color = vec4(1,1,1,0));
	ENGINE_EXPORT void EndLabel();
	#endif

	ENGINE_EXPORT void Reset(const std::string& name = "Command Buffer");

	inline RenderPass* CurrentRenderPass() const { return mCurrentRenderPass; }

	ENGINE_EXPORT VkPipelineLayout BindMaterial(Material* material, uint32_t backBufferIndex, const VertexInput* input, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
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