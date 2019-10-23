#pragma once
#include <unordered_map>
#include <Content/Mesh.hpp>
#include <Content/Texture.hpp>
#include <Core/Buffer.hpp>
#include <Core/DescriptorSet.hpp>
#include <Core/RenderPass.hpp>
#include <Core/Window.hpp>
#include <Scene/Object.hpp>
#include <Util/Util.hpp>

class Camera : public virtual Object {
public:
	enum StereoMode {
		SBSHorizontal, SBSVertical
	};

	ENGINE_EXPORT Camera(const std::string& name, Window* targetWindow, VkFormat depthFormat = VK_FORMAT_D32_SFLOAT);
	ENGINE_EXPORT Camera(const std::string& name, ::Device* device, VkFormat renderFormat = VK_FORMAT_R8G8B8A8_UNORM, VkFormat depthFormat = VK_FORMAT_D32_SFLOAT);
	ENGINE_EXPORT virtual ~Camera();

	inline ::Device* Device() const { return mDevice; }

	ENGINE_EXPORT virtual void PreRender();
	ENGINE_EXPORT virtual void PostRender(CommandBuffer* commandBuffer, uint32_t backBufferIndex);
	ENGINE_EXPORT virtual void BeginRenderPass(CommandBuffer* commandBuffer, uint32_t backBufferIndex);
	ENGINE_EXPORT virtual void EndRenderPass(CommandBuffer* commandBuffer, uint32_t backBufferIndex);
	inline virtual ::RenderPass* RenderPass() const { return mRenderPass; }

	ENGINE_EXPORT virtual float4 WorldToClip(float3 worldPos);
	ENGINE_EXPORT virtual float3 ClipToWorldRay(float3 clipPos);

	inline virtual uint32_t RenderPriority() const { return mRenderPriority; }
	inline virtual void RenderPriority(uint32_t x) { mRenderPriority = x; }

	inline Window* TargetWindow() const { return mTargetWindow; }

	inline virtual float Aspect() const { return (float)mPixelWidth / (float)mPixelHeight; }
	inline virtual float Near() const { return mNear; }
	inline virtual float Far() const { return mFar; }
	inline virtual float FieldOfView() const { return mFieldOfView; }
	inline virtual float4 PerspectiveBounds() const { return mPerspectiveBounds; }
	inline virtual uint32_t PixelWidth()  const { return mPixelWidth;  }
	inline virtual uint32_t PixelHeight() const { return mPixelHeight; }
	inline virtual VkSampleCountFlagBits SampleCount() const { return mSampleCount; }

	inline virtual void Near(float n) { mNear = n; mMatricesDirty = true; }
	inline virtual void Far (float f) { mFar = f;  mMatricesDirty = true; }
	inline virtual void FieldOfView(float f) { mPerspectiveBounds = float4(0.f); mFieldOfView = f; mMatricesDirty = true; }
	inline virtual void PerspectiveBounds(const float4& p) { mPerspectiveBounds = p; mFieldOfView = 0.f; mMatricesDirty = true; }
	inline virtual void PixelWidth (uint32_t w) { mPixelWidth  = w; DirtyFramebuffers(); mMatricesDirty = true; }
	inline virtual void PixelHeight(uint32_t h) { mPixelHeight = h; DirtyFramebuffers(); mMatricesDirty = true; }
	inline virtual void SampleCount(VkSampleCountFlagBits s) { mSampleCount = s; DirtyFramebuffers(); }

	inline virtual Texture* ColorBuffer(uint32_t backBufferIndex) const { return mFrameData[backBufferIndex].mColorBuffer; }
	inline virtual Texture* DepthNormalBuffer(uint32_t backBufferIndex) const { return mFrameData[backBufferIndex].mDepthNormalBuffer; }
	inline virtual Texture* DepthBuffer(uint32_t backBufferIndex) const { return mFrameData[backBufferIndex].mDepthBuffer; }
	inline virtual Buffer* UniformBuffer(uint32_t backBufferIndex) const { return mFrameData[backBufferIndex].mUniformBuffer; }
	ENGINE_EXPORT virtual ::DescriptorSet* DescriptorSet(uint32_t backBufferIndex, VkShaderStageFlags stage);

	inline virtual float4x4 View() { UpdateMatrices(); return mView; }
	inline virtual float4x4 Projection() { UpdateMatrices(); return mProjection; }
	inline virtual float4x4 ViewProjection() { UpdateMatrices(); return mViewProjection; }
	inline virtual float4x4 InverseViewProjection() { UpdateMatrices(); return mInvViewProjection; }

private:
	uint32_t mRenderPriority;

	VkFormat mRenderFormat;
	VkFormat mDepthFormat;

	bool mOrthographic;
	float mOrthographicSize;

	float mFieldOfView;
	float mNear;
	float mFar;
	uint32_t mPixelWidth;
	uint32_t mPixelHeight;
	VkSampleCountFlagBits mSampleCount;
	float4 mPerspectiveBounds;

	float4x4 mView;
	float4x4 mProjection;
	float4x4 mViewProjection;
	float4x4 mInvViewProjection;
	bool mMatricesDirty;

	VkViewport mViewport;

	Window* mTargetWindow;
	::Device* mDevice;

	struct FrameData {
		Texture* mColorBuffer;
		Texture* mDepthNormalBuffer;
		Texture* mDepthBuffer;

		VkFramebuffer mFramebuffer;
		bool mFramebufferDirty;
		Buffer* mUniformBuffer;
		std::unordered_map<VkShaderStageFlags, std::pair<VkDescriptorSetLayout, ::DescriptorSet*>> mDescriptorSets;
	};
	FrameData* mFrameData;
	::RenderPass* mRenderPass;

protected:
	ENGINE_EXPORT virtual void Dirty();
	ENGINE_EXPORT virtual void DirtyFramebuffers();
	ENGINE_EXPORT virtual bool UpdateMatrices();
	ENGINE_EXPORT virtual bool UpdateFramebuffer(uint32_t frameIndex);
};