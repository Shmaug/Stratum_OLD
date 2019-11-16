#pragma once

#include <Content/Shader.hpp>
#include <Core/Buffer.hpp>
#include <Scene/Renderer.hpp>
#include <Scene/Scene.hpp>
#include <Util/Util.hpp>

class TerrainRenderer : public Renderer {
public:
	bool mVisible;

	ENGINE_EXPORT TerrainRenderer(const std::string& name);
	ENGINE_EXPORT ~TerrainRenderer();

	inline virtual bool Visible() override { return mVisible && EnabledHierarchy(); }
	inline virtual uint32_t RenderQueue() override { return mShader ? mShader->RenderQueue() : 5000; }
	ENGINE_EXPORT virtual void Draw(CommandBuffer* commandBuffer, Camera* camera, ::Material* materialOverride) override;
	ENGINE_EXPORT virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override;
	
	inline virtual AABB Bounds() override { UpdateTransform(); return mAABB; }

private:
	struct QuadNode {
		TerrainRenderer* mTerrain;

		QuadNode* mParent;
		QuadNode* mChildren[4];

		bool mIsSplit;
		
		uint32_t mSiblingIndex;
		uint32_t mLod;

		float mSize;

		float mVertexResolution;
	};

protected:
	virtual bool UpdateTransform() override;

	QuadNode* mRootNode;

    Shader* mShader;
    AABB mAABB;
};