#pragma once

#include <Content/Material.hpp>
#include <Content/Mesh.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/Renderer.hpp>
#include <Util/Util.hpp>

class ClothRenderer : public Renderer {
public:
	bool mVisible;

	PLUGIN_EXPORT ClothRenderer(const std::string& name, float size = 1.f, uint32_t resolution = 128);
	PLUGIN_EXPORT ~ClothRenderer();

	inline void ExternalForce(const float3& f) { mExternalForce = f; }
	inline float3 ExternalForce() const { return mExternalForce; }
	inline void Drag(float d) { mDrag = d; }
	inline float Drag() const { return mDrag; }
	inline void Stiffness(float d) { mStiffness = d; }
	inline float Stiffness() const { return mStiffness; }

	inline bool Visible() override { return mVisible && EnabledHierarchy(); }
	inline PassType PassMask() override { return (PassType)(PASS_DEPTH | PASS_MAIN); }
	inline uint32_t RenderQueue() override { return 1000; }

	PLUGIN_EXPORT void FixedUpdate(CommandBuffer* commandBuffer) override;

	PLUGIN_EXPORT void PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override;
	PLUGIN_EXPORT void Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override;
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override;

	inline AABB Bounds() override { return AABB(-mSize, mSize) * ObjectToWorld(); }

private:
	uint32_t mRayMask;
	Texture* mMainTexture;
	Texture* mNormalTexture;
	Texture* mMaskTexture;
	uint2 mPinIndex;
	float3 mPinPos;

	float4 mColor;
	float mMetallic;
	float mRoughness;
	float mBumpStrength;
	float3 mEmission;
	float4 mTextureST;

    uint32_t mResolution;
	float3 mExternalForce;
	float mDrag;
	float mStiffness;
	float mSize;

	Buffer* mVertices;
	Material* mMaterial;
};