#pragma once

#include <Content/Animation.hpp>
#include <Scene/MeshRenderer.hpp>

class SkinnedMeshRenderer : public MeshRenderer {
public:
	ENGINE_EXPORT SkinnedMeshRenderer(const std::string& name);
	ENGINE_EXPORT ~SkinnedMeshRenderer();

	ENGINE_EXPORT virtual void Rig(const AnimationRig& rig);

	inline virtual void Mesh(::Mesh* mesh) { mMesh = mesh; }
	inline virtual void Mesh(std::shared_ptr<::Mesh> mesh) { mMesh = mesh; }

	ENGINE_EXPORT virtual Bone* GetBone(const std::string& name) const;
	
	ENGINE_EXPORT virtual void PreFrame(CommandBuffer* commandBuffer) override;
	ENGINE_EXPORT virtual void DrawInstanced(CommandBuffer* commandBuffer, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, PassType pass) override;
	
	ENGINE_EXPORT virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override;

protected:
	Buffer* mVertexBuffer;
	Buffer* mInstanceBuffer;

	std::unordered_map<std::string, Bone*> mBoneMap;
	AnimationRig mRig;
};