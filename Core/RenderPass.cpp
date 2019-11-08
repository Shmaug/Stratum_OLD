#include <Core/RenderPass.hpp>
#include <Core/Framebuffer.hpp>

using namespace std;

RenderPass::RenderPass(const string& name, ::Device* device, const std::vector<VkAttachmentDescription>& attachments, const std::vector<VkSubpassDescription>& subpasses)
	: mName(name), mDevice(device), mFramebuffer(nullptr) {
	mRasterizationSamples = attachments[subpasses[0].pColorAttachments[0].attachment].samples;
	mColorAttachmentCount = subpasses[0].colorAttachmentCount;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = (uint32_t)attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = (uint32_t)subpasses.size();
	renderPassInfo.pSubpasses = subpasses.data();
	ThrowIfFailed(vkCreateRenderPass(*mDevice, &renderPassInfo, nullptr, &mRenderPass), "vkCreateRenderPass failed");
	mDevice->SetObjectName(mRenderPass, mName + " RenderPass");
}
RenderPass::RenderPass(const string& name, ::Framebuffer* frameBuffer, const std::vector<VkAttachmentDescription>& attachments, const std::vector<VkSubpassDescription>& subpasses)
	: RenderPass(name, frameBuffer->Device(), attachments, subpasses) {
	mFramebuffer = frameBuffer;
}
RenderPass::~RenderPass() {
	vkDestroyRenderPass(*mDevice, mRenderPass, nullptr);
}