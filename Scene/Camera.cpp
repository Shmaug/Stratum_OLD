#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Gizmos.hpp>
#include <Shaders/include/shadercompat.h>

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

	uint32_t c = mDevice->MaxFramesInFlight();
	VkDeviceSize bufSize = AlignUp(sizeof(CameraBuffer), mDevice->Limits().minUniformBufferOffsetAlignment);
	mUniformBufferPtrs = new void*[c];
	mUniformBuffer = new Buffer(mName + " Uniforms", mDevice, bufSize * c, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	mDescriptorSets.resize(c);
	for (uint32_t i = 0; i < c; i++) {
		for (auto& s : combos) {
			binding.stageFlags = s;
			
			VkDescriptorSetLayout layout;
			vkCreateDescriptorSetLayout(*mDevice, &dslayoutinfo, nullptr, &layout);
			mDevice->SetObjectName(layout, mName + " DescriptorSetLayout", VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
			
			::DescriptorSet* ds = new ::DescriptorSet(mName + " DescriptorSet", mDevice, layout);
			ds->CreateUniformBufferDescriptor(mUniformBuffer, bufSize * i, bufSize, CAMERA_BUFFER_BINDING);
			ds->FlushWrites();
			mDescriptorSets[i].emplace(s, ds);

		}
		mUniformBufferPtrs[i] = (uint8_t*)mUniformBuffer->MappedData() + bufSize * i;
	}

	mViewport.x = 0;
	mViewport.y = 0;
	mViewport.width = (float)mFramebuffer->Width();
	mViewport.height = (float)mFramebuffer->Height();
	mViewport.minDepth = 0.f;
	mViewport.maxDepth = 1.f;
}

Camera::Camera(const string& name, ::Device* device, VkFormat renderFormat, VkFormat depthFormat, VkSampleCountFlagBits sampleCount)
	: Object(name), mDevice(device), mTargetWindow(nullptr),
	mDeleteFramebuffer(true),
	mOrthographic(false), mOrthographicSize(3),
	mFieldOfView(PI/4),
	mNear(.03f), mFar(500.f),
	mRenderPriority(100), mStereoMode(STEREO_NONE) {

	vector<VkFormat> colorFormats{ renderFormat, VK_FORMAT_R16G16B16A16_SFLOAT };
	mFramebuffer = new ::Framebuffer(name, mDevice, 1600, 900, colorFormats, depthFormat, sampleCount, {}, VK_ATTACHMENT_LOAD_OP_CLEAR);

	mResolveBuffers = new vector<Texture*>[mDevice->MaxFramesInFlight()];
	memset(mResolveBuffers, 0, sizeof(Texture*) * mDevice->MaxFramesInFlight());

	CreateDescriptorSet();
}
Camera::Camera(const string& name, Window* targetWindow, VkFormat depthFormat, VkSampleCountFlagBits sampleCount)
	: Object(name), mDevice(targetWindow->Device()), mTargetWindow(targetWindow),
	mFramebuffer(nullptr),
	mDeleteFramebuffer(true),
	mOrthographic(false), mOrthographicSize(3),
	mFieldOfView(PI/4),
	mNear(.03f), mFar(500.f),
	mRenderPriority(100), mStereoMode(STEREO_NONE) {

	mTargetWindow->mTargetCamera = this;

	VkFormat fmt = targetWindow->Format().format;

	vector<VkFormat> colorFormats{ fmt, VK_FORMAT_R16G16B16A16_SFLOAT };
	mFramebuffer = new ::Framebuffer(name, mDevice, targetWindow->ClientRect().extent.width, targetWindow->ClientRect().extent.height, colorFormats, depthFormat, sampleCount, {}, VK_ATTACHMENT_LOAD_OP_CLEAR);
	
	VkClearValue c = {};
	c.color.float32[0] = 1.f;
	c.color.float32[1] = 1.f;
	c.color.float32[2] = 1.f;
	c.color.float32[3] = 1.f;
	mFramebuffer->ClearValue(1, c);

	mResolveBuffers = new vector<Texture*>[mDevice->MaxFramesInFlight()];
	memset(mResolveBuffers, 0, sizeof(Texture*) * mDevice->MaxFramesInFlight());

	CreateDescriptorSet();
}
Camera::Camera(const string& name, ::Framebuffer* framebuffer)
		: Object(name), mDevice(framebuffer->Device()), mTargetWindow(nullptr),
	mFramebuffer(framebuffer),
	mDeleteFramebuffer(false),
	mOrthographic(false), mOrthographicSize(3),
	mFieldOfView(PI/4),
	mNear(.03f), mFar(500.f),
	mRenderPriority(100), mStereoMode(STEREO_NONE) {

	mResolveBuffers = new vector<Texture*>[mDevice->MaxFramesInFlight()];
	memset(mResolveBuffers, 0, sizeof(Texture*) * mDevice->MaxFramesInFlight());
	CreateDescriptorSet();
}

Camera::~Camera() {
	if (mTargetWindow) mTargetWindow->mTargetCamera = nullptr;
	for (uint32_t i = 0; i < mDevice->MaxFramesInFlight(); i++) {
		for (auto& s : mDescriptorSets[i]) {
			vkDestroyDescriptorSetLayout(*mDevice, s.second->Layout(), nullptr);
			safe_delete(s.second);
		}
		for (uint32_t j = 0; j < mResolveBuffers[i].size(); j++)
			safe_delete(mResolveBuffers[i][j]);
	}
	safe_delete_array(mResolveBuffers);
	safe_delete_array(mUniformBufferPtrs);
	safe_delete(mUniformBuffer);
	if (mDeleteFramebuffer) safe_delete(mFramebuffer);
}

float4 Camera::WorldToClip(const float3& worldPos, StereoEye eye) {
	UpdateTransform();
	return mViewProjection[eye] * float4(worldPos - WorldPosition(), 1);
}
float3 Camera::ClipToWorld(const float3& clipPos, StereoEye eye) {
	UpdateTransform();
	float4 wp = mInvViewProjection[eye] * float4(clipPos, 1);
	wp.xyz /= wp.w;
	return wp.xyz + WorldPosition();
}
Ray Camera::ScreenToWorldRay(const float2& uv, StereoEye eye) {
	UpdateTransform();
	float2 clip = 2.f * uv - 1.f;
	Ray ray;
	if (mOrthographic) {
		clip.x *= Aspect();
		ray.mOrigin = WorldPosition() + WorldRotation() * float3(clip * mOrthographicSize, mNear);
		ray.mDirection = WorldRotation().forward();
	} else {
		float4 p1 = mInvViewProjection[eye] * float4(clip, .1f, 1);
		ray.mDirection = normalize(p1.xyz / p1.w);
		ray.mOrigin = WorldPosition();
	}
	return ray;
}

::DescriptorSet* Camera::DescriptorSet(VkShaderStageFlags stages) {
	return mDescriptorSets[mDevice->FrameContextIndex()].at(stages);
}

void Camera::PreRender() {
	if (mTargetWindow && (FramebufferWidth() != mTargetWindow->BackBufferSize().width || FramebufferHeight() != mTargetWindow->BackBufferSize().height)) {
		mFramebuffer->Width(mTargetWindow->BackBufferSize().width);
		mFramebuffer->Height(mTargetWindow->BackBufferSize().height);
		Dirty();

		mViewport.x = 0;
		mViewport.y = 0;
		mViewport.width = (float)mFramebuffer->Width();
		mViewport.height = (float)mFramebuffer->Height();
	}
}
void Camera::Resolve(CommandBuffer* commandBuffer) {
	if (!mFramebuffer->Width() || !mFramebuffer->Height()) return;

	vector<Texture*>& buffers = mResolveBuffers[mDevice->FrameContextIndex()];
	if (buffers.size() < mFramebuffer->ColorBufferCount()) buffers.resize(mFramebuffer->ColorBufferCount());
	if (mFramebuffer->SampleCount() == VK_SAMPLE_COUNT_1_BIT)
		for (uint32_t i = 0; i < buffers.size(); i++)
			mFramebuffer->ColorBuffer(i)->TransitionImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
	else {
		PROFILER_BEGIN("Resolve/Copy Camera");
		BEGIN_CMD_REGION(commandBuffer, "Resolve/Copy Camera");
		for (uint32_t i = 0; i < buffers.size(); i++) {
			if (buffers[i] && (buffers[i]->Width() != mFramebuffer->Width() || buffers[i]->Height() != mFramebuffer->Height()))
				safe_delete(buffers[i]);
			if (!buffers[i]) {
				buffers[i] = new Texture("Camera Resolve", mDevice, mFramebuffer->Width(), mFramebuffer->Height(), 1, mFramebuffer->ColorBuffer(i)->Format(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
				buffers[i]->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);
			} else
				buffers[i]->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);
			mFramebuffer->ResolveColor(commandBuffer, i, buffers[i]->Image());
			buffers[i]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
		}
		END_CMD_REGION(commandBuffer);
		PROFILER_END;
	}
}
void Camera::PostRender(CommandBuffer* commandBuffer) {
	vector<Texture*>& buffers = mResolveBuffers[mDevice->FrameContextIndex()];
	if (mFramebuffer->SampleCount() == VK_SAMPLE_COUNT_1_BIT)
		for (uint32_t i = 0; i < buffers.size(); i++)
			mFramebuffer->ColorBuffer(i)->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);
}

