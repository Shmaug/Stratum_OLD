#pragma once

#include <Content/Animation.hpp>
#include <Scene/MeshRenderer.hpp>

class SkinnedMeshRenderer : public MeshRenderer {
public:
	ENGINE_EXPORT SkinnedMeshRenderer(const std::string& name);
	ENGINE_EXPORT ~SkinnedMeshRenderer();

	inline virtual float ShapeKey(const std::string& name) const { return mShapeKeys.at(name); };
	inline virtual void ShapeKey(const std::string& name, float val) { mShapeKeys[name] = val; };

	ENGINE_EXPORT virtual AnimationRig& Rig() { return mRig; };
	ENGINE_EXPORT virtual void Rig(const AnimationRig& rig);
	ENGINE_EXPORT virtual Bone* GetBone(const std::string& name) const;

	ENGINE_EXPORT virtual void PreFrame(CommandBuffer* commandBuffer) override;
	ENGINE_EXPORT virtual void DrawInstanced(CommandBuffer* commandBuffer, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, PassType pass) override;

	ENGINE_EXPORT bool Intersect(const Ray& ray, float* t, bool any) override;
	ENGINE_EXPORT virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override;

protected:
	Buffer* mVertexBuffer;

	std::unordered_map<std::string, Bone*> mBoneMap;
	AnimationRig mRig;
	std::unordered_map<std::string, float> mShapeKeys;
};