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
	ENGINE_EXPORT ~Camera();

	inline ::Device* Device() const { return mDevice; }

	ENGINE_EXPORT void PreRender();
	ENGINE_EXPORT void PostRender(CommandBuffer* commandBuffer, uint32_t backBufferIndex);
	ENGINE_EXPORT void BeginRenderPass(CommandBuffer* commandBuffer, uint32_t backBufferIndex);
	ENGINE_EXPORT void EndRenderPass(CommandBuffer* commandBuffer, uint32_t backBufferIndex);
	inline ::RenderPass* RenderPass() const { return mRenderPass; }

	ENGINE_EXPORT float4 WorldToClip(float3 worldPos);
	ENGINE_EXPORT float3 ClipToWorldRay(float3 clipPos);

	inline uint32_t RenderPriority() const { return mRenderPriority; }
	inline void RenderPriority(uint32_t x) { mRenderPriority = x; }

	inline bool RenderDepthNormals() const { return mRenderDepthNormals; }
	inline void RenderDepthNormals(bool x) { mRenderDepthNormals = x; }

	inline void Viewport(const VkViewport& v) { mViewport = v; }
	inline VkViewport Viewport() const { return mViewport; }

	inline float Aspect() const { return (float)mPixelWidth / (float)mPixelHeight; }
	inline float Near() const { return mNear; }
	inline float Far() const { return mFar; }
	inline float FieldOfView() const { return mFieldOfView; }
	inline float4 PerspectiveBounds() const { return mPerspectiveBounds; }
	inline uint32_t PixelWidth()  const { return mPixelWidth;  }
	inline uint32_t PixelHeight() const { return mPixelHeight; }
	inline VkSampleCountFlagBits SampleCount() const { return mSampleCount; }

	inline void Near(float n) { mNear = n; mMatricesDirty = true; }
	inline void Far (float f) { mFar = f;  mMatricesDirty = true; }
	inline void FieldOfView(float f) { mPerspectiveBounds = float4(0.f); mFieldOfView = f; mMatricesDirty = true; }
	inline void PerspectiveBounds(const float4& p) { mPerspectiveBounds = p; mFieldOfView = 0.f; mMatricesDirty = true; }
	inline void PixelWidth (uint32_t w) { mPixelWidth  = w; DirtyFramebuffers(); mMatricesDirty = true; }
	inline void PixelHeight(uint32_t h) { mPixelHeight = h; DirtyFramebuffers(); mMatricesDirty = true; }
	inline void SampleCount(VkSampleCountFlagBits s) { mSampleCount = s; DirtyFramebuffers(); }

	inline Texture* ColorBuffer(uint32_t backBufferIndex) const { return mFrameData[backBufferIndex].mColorBuffer; }
	inline Texture* DepthNormalBuffer(uint32_t backBufferIndex) const { return mFrameData[backBufferIndex].mDepthNormalBuffer; }
	inline Texture* DepthBuffer(uint32_t backBufferIndex) const { return mFrameData[backBufferIndex].mDepthBuffer; }
	inline Buffer* UniformBuffer(uint32_t backBufferIndex) const { return mFrameData[backBufferIndex].mUniformBuffer; }
	ENGINE_EXPORT ::DescriptorSet* DescriptorSet(uint32_t backBufferIndex, VkShaderStageFlags stage);

	inline float4x4 View() { UpdateMatrices(); return mView; }
	inline float4x4 Projection() { UpdateMatrices(); return mProjection; }
	inline float4x4 ViewProjection() { UpdateMatrices(); return mViewProjection; }
	inline float4x4 InverseViewProjection() { UpdateMatrices(); return mInvViewProjection; }

private:
	uint32_t mRenderPriority;

	VkFormat mRenderFormat;
	VkFormat mDepthFormat;

	bool mOrthographic;
	bool mRenderDepthNormals;
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