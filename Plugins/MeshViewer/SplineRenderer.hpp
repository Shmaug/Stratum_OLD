#pragma once

#include <Content/Shader.hpp>
#include <Core/Buffer.hpp>
#include <Scene/Renderer.hpp>
#include <Util/Util.hpp>

class SplineRenderer : public Renderer {
public:
	bool mVisible;

	PLUGIN_EXPORT SplineRenderer(const std::string& name);
	PLUGIN_EXPORT ~SplineRenderer();

    PLUGIN_EXPORT float3 Derivative(float t);
    PLUGIN_EXPORT float3 Evaluate(float t);

    PLUGIN_EXPORT void Points(const std::vector<float3>& pts);

	inline virtual bool Visible() override { return mVisible && mSpline.size() > 3 && EnabledHierarchy(); }
	inline virtual uint32_t RenderQueue() override { return mShader ? mShader->RenderQueue() : 5000; }
	inline virtual void Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {}
	PLUGIN_EXPORT virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override;
	
	inline virtual AABB Bounds() override { UpdateTransform(); return mAABB; }

protected:
	virtual bool UpdateTransform() override;

    std::unordered_map<Device*, std::pair<bool, Buffer*>*> mPointBuffers;

	uint32_t mCurveResolution;
    Shader* mShader;
    std::vector<float3> mSpline;
	AABB mPointAABB;
	AABB mAABB;
};