#include <Core/CommandBuffer.hpp>
#include <Core/Framebuffer.hpp>
#include <Util/Profiler.hpp>

using namespace std;

Framebuffer::Framebuffer(const string& name, ::Device* device, uint32_t width, uint32_t height, 
	const vector<VkFormat>& colorFormats, VkFormat depthFormat, VkSampleCountFlagBits sampleCount,
	const vector<VkSubpassDependency>& dependencies, VkAttachmentLoadOp loadOp)
	: mName(name), mDevice(device), mRenderPass(nullptr),
	mWidth(width), mHeight(height), mSampleCount(sampleCount), mColorFormats(colorFormats), mDepthFormat(depthFormat), mSubpassDependencies(dependencies), mLoadOp(loadOp) {

	mFramebuffers = new VkFramebuffer[mDevice->MaxFramesInFlight()];
	mColorBuffers = colorFormats.size() ? new vector<Texture*>[mDevice->MaxFramesInFlight()] : nullptr;
	mDepthBuffers = new Texture*[mDevice->MaxFramesInFlight()];

	memset(mFramebuffers, VK_NULL_HANDLE, sizeof(VkFramebuffer) * mDevice->MaxFramesInFlight());
	memset(mDepthBuffers, 0, sizeof(Texture*) * mDevice->MaxFramesInFlight());

	if (mColorBuffers) {
		for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++) {
			mColorBuffers[i] = vector<Texture*>(colorFormats.size());
			memset(mColorBuffers[i].data(), 0, sizeof(Texture*) * colorFormats.size());
		}
	}

	mClearValues.resize(mColorFormats.size() + 1);
	for (uint32_t i = 0; i < mColorFormats.size(); i++)
		mClearValues[i] = { .0f, .0f, .0f, 0.f };
	mClearValues[mColorFormats.size()] = { 1.f, 0.f };
}
Framebuffer::~Framebuffer() {
	safe_delete(mRenderPass);
	for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++) {
		if (mFramebuffers[i] != VK_NULL_HANDLE)
			vkDestroyFramebuffer(*mDevice, mFramebuffers[i], nullptr);
		if (mColorBuffers)
			for (Texture* t : mColorBuffers[i])
				safe_delete(t);
		safe_delete(mDepthBuffers[i]);
	}
	safe_delete_array(mColorBuffers);
	safe_delete_array(mDepthBuffers);
	safe_delete_array(mFramebuffers);
}

