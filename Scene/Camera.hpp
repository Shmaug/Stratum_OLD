#pragma once
#include <unordered_map>
#include <Content/Mesh.hpp>
#include <Core/Buffer.hpp>
#include <Core/Framebuffer.hpp>
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

	ENGINE_EXPORT Camera(const std::string& name, Window* targetWindow, VkFormat depthFormat = VK_FORMAT_D32_SFLOAT, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_8_BIT, bool renderDepthNormals = true);
	ENGINE_EXPORT Camera(const std::string& name, ::Device* device, VkFormat renderFormat = VK_FORMAT_R8G8B8A8_UNORM, VkFormat depthFormat = VK_FORMAT_D32_SFLOAT, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_8_BIT, bool renderDepthNormals = true);
	ENGINE_EXPORT Camera(const std::string& name, ::Framebuffer* framebuffer);
	ENGINE_EXPORT virtual ~Camera();

	inline ::Device* Device() const { return mDevice; }

	ENGINE_EXPORT virtual void PreRender();
	ENGINE_EXPORT virtual void ResolveWindow(CommandBuffer* commandBuffer);
	ENGINE_EXPORT virtual void Set(CommandBuffer* commandBuffer);

	ENGINE_EXPORT virtual float4 WorldToClip(const float3& worldPos);
	ENGINE_EXPORT virtual float3 ClipToWorld(const float3& clipPos);
	ENGINE_EXPORT virtual Ray ScreenToWorldRay(const float2& uv);

	inline virtual uint32_t RenderPriority() const { return mRenderPriority; }
	inline virtual void RenderPriority(uint32_t x) { mRenderPriority = x; }

	inline Window* TargetWindow() const { return mTargetWindow; }

	inline virtual bool Orthographic() const { return mOrthographic; }
	inline virtual float OrthographicSize() const { return mOrthographicSize; }
	inline virtual float Aspect() const { return mViewport.width / mViewport.height; }
	inline virtual float Near() const { return mNear; }
	inline virtual float Far() const { return mFar; }
	inline virtual float FieldOfView() const { return mFieldOfView; }
	inline virtual float2 PerspectiveSize() const { return mPerspectiveSize; }
	inline virtual float ViewportX() const { return mViewport.x; }
	inline virtual float ViewportY() const { return mViewport.y; }
	inline virtual float ViewportWidth() const { return mViewport.width; }
	inline virtual float ViewportHeight() const { return mViewport.height; }
	inline virtual uint32_t FramebufferWidth()  const { return mFramebuffer->Width();  }
	inline virtual uint32_t FramebufferHeight() const { return mFramebuffer->Height(); }
	inline virtual VkSampleCountFlagBits SampleCount() const { return mFramebuffer->SampleCount(); }

	inline virtual void Orthographic(bool o) { mOrthographic = o; mMatricesDirty = true; }
	inline virtual void OrthographicSize(float s) { mOrthographicSize = s; mMatricesDirty = true; }
	inline virtual void Near(float n) { mNear = n; mMatricesDirty = true; }
	inline virtual void Far(float f) { mFar = f;  mMatricesDirty = true; }
	inline virtual void FieldOfView(float f) { mPerspectiveSize = 0; mFieldOfView = f; mMatricesDirty = true; }
	inline virtual void ViewportX(float x) { mViewport.x = x; }
	inline virtual void ViewportY(float y) { mViewport.y = y; }
	inline virtual void ViewportWidth(float f) { mViewport.width = f; mMatricesDirty = true; }
	inline virtual void ViewportHeight(float f) { mViewport.height = f; mMatricesDirty = true; }
	inline virtual void PerspectiveSize(const float2& p) { mPerspectiveSize = p; mFieldOfView = 0; mMatricesDirty = true; }
	inline virtual void FramebufferWidth (uint32_t w) { mFramebuffer->Width(w);  mMatricesDirty = true; }
	inline virtual void FramebufferHeight(uint32_t h) { mFramebuffer->Height(h); mMatricesDirty = true; }

	inline virtual ::Framebuffer* Framebuffer() const { return mFramebuffer; }
	inline virtual Texture* ColorBuffer() const { return mFramebuffer->ColorBuffer(0); }
	inline virtual Texture* DepthNormalBuffer() const { return mRenderDepthNormals ? mFramebuffer->ColorBuffer(1) : nullptr; }
	inline virtual Texture* DepthBuffer() const { return mFramebuffer->DepthBuffer(); }
	inline virtual Buffer* UniformBuffer() const { return mUniformBuffer; }
	ENGINE_EXPORT virtual ::DescriptorSet* DescriptorSet(VkShaderStageFlags stage);

	inline virtual float4x4 View() { UpdateMatrices(); return mView; }
	inline virtual float4x4 Projection() { UpdateMatrices(); return mProjection; }
	inline virtual float4x4 ViewProjection() { UpdateMatrices(); return mViewProjection; }
	inline virtual float4x4 InverseView() { UpdateMatrices(); return mInvView; }
	inline virtual float4x4 InverseProjection() { UpdateMatrices(); return mInvProjection; }
	inline virtual float4x4 InverseViewProjection() { UpdateMatrices(); return mInvViewProjection; }

	ENGINE_EXPORT bool IntersectFrustum(const AABB& aabb);

private:
	uint32_t mRenderPriority;

	bool mRenderDepthNormals;

	bool mOrthographic;
	float mOrthographicSize;

	float mFieldOfView;
	float mNear;
	float mFar;
	float2 mPerspectiveSize;

	float4x4 mView;
	float4x4 mProjection;
	float4x4 mViewProjection;
	float4x4 mInvView;
	float4x4 mInvProjection;
	float4x4 mInvViewProjection;
	bool mMatricesDirty;

	float4 mFrustum[6];

	VkViewport mViewport;

	Window* mTargetWindow;
	::Device* mDevice;
	::Framebuffer* mFramebuffer;
	bool mDeleteFramebuffer;

	Buffer* mUniformBuffer;
	std::vector<std::unordered_map<VkShaderStageFlags, ::DescriptorSet*>> mDescriptorSets;

	void CreateDescriptorSet();

protected:
	ENGINE_EXPORT virtual void Dirty();
	ENGINE_EXPORT virtual bool UpdateMatrices();
};