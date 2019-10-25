#include <Core/CommandBuffer.hpp>
#include <Core/RenderPass.hpp>
#include <Content/Material.hpp>
#include <Content/Shader.hpp>
#include <Core/Device.hpp>
#include <Scene/Camera.hpp>
#include <Util/Util.hpp>
#include <cstring>

#include <Shaders/shadercompat.h>

using namespace std;

Fence::Fence(Device* device) : mDevice(device) {
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	ThrowIfFailed(vkCreateFence(*mDevice, &fenceInfo, nullptr, &mFence), "vkCreateFence failed");
}
Fence::~Fence() {
	vkDestroyFence(*mDevice, mFence, nullptr);
}
void Fence::Wait(){
	vkWaitForFences(*mDevice, 1, &mFence, true, numeric_limits<uint64_t>::max());
}
bool Fence::Signaled() {
	return vkGetFenceStatus(*mDevice, mFence) == VK_SUCCESS;
}
void Fence::Reset(){
	vkResetFences(*mDevice, 1, &mFence);
}

CommandBuffer::CommandBuffer(::Device* device, VkCommandPool commandPool, const string& name)
	: mDevice(device), mCommandPool(commandPool), mCurrentRenderPass(nullptr), mCurrentPipeline(VK_NULL_HANDLE) {
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = mCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	ThrowIfFailed(vkAllocateCommandBuffers(*mDevice, &allocInfo, &mCommandBuffer), "vkAllocateCommandBuffers failed");

	mCompletionFence = make_shared<Fence>(device);

	mDevice->SetObjectName(mCommandBuffer, name);
	mDevice->SetObjectName((VkFence)*mCompletionFence, name + " Fence");
}
CommandBuffer::~CommandBuffer() {
	vkFreeCommandBuffers(*mDevice, mCommandPool, 1, &mCommandBuffer);
}

#ifdef ENABLE_DEBUG_LAYERS
void CommandBuffer::BeginLabel(const string& text, const float4& color) {
	VkDebugUtilsLabelEXT label = {};
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	memcpy(label.color, &color, sizeof(color));
	label.pLabelName = text.c_str();
	mDevice->CmdBeginDebugUtilsLabelEXT(mCommandBuffer, &label);
}
void CommandBuffer::EndLabel() {
	mDevice->CmdEndDebugUtilsLabelEXT(mCommandBuffer);
}
#endif

void CommandBuffer::Reset(const string& name) {
	vkResetCommandBuffer(mCommandBuffer, 0);
	mCompletionFence->Reset();

	mDevice->SetObjectName(mCommandBuffer, name);
	mDevice->SetObjectName((VkFence)*mCompletionFence, name + " Fence");
	mCurrentRenderPass = nullptr;
	mCurrentPipeline = VK_NULL_HANDLE;
}

void CommandBuffer::BeginRenderPass(RenderPass* renderPass, const VkExtent2D& bufferSize, VkFramebuffer frameBuffer, VkClearValue* clearValues, uint32_t clearValueCount) {
	VkRenderPassBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.renderPass = *renderPass;
	info.clearValueCount = clearValueCount;
	info.pClearValues = clearValues;
	info.renderArea = { { 0, 0 }, bufferSize };
	info.framebuffer = frameBuffer;
	vkCmdBeginRenderPass(*this, &info, VK_SUBPASS_CONTENTS_INLINE);

	mCurrentRenderPass = renderPass;
}
void CommandBuffer::EndRenderPass() {
	vkCmdEndRenderPass(*this);
	mCurrentRenderPass = nullptr;
	mCurrentPipeline = VK_NULL_HANDLE;
}

VkPipelineLayout CommandBuffer::BindShader(GraphicsShader* shader, uint32_t backBufferIndex, const VertexInput* input, VkPrimitiveTopology topology) {
	VkPipeline pipeline = shader->GetPipeline(mCurrentRenderPass, input, topology);
	if (mCurrentPipeline == pipeline) return shader->mPipelineLayout;
	mCurrentPipeline = pipeline;
	vkCmdBindPipeline(*this, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	if (mCurrentRenderPass && mCurrentRenderPass->Camera() && shader->mDescriptorBindings.count("Camera")) {
		VkDescriptorSet camds = *mCurrentRenderPass->Camera()->DescriptorSet(backBufferIndex, shader->mDescriptorBindings.at("Camera").second.stageFlags);
		vkCmdBindDescriptorSets(*this, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->mPipelineLayout, PER_CAMERA, 1, &camds, 0, nullptr);
	}
	return shader->mPipelineLayout;
}
VkPipelineLayout CommandBuffer::BindMaterial(Material* material, uint32_t backBufferIndex, const VertexInput* input, VkPrimitiveTopology topology) {
	VkPipeline pipeline = material->GetShader(mDevice)->GetPipeline(mCurrentRenderPass, input, topology);
	if (pipeline == mCurrentPipeline) return material->GetShader(mDevice)->mPipelineLayout;
	mCurrentPipeline = pipeline;
	return material->Bind(this, backBufferIndex, mCurrentRenderPass, input, topology);
}