void Framebuffer::CreateRenderPass() {
	PROFILER_BEGIN("Create RenderPass");
	vector<VkAttachmentReference> colorAttachments(mColorFormats.size());
	vector<VkAttachmentDescription> attachments(mColorFormats.size() + 1);
	for (uint32_t i = 0; i < mColorFormats.size(); i++) {
		attachments[i].format = mColorFormats[i];
		attachments[i].samples = mSampleCount;
		attachments[i].loadOp = mLoadOp;
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorAttachments[i].attachment = i;
		colorAttachments[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	attachments[mColorFormats.size()].format = mDepthFormat;
	attachments[mColorFormats.size()].samples = mSampleCount;
	attachments[mColorFormats.size()].loadOp = mLoadOp;
	attachments[mColorFormats.size()].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[mColorFormats.size()].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[mColorFormats.size()].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[mColorFormats.size()].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[mColorFormats.size()].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = (uint32_t)mColorFormats.size();
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	vector<VkSubpassDescription> subpasses(1);
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = (uint32_t)mColorFormats.size();
	subpasses[0].pColorAttachments = colorAttachments.data();
	subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;

	safe_delete(mRenderPass);
	mRenderPass = new ::RenderPass(mName + "RenderPass", this, attachments, subpasses, mSubpassDependencies);
	PROFILER_END;
}

bool Framebuffer::UpdateBuffers() {
	uint32_t frameContextIndex = mDevice->FrameContextIndex();

	if (!mRenderPass || mRenderPass->RasterizationSamples() != mSampleCount) CreateRenderPass();

	if (mFramebuffers[frameContextIndex] == VK_NULL_HANDLE
		|| mDepthBuffers[frameContextIndex]->Width() != mWidth || mDepthBuffers[frameContextIndex]->Height() != mHeight || mDepthBuffers[frameContextIndex]->SampleCount() != mSampleCount) {
		
		PROFILER_BEGIN("Create Framebuffers");
		if (mFramebuffers[frameContextIndex] != VK_NULL_HANDLE)
			vkDestroyFramebuffer(*mDevice, mFramebuffers[frameContextIndex], nullptr);

		vector<VkImageView> views((mColorBuffers ? mColorBuffers[frameContextIndex].size() : 0) + 1);

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		if (mSampleCount == VK_SAMPLE_COUNT_1_BIT) usage |= VK_IMAGE_USAGE_STORAGE_BIT;

		for (uint32_t i = 0; i < mColorFormats.size(); i++) {
			safe_delete(mColorBuffers[frameContextIndex][i]);
			mColorBuffers[frameContextIndex][i] = new Texture(mName + "ColorBuffer", mDevice, mWidth, mHeight, 1, mColorFormats[i],
				mSampleCount, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			views[i] = mColorBuffers[frameContextIndex][i]->View();
		}

		safe_delete(mDepthBuffers[frameContextIndex]);
		mDepthBuffers[frameContextIndex] = new Texture(mName + "DepthBuffer", mDevice, mWidth, mHeight, 1, mDepthFormat, mSampleCount, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		views[views.size() - 1] = mDepthBuffers[frameContextIndex]->View();

		VkFramebufferCreateInfo fb = {};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.attachmentCount = (uint32_t)views.size();
		fb.pAttachments = views.data();
		fb.renderPass = *mRenderPass;
		fb.width = mWidth;
		fb.height = mHeight;
		fb.layers = 1;
		vkCreateFramebuffer(*mDevice, &fb, nullptr, &mFramebuffers[frameContextIndex]);
		mDevice->SetObjectName(mFramebuffers[frameContextIndex], mName + " Framebuffer " + to_string(frameContextIndex), VK_OBJECT_TYPE_FRAMEBUFFER);
		PROFILER_END;
		return true;
	}
	return false;
}

void Framebuffer::BeginRenderPass(CommandBuffer* commandBuffer) {
	uint32_t frameContextIndex = mDevice->FrameContextIndex();
	if (UpdateBuffers()) {
		if (mColorFormats.size()) {
			VkPipelineStageFlags srcStage, destStage;
			vector<VkImageMemoryBarrier> barriers;
			for (uint32_t i = 0; i < mColorFormats.size(); i++)
				barriers.push_back(mColorBuffers[frameContextIndex][i]->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, srcStage, destStage));
			vkCmdPipelineBarrier(*commandBuffer,
				srcStage, destStage,
				0,
				0, nullptr,
				0, nullptr,
				(uint32_t)barriers.size(), barriers.data());
		}
		mDepthBuffers[frameContextIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, commandBuffer);
	}
	commandBuffer->BeginRenderPass(mRenderPass, { mWidth, mHeight }, mFramebuffers[frameContextIndex], mClearValues.data(), (uint32_t)mClearValues.size());
}

void Framebuffer::Clear(CommandBuffer* commandBuffer) {
	vector<VkClearAttachment> clears(mClearValues.size());
	for (uint32_t i = 0; i < mClearValues.size(); i++) {
		clears[i] = {};
		clears[i].clearValue = mClearValues[i];
		clears[i].colorAttachment = i;
		clears[i].aspectMask = i == mColorFormats.size() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	}

	VkClearRect rect = {};
	rect.layerCount = 1;
	rect.rect.extent = { mWidth, mHeight };
	vkCmdClearAttachments(*commandBuffer, clears.size(), clears.data(), 1, &rect);
}

void Framebuffer::ResolveColor(CommandBuffer* commandBuffer, uint32_t index, VkImage destination) {
	if (!mColorBuffers) return;

	uint32_t frameContextIndex = mDevice->FrameContextIndex();

	mColorBuffers[frameContextIndex][index]->TransitionImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer);

	if (mSampleCount == VK_SAMPLE_COUNT_1_BIT) {
		VkImageCopy region = {};
		region.extent = { mWidth, mHeight, 1 };
		region.dstSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.layerCount = 1;
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vkCmdCopyImage(*commandBuffer,
			mColorBuffers[frameContextIndex][index]->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	} else {
		VkImageResolve region = {};
		region.extent = { mWidth, mHeight, 1 };
		region.dstSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.layerCount = 1;
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vkCmdResolveImage(*commandBuffer,
			mColorBuffers[frameContextIndex][index]->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	mColorBuffers[frameContextIndex][index]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);
}
void Framebuffer::ResolveDepth(CommandBuffer* commandBuffer, VkImage destination) {
	uint32_t frameContextIndex = mDevice->FrameContextIndex();

	mDepthBuffers[frameContextIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer);

	if (mSampleCount == VK_SAMPLE_COUNT_1_BIT) {
		VkImageCopy region = {};
		region.extent = { mWidth, mHeight, 1 };
		region.dstSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.srcSubresource.layerCount = 1;
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		vkCmdCopyImage(*commandBuffer,
			mDepthBuffers[frameContextIndex]->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	} else {
		VkImageResolve region = {};
		region.extent = { mWidth, mHeight, 1 };
		region.dstSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.srcSubresource.layerCount = 1;
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		vkCmdResolveImage(*commandBuffer,
			mDepthBuffers[frameContextIndex]->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	mDepthBuffers[frameContextIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, commandBuffer);
}