#pragma once

#include <Content/Material.hpp>
#include <Content/Mesh.hpp>
#include <Core/DescriptorSet.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Util/Util.hpp>

class ClothRenderer : public MeshRenderer {
public:
	ENGINE_EXPORT ClothRenderer(const std::string& name);
	ENGINE_EXPORT ~ClothRenderer();

	inline void Drag(float d) { mDrag = d; }
	inline float Drag() const { return mDrag; }
	inline void Stiffness(float d) { mStiffness = d; }
	inline float Stiffness() const { return mStiffness; }

	ENGINE_EXPORT virtual void Mesh(::Mesh* m) override;
	ENGINE_EXPORT virtual void Mesh(std::shared_ptr<::Mesh> m) override;

	inline virtual void AddSphereCollider(Object* obj, float radius) { mSphereColliders.push_back(std::make_pair(obj, radius)); }
	
	ENGINE_EXPORT virtual void FixedUpdate(CommandBuffer* commandBuffer) override;
	ENGINE_EXPORT virtual void PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override;
	ENGINE_EXPORT virtual void DrawInstanced(CommandBuffer* commandBuffer, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, PassType pass) override;

	ENGINE_EXPORT bool Intersect(const Ray& ray, float* t, bool any) override;

protected:
	Buffer* mVertexBuffer;
	Buffer* mLastVertexBuffer;
	Buffer* mForceBuffer;
	Buffer* mColliderBuffer;
	bool mCopyVertices;

	std::vector<std::pair<Object*, float>> mSphereColliders;

	float mDrag;
	float mStiffness;
};