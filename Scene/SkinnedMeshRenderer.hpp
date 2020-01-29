#pragma once

#include <Content/Animation.hpp>
#include <Scene/MeshRenderer.hpp>

class SkinnedMeshRenderer : public MeshRenderer {
public:
	ENGINE_EXPORT SkinnedMeshRenderer(const std::string& name);
	ENGINE_EXPORT ~SkinnedMeshRenderer();

	ENGINE_EXPORT virtual void Rig(const AnimationRig& rig);
	ENGINE_EXPORT virtual Bone* GetBone(const std::string& name) const;

	inline virtual void Vertices(std::vector<StdVertex>& vertices) { mVertices = vertices; }
	inline virtual void Weights(std::vector<VertexWeight>& weights) { mWeights = weights; }
	
	ENGINE_EXPORT virtual void PreFrame(CommandBuffer* commandBuffer) override;
	ENGINE_EXPORT virtual void DrawInstanced(CommandBuffer* commandBuffer, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, PassType pass) override;
	
	ENGINE_EXPORT virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override;

protected:
	std::vector<StdVertex> mVertices;
	std::vector<VertexWeight> mWeights;

	Buffer* mVertexBuffer;

	std::unordered_map<std::string, Bone*> mBoneMap;
	AnimationRig mRig;
};