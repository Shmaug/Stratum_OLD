#include <Core/RenderPass.hpp>
#include <Core/Framebuffer.hpp>

using namespace std;

RenderPass::RenderPass(const string& name, ::Device* device,
	const vector<VkAttachmentDescription>& attachments,
	const vector<VkSubpassDescription>& subpasses,
	const vector<VkSubpassDependency>& dependencies)
	: mName(name), mDevice(device), mFramebuffer(nullptr) {
	mRasterizationSamples = attachments[subpasses[0].pDepthStencilAttachment->attachment].samples;
	mColorAttachmentCount = subpasses[0].colorAttachmentCount;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = (uint32_t)attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = (uint32_t)subpasses.size();
	renderPassInfo.pSubpasses = subpasses.data();
	renderPassInfo.dependencyCount = (uint32_t)dependencies.size();
	renderPassInfo.pDependencies = dependencies.data();
	ThrowIfFailed(vkCreateRenderPass(*mDevice, &renderPassInfo, nullptr, &mRenderPass), "vkCreateRenderPass failed");
	mDevice->SetObjectName(mRenderPass, mName + " RenderPass", VK_OBJECT_TYPE_RENDER_PASS);
}
RenderPass::RenderPass(const string& name, ::Framebuffer* frameBuffer,
	const vector<VkAttachmentDescription>& attachments,
	const vector<VkSubpassDescription>& subpasses,
	const vector<VkSubpassDependency>& dependencies)
	: RenderPass(name, frameBuffer->Device(), attachments, subpasses, dependencies) {
	mFramebuffer = frameBuffer;
}
RenderPass::~RenderPass() {
	vkDestroyRenderPass(*mDevice, mRenderPass, nullptr);
}