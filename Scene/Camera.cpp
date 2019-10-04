#include <Scene/Camera.hpp>
#include <Shaders/shadercompat.h>

using namespace std;

Camera::Camera(const string& name, ::Device* device, VkFormat renderFormat, VkFormat depthFormat)
	: Object(name), mDevice(device),
	mRenderPass(VK_NULL_HANDLE), mDescriptorSetLayout(VK_NULL_HANDLE), mFrameData(nullptr), mRenderDepthNormals(true),
	mMatricesDirty(true),
	mRenderFormat(renderFormat),
	mDepthFormat(depthFormat),
	mOrthographic(false), mOrthographicSize(0),
	mFieldOfView(radians(70.f)), mPerspectiveBounds(vec4(0.f)),
	mNear(.03f), mFar(500.f),
	mPixelWidth(1600), mPixelHeight(900),
	mSampleCount(VK_SAMPLE_COUNT_1_BIT),
	mRenderPriority(0),
	mView(mat4(1.f)), mProjection(mat4(1.f)), mViewProjection(mat4(1.f)), mInvViewProjection(mat4(1.f)) {

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

	mRenderPass = new ::RenderPass(mName + "RenderPass", mDevice, attachments, subpasses);
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

vec4 Camera::WorldToClip(vec3 worldPos) {
	UpdateMatrices();
	return mViewProjection * vec4(worldPos, 1);
}
vec3 Camera::ClipToWorld(vec3 clipPos) {
	UpdateMatrices();
	vec4 wp = mInvViewProjection * vec4(clipPos, 1);
	return wp / wp.w;
}

void Camera::BeginRenderPass(CommandBuffer* commandBuffer, uint32_t backBufferIndex) {
	UpdateFramebuffer(backBufferIndex);

	mFrameData[backBufferIndex].mColorBuffer->TransitionImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);

	CameraBuffer* buf = (CameraBuffer*)mFrameData[backBufferIndex].mUniformBuffer->MappedData();
	buf->ViewProjection = ViewProjection();
	buf->Viewport = vec4((float)mPixelWidth, (float)mPixelHeight, mNear, mFar);
	buf->Position = WorldPosition();
	buf->Right = WorldRotation() * vec3(1, 0, 0);
	buf->Up = WorldRotation() * vec3(0, 1, 0);
	
	VkClearValue clearValues[]{
		{ .0f, .0f, .0f, 0.f },
		{ .0f, .0f, .0f, 0.f },
		{ 1.f, 0.f },
	};
	commandBuffer->BeginRenderPass(mRenderPass, { mPixelWidth, mPixelHeight }, mFrameData[backBufferIndex].mFramebuffer, clearValues, 3);

	// vulkan clip space has inverted y coordinate, flip the viewport upside-down5
	VkViewport vp{ 0, 0, (float)mPixelWidth, (float)mPixelHeight, 0, 1.f };
	vkCmdSetViewport(*commandBuffer, 0, 1, &vp);

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

	VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	fd.mColorBuffer = new Texture(mName + "ColorBuffer", mDevice, mPixelWidth, mPixelHeight, mRenderFormat, mSampleCount, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	fd.mDepthNormalBuffer = new Texture(mName + "DepthNormalBuffer", mDevice, mPixelWidth, mPixelHeight, VK_FORMAT_R8G8B8A8_UNORM, mSampleCount, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	fd.mDepthBuffer = new Texture(mName + "DepthBuffer", mDevice, mPixelWidth, mPixelHeight, mDepthFormat , mSampleCount, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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

	vec3 up = WorldRotation() * vec3(0.f, 1.f, 0.f);
	vec3 fwd = WorldRotation() * vec3(0.f, 0.f, 1.f);

	mView = lookAt(WorldPosition(), WorldPosition() + fwd, up);

	float aspect = Aspect();

	if (mOrthographic)
		mProjection = ortho(-mOrthographicSize / aspect, mOrthographicSize / aspect, -mOrthographicSize, mOrthographicSize, mNear, mFar);
	else {
		if (mFieldOfView)
			mProjection = perspective(mFieldOfView, aspect, mNear, mFar);
		else {
			vec4 s = mPerspectiveBounds * mNear;
			mProjection = frustum(s.x, s.y, s.z, s.w, mNear, mFar);
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