void Camera::Set(CommandBuffer* commandBuffer) {
	UpdateTransform();
	CameraBuffer& buf = *(CameraBuffer*)mUniformBufferPtrs[commandBuffer->Device()->FrameContextIndex()];
	buf.View[0] = mView[0];
	buf.View[1] = mView[1];
	buf.Projection[0] = mProjection[0];
	buf.Projection[1] = mProjection[1];
	buf.ViewProjection[0] = mViewProjection[0];
	buf.ViewProjection[1] = mViewProjection[1];
	buf.InvProjection[0] = mInvProjection[0];
	buf.InvProjection[1] = mInvProjection[1];
	buf.Viewport = float4(mViewport.width, mViewport.height, mNear, mFar);
	buf.ProjParams = float4(Aspect(), mOrthographicSize, mFieldOfView, mOrthographic ? 1.f : 0.f);
	buf.Position = WorldPosition();
	buf.Right = WorldRotation() * float3(1, 0, 0);
	buf.Up = WorldRotation() * float3(0, 1, 0);
	
	VkRect2D scissor{ { 0, 0 }, { mFramebuffer->Width(), mFramebuffer->Height() } };
	vkCmdSetScissor(*commandBuffer, 0, 1, &scissor);
	vkCmdSetViewport(*commandBuffer, 0, 1, &mViewport);
}

