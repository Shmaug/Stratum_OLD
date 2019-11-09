#include <Scene/Camera.hpp>
#include <Shaders/shadercompat.h>

#include <Util/Profiler.hpp>
 
using namespace std;

void Camera::CreateDescriptorSet() {
	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = CAMERA_BUFFER_BINDING;
	binding.descriptorCount = 1;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	VkDescriptorSetLayoutCreateInfo dslayoutinfo = {};
	dslayoutinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslayoutinfo.bindingCount = 1;
	dslayoutinfo.pBindings = &binding;

	vector<VkShaderStageFlags> combos{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
	};

	mFrameData = new FrameData[mDevice->MaxFramesInFlight()];
	for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++) {
		mFrameData[i].mUniformBuffer = new Buffer(mName + " Uniforms", mDevice, sizeof(CameraBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		mFrameData[i].mUniformBuffer->Map();

		for (auto& s : combos) {
			VkDescriptorSetLayout layout;
			binding.stageFlags = s;
			vkCreateDescriptorSetLayout(*mDevice, &dslayoutinfo, nullptr, &layout);
			mDevice->SetObjectName(layout, mName + " DescriptorSetLayout", VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
			::DescriptorSet* ds = new ::DescriptorSet(mName + " DescriptorSet", mDevice->DescriptorPool(), layout);
			ds->CreateUniformBufferDescriptor(mFrameData[i].mUniformBuffer, CAMERA_BUFFER_BINDING);
			mFrameData[i].mDescriptorSets.emplace(s, make_pair(layout, ds));
		}
	}

	mViewport.x = 0;
	mViewport.y = 0;
	mViewport.width = (float)mFramebuffer->Width();
	mViewport.height = (float)mFramebuffer->Height();
	mViewport.minDepth = 0.f;
	mViewport.maxDepth = 1.f;
}

Camera::Camera(const string& name, ::Device* device, VkFormat renderFormat, VkFormat depthFormat, VkSampleCountFlagBits sampleCount, bool renderDepthNormals)
	: Object(name), mDevice(device), mTargetWindow(nullptr),
	mFrameData(nullptr),
	mMatricesDirty(true),
	mDeleteFramebuffer(true),
	mRenderDepthNormals(renderDepthNormals),
	mOrthographic(false), mOrthographicSize(3),
	mFieldOfView(PI/4), mPerspectiveSize(0),
	mNear(.03f), mFar(500.f),
	mRenderPriority(0),
	mView(float4x4(1.f)), mProjection(float4x4(1.f)), mViewProjection(float4x4(1.f)), mInvViewProjection(float4x4(1.f)) {

	vector<VkFormat> colorFormats{ VK_FORMAT_R8G8B8A8_UNORM };
	if (renderDepthNormals) colorFormats.push_back(VK_FORMAT_R8G8B8A8_UNORM);
	mFramebuffer = new ::Framebuffer(name, mDevice, 1600, 900, colorFormats, depthFormat, sampleCount);

	CreateDescriptorSet();
}
Camera::Camera(const string& name, Window* targetWindow, VkFormat depthFormat, VkSampleCountFlagBits sampleCount, bool renderDepthNormals)
	: Object(name), mDevice(targetWindow->Device()), mTargetWindow(targetWindow),
	mFrameData(nullptr),
	mMatricesDirty(true),
	mFramebuffer(nullptr),
	mDeleteFramebuffer(true),
	mRenderDepthNormals(renderDepthNormals),
	mOrthographic(false), mOrthographicSize(3),
	mFieldOfView(PI/4), mPerspectiveSize(0),
	mNear(.03f), mFar(500.f),
	mRenderPriority(0),
	mView(float4x4(1.f)), mProjection(float4x4(1.f)), mViewProjection(float4x4(1.f)), mInvViewProjection(float4x4(1.f)) {

	mTargetWindow->mTargetCamera = this;

	vector<VkFormat> colorFormats;
	colorFormats.push_back(targetWindow->Format().format);
	if (renderDepthNormals) colorFormats.push_back(VK_FORMAT_R8G8B8A8_UNORM);
	mFramebuffer = new ::Framebuffer(name, mDevice, targetWindow->ClientRect().extent.width, targetWindow->ClientRect().extent.height, colorFormats, depthFormat, sampleCount);

	CreateDescriptorSet();
}
Camera::Camera(const string& name, ::Framebuffer* framebuffer)
		: Object(name), mDevice(framebuffer->Device()), mTargetWindow(nullptr),
	mFrameData(nullptr),
	mMatricesDirty(true),
	mFramebuffer(framebuffer),
	mDeleteFramebuffer(false),
	mRenderDepthNormals(false),
	mOrthographic(false), mOrthographicSize(3),
	mFieldOfView(PI/4), mPerspectiveSize(0),
	mNear(.03f), mFar(500.f),
	mRenderPriority(0),
	mView(float4x4(1.f)), mProjection(float4x4(1.f)), mViewProjection(float4x4(1.f)), mInvViewProjection(float4x4(1.f)) {
	CreateDescriptorSet();
}

Camera::~Camera() {
	if (mTargetWindow) mTargetWindow->mTargetCamera = nullptr;
	for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++) {
		for (auto& s : mFrameData[i].mDescriptorSets) {
			vkDestroyDescriptorSetLayout(*mDevice, s.second.first, nullptr);
			safe_delete(s.second.second);
		}
		safe_delete(mFrameData[i].mUniformBuffer);
	}
	if (mDeleteFramebuffer) safe_delete(mFramebuffer);
	safe_delete_array(mFrameData);
}

float4 Camera::WorldToClip(const float3& worldPos) {
	UpdateMatrices();
	return mViewProjection * float4(worldPos, 1);
}
float3 Camera::ClipToWorld(const float3& clipPos) {
	UpdateMatrices();
	float4 wp = mInvViewProjection * float4(clipPos, 1);
	wp.xyz /= wp.w;
	return wp.xyz;
}
Ray Camera::ScreenToWorldRay(const float2& uv) {
	UpdateMatrices();
	float2 clip = 2.f * uv - 1.f;
	Ray ray;
	if (mOrthographic) {
		clip.x *= Aspect();
		ray.mOrigin = WorldPosition() + WorldRotation() * float3(clip * mOrthographicSize, mNear);
		ray.mDirection = WorldRotation().forward();
	} else {
		float4 p1 = mInvViewProjection * float4(clip, .1f, 1);
		ray.mOrigin = WorldPosition();
		ray.mDirection = normalize(p1.xyz / p1.w - ray.mOrigin);
	}
	return ray;
}

::DescriptorSet* Camera::DescriptorSet(uint32_t backBufferIndex, VkShaderStageFlags stages) {
	return mFrameData[backBufferIndex].mDescriptorSets.at(stages).second;
}

void Camera::PreRender() {
	if (mTargetWindow && (FramebufferWidth() != mTargetWindow->BackBufferSize().width || FramebufferHeight() != mTargetWindow->BackBufferSize().height)) {
		mFramebuffer->Width(mTargetWindow->BackBufferSize().width);
		mFramebuffer->Height(mTargetWindow->BackBufferSize().height);
		mMatricesDirty = true;

		mViewport.x = 0;
		mViewport.y = 0;
		mViewport.width = (float)mFramebuffer->Width();
		mViewport.height = (float)mFramebuffer->Height();
	}
}
void Camera::ResolveWindow(CommandBuffer* commandBuffer, uint32_t backBufferIndex){
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
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(*commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		if (SampleCount() == VK_SAMPLE_COUNT_1_BIT) {
			VkImageCopy region = {};
			region.extent = { FramebufferWidth(), FramebufferHeight(), 1 };
			region.dstSubresource.layerCount = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkCmdCopyImage(*commandBuffer,
				ColorBuffer(backBufferIndex)->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				mTargetWindow->CurrentBackBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		} else {
			VkImageResolve region = {};
			region.extent = { FramebufferWidth(), FramebufferHeight(), 1 };
			region.dstSubresource.layerCount = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkCmdResolveImage(*commandBuffer,
				ColorBuffer(backBufferIndex)->Image(mDevice), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				mTargetWindow->CurrentBackBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		swap(barrier.oldLayout, barrier.newLayout);
		swap(barrier.srcAccessMask, barrier.dstAccessMask);
		vkCmdPipelineBarrier(*commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier );
		END_CMD_REGION(commandBuffer);
		PROFILER_END;
	}
}
void Camera::Set(CommandBuffer* commandBuffer, uint32_t backBufferIndex) {
	CameraBuffer* buf = (CameraBuffer*)mFrameData[backBufferIndex].mUniformBuffer->MappedData();
	buf->View = View();
	buf->Projection = Projection();
	buf->ViewProjection = ViewProjection();
	buf->InvProjection = InverseProjection();
	buf->Viewport = float4(mViewport.width, mViewport.height, mNear, mFar);
	buf->ProjParams = float4(Aspect(), mOrthographicSize, mFieldOfView, mOrthographic ? 1 : 0);
	buf->Position = WorldPosition();
	buf->Right = WorldRotation() * float3(1, 0, 0);
	buf->Up = WorldRotation() * float3(0, 1, 0);
	
	vkCmdSetViewport(*commandBuffer, 0, 1, &mViewport);
	VkRect2D scissor{ {0, 0}, { mFramebuffer->Width(), mFramebuffer->Height() } };
	vkCmdSetScissor(*commandBuffer, 0, 1, &scissor);
	vkCmdSetLineWidth(*commandBuffer, 1.0f);
}

bool Camera::UpdateMatrices() {
	if (!mMatricesDirty) return false;

	float3 up = WorldRotation() * float3(0, 1, 0);
	float3 fwd = WorldRotation() * float3(0, 0, 1);

	mView = float4x4::Look(WorldPosition(), fwd, up);

	if (mOrthographic)
		mProjection = float4x4::Orthographic(mOrthographicSize * Aspect(), mOrthographicSize, mNear, mFar);
	else {
		if (mFieldOfView)
			mProjection = float4x4::PerspectiveFov(mFieldOfView, Aspect(), mNear, mFar);
		else
			mProjection = float4x4::Perspective(mPerspectiveSize.x, mPerspectiveSize.y, mNear, mFar);
	}

	mViewProjection = mProjection * mView;
	mInvView = inverse(mView);
	mInvProjection = inverse(mProjection);
	mInvViewProjection = inverse(mViewProjection);
	
	float3 corners[8] {
		float3(-1,  1, 0),
		float3( 1,  1, 0),
		float3(-1, -1, 0),
		float3( 1, -1, 0),
		
		float3(-1,  1, 1),
		float3( 1,  1, 1),
		float3(-1, -1, 1),
		float3( 1, -1, 1),
	};
	for (uint32_t i = 0; i < 8; i++) {
		float4 c = mInvViewProjection * float4(corners[i], 1);
		corners[i] = c.xyz / c.w;
	}

	mFrustum[0].xyz = normalize(cross(corners[1] - corners[0], corners[2] - corners[0])); // near
	mFrustum[1].xyz = normalize(cross(corners[6] - corners[4], corners[5] - corners[4])); // far
	mFrustum[2].xyz = normalize(cross(corners[5] - corners[1], corners[3] - corners[1])); // right
	mFrustum[3].xyz = normalize(cross(corners[2] - corners[0], corners[4] - corners[0])); // left
	mFrustum[4].xyz = normalize(cross(corners[3] - corners[2], corners[6] - corners[2])); // top
	mFrustum[5].xyz = normalize(cross(corners[4] - corners[0], corners[1] - corners[0])); // bottom

	mFrustum[0].w = dot(mFrustum[0].xyz, corners[0]);
	mFrustum[1].w = dot(mFrustum[1].xyz, corners[4]);
	mFrustum[2].w = dot(mFrustum[2].xyz, corners[1]);
	mFrustum[3].w = dot(mFrustum[3].xyz, corners[0]);
	mFrustum[4].w = dot(mFrustum[4].xyz, corners[2]);
	mFrustum[5].w = dot(mFrustum[5].xyz, corners[0]);

	mMatricesDirty = false;
	return true;
}

void Camera::Dirty() {
	Object::Dirty();
	mMatricesDirty = true;
}
bool Camera::IntersectFrustum(const AABB& aabb) {
	UpdateMatrices();
	float r = length(aabb.mExtents);
	for (uint32_t i = 0; i < 6; i++)
		if (dot(mFrustum[i].xyz, aabb.mCenter - mFrustum[i].xyz * mFrustum[i].w) < -r)
			return false;
	return true;
}