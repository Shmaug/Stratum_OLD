#include <Core/CommandBuffer.hpp>
#include <Core/Framebuffer.hpp>

using namespace std;

Framebuffer::Framebuffer(const string& name, ::Device* device, uint32_t width, uint32_t height, 
	const vector<VkFormat>& colorFormats, VkFormat depthFormat, VkSampleCountFlagBits sampleCount,
	const vector<VkSubpassDependency>& dependencies, VkAttachmentLoadOp loadOp)
	: mName(name), mDevice(device), mWidth(width), mHeight(height), mSampleCount(sampleCount), mColorFormats(colorFormats), mDepthFormat(depthFormat) {

	mFramebuffers = new VkFramebuffer[mDevice->MaxFramesInFlight()];
	mColorBuffers = colorFormats.size() ? new vector<Texture*>[mDevice->MaxFramesInFlight()] : nullptr;
	mDepthBuffers = new Texture*[mDevice->MaxFramesInFlight()];
	mResolveBuffers = colorFormats.size() ? new vector<Texture*>[mDevice->MaxFramesInFlight()] : nullptr;
	mResolveDepthBuffers = new Texture*[mDevice->MaxFramesInFlight()];

	memset(mFramebuffers, VK_NULL_HANDLE, sizeof(VkFramebuffer) * mDevice->MaxFramesInFlight());
	memset(mDepthBuffers, 0, sizeof(Texture*) * mDevice->MaxFramesInFlight());
	memset(mResolveDepthBuffers, 0, sizeof(Texture*) * mDevice->MaxFramesInFlight());

	for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++) {
		if (mColorBuffers) {
			mColorBuffers[i] = vector<Texture*>(colorFormats.size());
			mResolveBuffers[i] = vector<Texture*>(colorFormats.size());
			memset(mColorBuffers[i].data(), 0, sizeof(Texture*) * colorFormats.size());
			memset(mResolveBuffers[i].data(), 0, sizeof(Texture*) * colorFormats.size());
		}
	}

	mClearValues.resize(mColorFormats.size() + 1);
	for (uint32_t i = 0; i < mColorFormats.size(); i++)
		mClearValues[i] = { .0f, .0f, .0f, 0.f };
	mClearValues[mColorFormats.size()] = { 1.f, 0.f };

	vector<VkAttachmentReference> colorAttachments(colorFormats.size());
	vector<VkAttachmentDescription> attachments(colorFormats.size() + 1);
	for (uint32_t i = 0; i < colorFormats.size(); i++) {
		attachments[i].format = colorFormats[i];
		attachments[i].samples = mSampleCount;
		attachments[i].loadOp = loadOp;
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorAttachments[i].attachment = i;
		colorAttachments[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	attachments[colorFormats.size()].format = mDepthFormat;
	attachments[colorFormats.size()].samples = mSampleCount;
	attachments[colorFormats.size()].loadOp = loadOp;
	attachments[colorFormats.size()].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[colorFormats.size()].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[colorFormats.size()].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[colorFormats.size()].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[colorFormats.size()].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = (uint32_t)colorFormats.size();
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	vector<VkSubpassDescription> subpasses(1);
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = (uint32_t)mColorFormats.size();
	subpasses[0].pColorAttachments = colorAttachments.data();
	subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;

	mRenderPass = new ::RenderPass(mName + "RenderPass", this, attachments, subpasses, dependencies);
	mColorUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	mDepthUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
}
Framebuffer::~Framebuffer() {
	safe_delete(mRenderPass);
	for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++) {
		if (mFramebuffers[i] != VK_NULL_HANDLE)
			vkDestroyFramebuffer(*mDevice, mFramebuffers[i], nullptr);
		if (mColorBuffers)
			for (Texture* t : mColorBuffers[i])
				safe_delete(t);
		if (mResolveBuffers)
			for (Texture* t : mResolveBuffers[i])
				safe_delete(t);
		safe_delete(mDepthBuffers[i]);
		safe_delete(mResolveDepthBuffers[i]);
	}
	safe_delete_array(mColorBuffers);
	safe_delete_array(mResolveBuffers);
	safe_delete_array(mResolveDepthBuffers);
	safe_delete_array(mDepthBuffers);
	safe_delete_array(mFramebuffers);
}

