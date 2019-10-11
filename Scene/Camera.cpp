#include <Scene/Camera.hpp>
#include <Shaders/shadercompat.h>

#include <Util/Profiler.hpp>
 
using namespace std;

Camera::Camera(const string& name, ::Device* device, VkFormat renderFormat, VkFormat depthFormat)
	: Object(name), mDevice(device), mTargetWindow(nullptr),
	mRenderPass(VK_NULL_HANDLE), mDescriptorSetLayout(VK_NULL_HANDLE), mFrameData(nullptr), mRenderDepthNormals(true),
	mMatricesDirty(true),
	mRenderFormat(renderFormat),
	mDepthFormat(depthFormat),
	mOrthographic(false), mOrthographicSize(0),
	mFieldOfView(radians(70.f)), mPerspectiveBounds(float4(0.f)),
	mNear(.03f), mFar(500.f),
	mPixelWidth(1600), mPixelHeight(900),
	mSampleCount(VK_SAMPLE_COUNT_1_BIT),
	mRenderPriority(0),
	mView(float4x4(1.f)), mProjection(float4x4(1.f)), mViewProjection(float4x4(1.f)), mInvViewProjection(float4x4(1.f)) {

	#pragma region create renderpass
	vector<VkAttachmentDescription> attachments(3);
	attachments[0].format = mRenderFormat;
	attachments[0].samples = mSampleCount;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	attachments[1].format = VK_FORMAT_R8G8B8A8_UNORM;
	attachments[1].samples = mSampleCount;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	attachments[2].format = mDepthFormat;
	attachments[2].samples = mSampleCount;
	attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depthNormalAttachmentRef = {};
	depthNormalAttachmentRef.attachment = 1;
	depthNormalAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 2;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachments[]{
		colorAttachmentRef, depthNormalAttachmentRef
	};

	vector<VkSubpassDescription> subpasses(1);
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = 2;
	subpasses[0].pColorAttachments = colorAttachments;
	subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;

	mRenderPass = new ::RenderPass(mName + "RenderPass", this, attachments, subpasses);
	#pragma endregion

	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = CAMERA_BUFFER_BINDING;
	binding.descriptorCount = 1;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutCreateInfo dslayoutinfo = {};
	dslayoutinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslayoutinfo.bindingCount = 1;
	dslayoutinfo.pBindings = &binding;
	vkCreateDescriptorSetLayout(*mDevice, &dslayoutinfo, nullptr, &mDescriptorSetLayout);
	mDevice->SetObjectName(mDescriptorSetLayout, mName + " DescriptorSetLayout");

	mFrameData = new FrameData[mDevice->MaxFramesInFlight()];
	for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++) {
		mFrameData[i].mUniformBuffer = new Buffer(mName + " Uniforms", device, sizeof(CameraBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		mFrameData[i].mUniformBuffer->Map();
		mFrameData[i].mDescriptorSet = new ::DescriptorSet(mName + " DescriptorSet", device->DescriptorPool(), mDescriptorSetLayout);
		mFrameData[i].mDescriptorSet->CreateUniformBufferDescriptor(mFrameData[i].mUniformBuffer, CAMERA_BUFFER_BINDING);
		mFrameData[i].mFramebuffer = VK_NULL_HANDLE;
		mFrameData[i].mFramebufferDirty = true;
		mFrameData[i].mColorBuffer = nullptr;
		mFrameData[i].mDepthNormalBuffer = nullptr;
		mFrameData[i].mDepthBuffer = nullptr;
	}

	mViewport.x = 0;
	mViewport.y = 0;
	mViewport.width = (float)mPixelWidth;
	mViewport.height = (float)mPixelHeight;
	mViewport.minDepth = 0.f;
	mViewport.maxDepth = 1.f;
}
Camera::Camera(const string& name, Window* targetWindow, VkFormat depthFormat)
	: Object(name), mDevice(targetWindow->Device()), mTargetWindow(targetWindow),
	mRenderPass(VK_NULL_HANDLE), mDescriptorSetLayout(VK_NULL_HANDLE), mFrameData(nullptr), mRenderDepthNormals(true),
	mMatricesDirty(true),
	mRenderFormat(targetWindow->Format().format),
	mDepthFormat(depthFormat),
	mOrthographic(false), mOrthographicSize(0),
	mFieldOfView(radians(70.f)), mPerspectiveBounds(float4(0.f)),
	mNear(.03f), mFar(500.f),
	mPixelWidth(1600), mPixelHeight(900),
	mSampleCount(VK_SAMPLE_COUNT_1_BIT),
	mRenderPriority(0),
	mView(float4x4(1.f)), mProjection(float4x4(1.f)), mViewProjection(float4x4(1.f)), mInvViewProjection(float4x4(1.f)) {

	mTargetWindow->mTargetCamera = this;

	#pragma region create renderpass
	vector<VkAttachmentDescription> attachments(3);
	attachments[0].format = mRenderFormat;
	attachments[0].samples = mSampleCount;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	attachments[1].format = VK_FORMAT_R8G8B8A8_UNORM;
	attachments[1].samples = mSampleCount;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	attachments[2].format = mDepthFormat;
	attachments[2].samples = mSampleCount;
	attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depthNormalAttachmentRef = {};
	depthNormalAttachmentRef.attachment = 1;
	depthNormalAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 2;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachments[]{
		colorAttachmentRef, depthNormalAttachmentRef
	};

	vector<VkSubpassDescription> subpasses(1);
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = 2;
	subpasses[0].pColorAttachments = colorAttachments;
	subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;

	mRenderPass = new ::RenderPass(mName + "RenderPass", this, attachments, subpasses);
	#pragma endregion

	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = CAMERA_BUFFER_BINDING;
	binding.descriptorCount = 1;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutCreateInfo dslayoutinfo = {};
	dslayoutinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslayoutinfo.bindingCount = 1;
	dslayoutinfo.pBindings = &binding;
	vkCreateDescriptorSetLayout(*mDevice, &dslayoutinfo, nullptr, &mDescriptorSetLayout);
	mDevice->SetObjectName(mDescriptorSetLayout, mName + " DescriptorSetLayout");

	mFrameData = new FrameData[mDevice->MaxFramesInFlight()];
	for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++) {
		mFrameData[i].mUniformBuffer = new Buffer(mName + " Uniforms", mDevice, sizeof(CameraBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		mFrameData[i].mUniformBuffer->Map();
		mFrameData[i].mDescriptorSet = new ::DescriptorSet(mName + " DescriptorSet", mDevice->DescriptorPool(), mDescriptorSetLayout);
		mFrameData[i].mDescriptorSet->CreateUniformBufferDescriptor(mFrameData[i].mUniformBuffer, CAMERA_BUFFER_BINDING);
		mFrameData[i].mFramebuffer = VK_NULL_HANDLE;
		mFrameData[i].mFramebufferDirty = true;
		mFrameData[i].mColorBuffer = nullptr;
		mFrameData[i].mDepthNormalBuffer = nullptr;
		mFrameData[i].mDepthBuffer = nullptr;
	}

	mViewport.x = 0;
	mViewport.y = 0;
	mViewport.width = (float)mPixelWidth;
	mViewport.height = (float)mPixelHeight;
	mViewport.minDepth = 0.f;
	mViewport.maxDepth = 1.f;
}
Camera::~Camera() {
	vkDestroyDescriptorSetLayout(*mDevice, mDescriptorSetLayout, nullptr);
	delete mRenderPass;
	for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++) {
		safe_delete(mFrameData[i].mDescriptorSet);
		safe_delete(mFrameData[i].mUniformBuffer);
		safe_delete(mFrameData[i].mColorBuffer);
		safe_delete(mFrameData[i].mDepthNormalBuffer);
		safe_delete(mFrameData[i].mDepthBuffer);
		vkDestroyFramebuffer(*mDevice, mFrameData[i].mFramebuffer, nullptr);
	}
	safe_delete(mFrameData);
}

float4 Camera::WorldToClip(float3 worldPos) {
	UpdateMatrices();
	return mViewProjection * float4(worldPos, 1);
}
float3 Camera::ClipToWorld(float3 clipPos) {
	UpdateMatrices();
	float4 wp = mInvViewProjection * float4(clipPos, 1);
	return wp.xyz / wp.w;
}

void Camera::PreRender() {
	if (mTargetWindow && (mPixelWidth != mTargetWindow->BackBufferSize().width || mPixelHeight != mTargetWindow->BackBufferSize().height)) {
		mPixelWidth = mTargetWindow->BackBufferSize().width;
		mPixelHeight = mTargetWindow->BackBufferSize().height;
		DirtyFramebuffers();
		mMatricesDirty = true;

		mViewport.x = 0;
		mViewport.y = 0;
		mViewport.width = (float)mPixelWidth;
		mViewport.height = (float)mPixelHeight;
		mViewport.minDepth = 0.f;
		mViewport.maxDepth = 1.f;
	}
}
void Camera::PostRender(CommandBuffer* commandBuffer, uint32_t backBufferIndex){
	// resolve or copy render target to target window
	if (mTargetWindow) {
		PROFILER_BEGIN("Resolve/Copy RenderTarget");
		BEGIN_CMD_REGION(commandBuffer, "Resolve/Copy");
		ColorBuffer(backBufferIndex)->TransitionImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = mTargetWindow->CurrentBackBuffer();
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(*commandBuffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		if (mSampleCount == VK_SAMPLE_COUNT_1_BIT) {
			VkImageCopy region = {};
			region.extent = { mPixelWidth, mPixelHeight, 1 };
			region.dstSubresource.layerCount = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkCmdCopyImage(*commandBuffer, ColorBuffer(backBufferIndex)->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				mTargetWindow->CurrentBackBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		} else {
			VkImageResolve region = {};
			region.extent = { mPixelWidth, mPixelHeight, 1 };
			region.dstSubresource.layerCount = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkCmdResolveImage(*commandBuffer, ColorBuffer(backBufferIndex)->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				mTargetWindow->CurrentBackBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		swap(barrier.oldLayout, barrier.newLayout);
		swap(barrier.srcAccessMask, barrier.dstAccessMask);
		vkCmdPipelineBarrier(*commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier );
		END_CMD_REGION(commandBuffer);
		PROFILER_END;
	}
}
void Camera::BeginRenderPass(CommandBuffer* commandBuffer, uint32_t backBufferIndex) {
	if (UpdateFramebuffer(backBufferIndex)) {
		mFrameData[backBufferIndex].mColorBuffer->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);
		mFrameData[backBufferIndex].mDepthNormalBuffer->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);
		mFrameData[backBufferIndex].mDepthBuffer->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, commandBuffer);
	}

	CameraBuffer* buf = (CameraBuffer*)mFrameData[backBufferIndex].mUniformBuffer->MappedData();
	buf->ViewProjection = ViewProjection();
	buf->Viewport = float4((float)mPixelWidth, (float)mPixelHeight, mNear, mFar);
	buf->Position = WorldPosition();
	buf->Right = WorldRotation() * float3(1, 0, 0);
	buf->Up = WorldRotation() * float3(0, 1, 0);
	
	VkClearValue clearValues[]{
		{ .0f, .0f, .0f, 0.f },
		{ .0f, .0f, .0f, 0.f },
		{ 1.f, 0.f },
	};
	commandBuffer->BeginRenderPass(mRenderPass, { mPixelWidth, mPixelHeight }, mFrameData[backBufferIndex].mFramebuffer, clearValues, 3);

	vkCmdSetViewport(*commandBuffer, 0, 1, &mViewport);

	VkRect2D scissor{ {0, 0}, { mPixelWidth, mPixelHeight } };
	vkCmdSetScissor(*commandBuffer, 0, 1, &scissor);
}
void Camera::EndRenderPass(CommandBuffer* commandBuffer, uint32_t backBufferIndex) {
	vkCmdEndRenderPass(*commandBuffer);
}

bool Camera::UpdateFramebuffer(uint32_t backBufferIndex) {
	FrameData& fd = mFrameData[backBufferIndex];
	if (!fd.mFramebufferDirty) return false;

	if (fd.mFramebuffer != VK_NULL_HANDLE)
		vkDestroyFramebuffer(*mDevice, fd.mFramebuffer, nullptr);

	safe_delete(mFrameData[backBufferIndex].mColorBuffer);
	safe_delete(mFrameData[backBufferIndex].mDepthNormalBuffer);
	safe_delete(mFrameData[backBufferIndex].mDepthBuffer);

	VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	fd.mColorBuffer = new Texture(mName + "ColorBuffer", mDevice, mPixelWidth, mPixelHeight, 1, mRenderFormat, mSampleCount, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	fd.mDepthNormalBuffer = new Texture(mName + "DepthNormalBuffer", mDevice, mPixelWidth, mPixelHeight, 1, VK_FORMAT_R8G8B8A8_UNORM, mSampleCount, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	fd.mDepthBuffer = new Texture(mName + "DepthBuffer", mDevice, mPixelWidth, mPixelHeight, 1, mDepthFormat , mSampleCount, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkImageView views[]{
		fd.mColorBuffer->View(mDevice),
		fd.mDepthNormalBuffer->View(mDevice),
		fd.mDepthBuffer->View(mDevice)
	};

	VkFramebufferCreateInfo fb = {};
	fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb.attachmentCount = 3;
	fb.pAttachments = views;
	fb.renderPass = *mRenderPass;
	fb.width = mPixelWidth;
	fb.height = mPixelHeight;
	fb.layers = 1;
	vkCreateFramebuffer(*mDevice, &fb, nullptr, &fd.mFramebuffer);
	mDevice->SetObjectName(fd.mFramebuffer, mName + " Framebuffer " + to_string(backBufferIndex));

	mFrameData[backBufferIndex].mFramebufferDirty = false;
	return true;
}

bool Camera::UpdateMatrices() {
	if (!mMatricesDirty) return false;

	float3 up = WorldRotation() * float3(0, 1, 0);
	float3 fwd = WorldRotation() * float3(0, 0, 1);

	mView = float4x4::Look(WorldPosition(), fwd, up);

	float aspect = Aspect();

	if (mOrthographic)
		mProjection = float4x4::Orthographic(-mOrthographicSize / aspect, mOrthographicSize / aspect, -mOrthographicSize, mOrthographicSize, mNear, mFar);
	else {
		if (mFieldOfView)
			mProjection = float4x4::PerspectiveFov(mFieldOfView, aspect, mNear, mFar);
		else {
			float4 s = mPerspectiveBounds * mNear;
			mProjection = float4x4::PerspectiveBounds(s.x, s.y, s.z, s.w, mNear, mFar);
		}
	}

	mProjection[1][1] = -mProjection[1][1];

	mViewProjection = mProjection * mView;
	mInvViewProjection = inverse(mViewProjection);

	mMatricesDirty = false;
	
	return true;
}

void Camera::Dirty() {
	Object::Dirty();
	mMatricesDirty = true;
}
void Camera::DirtyFramebuffers() {
	for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++)
		mFrameData[i].mFramebufferDirty = true;
}