void Camera::SetStereo(CommandBuffer* commandBuffer, ShaderVariant* shader, StereoEye eye) {
	if (!shader) return;
	uint32_t eyec = eye;
	commandBuffer->PushConstant(shader, "StereoEye", &eyec);

	float4 clipst(1, 1, 0, 0);
	VkRect2D scissor{ { 0, 0 }, { mFramebuffer->Width(), mFramebuffer->Height() } };
	if (mStereoMode == STEREO_SBS_HORIZONTAL) {
		clipst = float4(.5f, 1, eye == EYE_LEFT ? -.25f : .25f, 0);
		scissor.extent.width /= 2;
		scissor.offset.x = eye == EYE_LEFT ? 0 : scissor.extent.width;
	} else if (mStereoMode == STEREO_SBS_VERTICAL) {
		clipst = float4(1, .5f, 0, eye == EYE_LEFT ? -.25f : .25f);
		scissor.extent.height /= 2;
		scissor.offset.y = eye == EYE_LEFT ? 0 : scissor.extent.height;
	}

	commandBuffer->PushConstant(shader, "StereoClipTransform", &clipst);

	vkCmdSetScissor(*commandBuffer, 0, 1, &scissor);
}

bool Camera::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;

	switch (mStereoMode) {
	case STEREO_NONE:
		mView[0] = mView[1] = float4x4::Look(0, WorldRotation().forward(), WorldRotation() * float3(0, 1, 0));

		if (mOrthographic)
			mProjection[0] = mProjection[1] = float4x4::Orthographic(mOrthographicSize * Aspect(), mOrthographicSize, mNear, mFar);
		else if (mFieldOfView)
			mProjection[0] = mProjection[1] = float4x4::PerspectiveFov(mFieldOfView, Aspect(), mNear, mFar);
		break;

	case STEREO_SBS_HORIZONTAL:
	case STEREO_SBS_VERTICAL:
		mView[0] = mView[1] = float4x4::Look(0, WorldRotation().forward(), WorldRotation() * float3(0, 1, 0));
		mView[0] = mHeadToEye[0] * mView[0];
		mView[1] = mHeadToEye[1] * mView[1];

		if (mOrthographic)
			mProjection[0] = mProjection[1] = float4x4::Orthographic(mOrthographicSize * Aspect(), mOrthographicSize, mNear, mFar);
		else if (mFieldOfView)
			mProjection[0] = mProjection[1] = float4x4::PerspectiveFov(mFieldOfView, Aspect(), mNear, mFar);
		break;
	}

	mViewProjection[0] = mProjection[0] * mView[0];
	mViewProjection[1] = mProjection[1] * mView[1];

	mInvView[0] = inverse(mView[0]);
	mInvView[1] = inverse(mView[1]);
	mInvProjection[0] = inverse(mProjection[0]);
	mInvProjection[1] = inverse(mProjection[1]);
	mInvViewProjection[0] = inverse(mViewProjection[0]);
	mInvViewProjection[1] = inverse(mViewProjection[1]);
	
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
		float4 c = mInvViewProjection[0] * float4(corners[i], 1);
		corners[i] = c.xyz / c.w + WorldPosition();
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

	return true;
}

