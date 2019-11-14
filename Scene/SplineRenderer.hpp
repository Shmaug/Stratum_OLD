#pragma once

#include <Content/Shader.hpp>
#include <Core/Buffer.hpp>
#include <Scene/Renderer.hpp>
#include <Scene/Scene.hpp>
#include <Util/Util.hpp>

class SplineRenderer : public Renderer {
public:
	bool mVisible;

	ENGINE_EXPORT SplineRenderer(const std::string& name);
	ENGINE_EXPORT ~SplineRenderer();

    ENGINE_EXPORT float3 Derivative(float t);
    ENGINE_EXPORT float3 Evaluate(float t);

    ENGINE_EXPORT void Points(const std::vector<float3>& pts);

	inline virtual bool Visible() override { return mVisible && mSpline.size() > 2 && (mSpline.size() % 2) == 0 && EnabledHierarchy(); }
	inline virtual uint32_t RenderQueue() override { return 5000; }
	ENGINE_EXPORT virtual void Draw(CommandBuffer* commandBuffer, Camera* camera, ::Material* materialOverride) override;
	
	inline virtual AABB Bounds() override { UpdateTransform(); return mAABB; }

protected:
    std::unordered_map<Device*, std::pair<bool, Buffer*>*> mPointBuffers;

	uint32_t mCurveResolution;
    Shader* mShader;
    std::vector<float3> mSpline;
	AABB mPointAABB;
	AABB mAABB;
};