bool Framebuffer::UpdateBuffers() {
	uint32_t frameContextIndex = mDevice->FrameContextIndex();
	if (mFramebuffers[frameContextIndex] == VK_NULL_HANDLE
		|| mDepthBuffers[frameContextIndex]->Width() != mWidth || mDepthBuffers[frameContextIndex]->Height() != mHeight) {

		if (mFramebuffers[frameContextIndex] != VK_NULL_HANDLE)
			vkDestroyFramebuffer(*mDevice, mFramebuffers[frameContextIndex], nullptr);

		vector<VkImageView> views((mColorBuffers ? mColorBuffers[frameContextIndex].size() : 0) + 1);

		for (uint32_t i = 0; i < mColorFormats.size(); i++) {
			safe_delete(mColorBuffers[frameContextIndex][i]);
			mColorBuffers[frameContextIndex][i] = new Texture(mName + "ColorBuffer", mDevice, mWidth, mHeight, 1, mColorFormats[i],
				mSampleCount, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			views[i] = mColorBuffers[frameContextIndex][i]->View(mDevice);
			
			if (mResolveBuffers){
				safe_delete(mResolveBuffers[frameContextIndex][i]);
				mResolveBuffers[frameContextIndex][i] = new Texture(mName + "ResolveBuffer", mDevice, mWidth, mHeight, 1, mColorFormats[i],
					VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			}
		}

		safe_delete(mDepthBuffers[frameContextIndex]);
		mDepthBuffers[frameContextIndex] = new Texture(mName + "DepthBuffer", mDevice, mWidth, mHeight, 1, mDepthFormat, mSampleCount, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		views[views.size() - 1] = mDepthBuffers[frameContextIndex]->View(mDevice);

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

		return true;
	}
	return false;
}

void Framebuffer::BeginRenderPass(CommandBuffer* commandBuffer) {
	uint32_t frameContextIndex = mDevice->FrameContextIndex();
	if (UpdateBuffers()){
		for (uint32_t i = 0; i < mColorFormats.size(); i++){
			mColorBuffers[frameContextIndex][i]->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);
			if (mResolveBuffers)
				mResolveBuffers[frameContextIndex][i]->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
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
void Framebuffer::ResolveColor(CommandBuffer* commandBuffer) {
	if (!mColorBuffers) return;

	uint32_t frameContextIndex = mDevice->FrameContextIndex();
	
	for (uint32_t i = 0; i < mColorBuffers[frameContextIndex].size(); i++){
		if (mResolveBuffers[frameContextIndex][i] && 
			(mResolveBuffers[frameContextIndex][i]->Width()  != mWidth || 
			 mResolveBuffers[frameContextIndex][i]->Height() != mHeight)){
			delete mResolveBuffers[frameContextIndex][i];
		}
		if (!mResolveBuffers[frameContextIndex][i]){
			mResolveBuffers[frameContextIndex][i] = new Texture(mName + " Resolve", mDevice, mWidth, mHeight, 1, mColorFormats[i], VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
			mResolveBuffers[frameContextIndex][i]->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
		}

		mColorBuffers[frameContextIndex][i]->TransitionImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer);
		mResolveBuffers[frameContextIndex][i]->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);

		if (mSampleCount == VK_SAMPLE_COUNT_1_BIT){
			VkImageCopy region = {};
			region.extent = { mWidth, mHeight, 1 };
			region.dstSubresource.layerCount = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkCmdCopyImage(*commandBuffer,
				mColorBuffers[frameContextIndex][i]->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				mResolveBuffers[frameContextIndex][i]->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}else{
			VkImageResolve region = {};
			region.extent = { mWidth, mHeight, 1 };
			region.dstSubresource.layerCount = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkCmdResolveImage(*commandBuffer,
				mColorBuffers[frameContextIndex][i]->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				mResolveBuffers[frameContextIndex][i]->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		mColorBuffers[frameContextIndex][i]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);
		mResolveBuffers[frameContextIndex][i]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
	}
}
void Framebuffer::ResolveDepth(CommandBuffer* commandBuffer) {
	uint32_t frameContextIndex = mDevice->FrameContextIndex();

	if (mResolveDepthBuffers[frameContextIndex] && 
		(mResolveDepthBuffers[frameContextIndex]->Width()  != mWidth || 
		 mResolveDepthBuffers[frameContextIndex]->Height() != mHeight)){
		delete mResolveDepthBuffers[frameContextIndex];
	}
	if (!mResolveDepthBuffers[frameContextIndex]){
		mResolveDepthBuffers[frameContextIndex] = new Texture(mName + " Resolve", mDevice, mWidth, mHeight, 1, mDepthFormat, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		mResolveDepthBuffers[frameContextIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
	}

	mDepthBuffers[frameContextIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer);
	mResolveDepthBuffers[frameContextIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);

	if (mSampleCount == VK_SAMPLE_COUNT_1_BIT){
		VkImageCopy region = {};
		region.extent = { mWidth, mHeight, 1 };
		region.dstSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.srcSubresource.layerCount = 1;
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		vkCmdCopyImage(*commandBuffer,
			mDepthBuffers[frameContextIndex]->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			mResolveDepthBuffers[frameContextIndex]->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}else{
		VkImageResolve region = {};
		region.extent = { mWidth, mHeight, 1 };
		region.dstSubresource.layerCount = 1;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.srcSubresource.layerCount = 1;
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		vkCmdResolveImage(*commandBuffer,
			mDepthBuffers[frameContextIndex]->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			mResolveDepthBuffers[frameContextIndex]->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	mDepthBuffers[frameContextIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, commandBuffer);
	mResolveDepthBuffers[frameContextIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
}