void Camera::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	if (camera == this) return;
	Gizmos::DrawWireSphere(WorldPosition(), .01f, 1.f);

	float3 f0 = ClipToWorld(float3(-1, -1, 0));
	float3 f1 = ClipToWorld(float3(-1, 1, 0));
	float3 f2 = ClipToWorld(float3(1, -1, 0));
	float3 f3 = ClipToWorld(float3(1, 1, 0));

	float3 f4 = ClipToWorld(float3(-1, -1, 1));
	float3 f5 = ClipToWorld(float3(-1, 1, 1));
	float3 f6 = ClipToWorld(float3(1, -1, 1));
	float3 f7 = ClipToWorld(float3(1, 1, 1));

	Gizmos::DrawLine(f0, f1, 1);
	Gizmos::DrawLine(f0, f2, 1);
	Gizmos::DrawLine(f3, f1, 1);
	Gizmos::DrawLine(f3, f2, 1);
	
	Gizmos::DrawLine(f4, f5, 1);
	Gizmos::DrawLine(f4, f6, 1);
	Gizmos::DrawLine(f7, f5, 1);
	Gizmos::DrawLine(f7, f6, 1);
	
	Gizmos::DrawLine(f0, f4, 1);
	Gizmos::DrawLine(f1, f5, 1);
	Gizmos::DrawLine(f2, f6, 1);
	Gizmos::DrawLine(f3, f7, 1);

	if (mStereoMode != STEREO_NONE) {
		f0 = ClipToWorld(float3(-1, -1, 0), EYE_RIGHT);
		f1 = ClipToWorld(float3(-1, 1, 0), EYE_RIGHT);
		f2 = ClipToWorld(float3(1, -1, 0), EYE_RIGHT);
		f3 = ClipToWorld(float3(1, 1, 0), EYE_RIGHT);

		f4 = ClipToWorld(float3(-1, -1, 1), EYE_RIGHT);
		f5 = ClipToWorld(float3(-1, 1, 1), EYE_RIGHT);
		f6 = ClipToWorld(float3(1, -1, 1), EYE_RIGHT);
		f7 = ClipToWorld(float3(1, 1, 1), EYE_RIGHT);

		Gizmos::DrawLine(f0, f1, 1);
		Gizmos::DrawLine(f0, f2, 1);
		Gizmos::DrawLine(f3, f1, 1);
		Gizmos::DrawLine(f3, f2, 1);

		Gizmos::DrawLine(f4, f5, 1);
		Gizmos::DrawLine(f4, f6, 1);
		Gizmos::DrawLine(f7, f5, 1);
		Gizmos::DrawLine(f7, f6, 1);

		Gizmos::DrawLine(f0, f4, 1);
		Gizmos::DrawLine(f1, f5, 1);
		Gizmos::DrawLine(f2, f6, 1);
		Gizmos::DrawLine(f3, f7, 1);
	}
}