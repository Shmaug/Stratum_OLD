#pragma once

#include <Scene/Bvh.hpp>

class Bvh2 : public Bvh {
public:

	ENGINE_EXPORT Bvh2(uint32_t leafSize = 4);
	ENGINE_EXPORT ~Bvh2();

	ENGINE_EXPORT virtual void Build(Object** objects, uint32_t count, uint32_t mask = ~0) override;

	inline virtual AABB Bounds() override { return mNodes.size() ? mNodes[0].mBounds : AABB(); };
	ENGINE_EXPORT virtual Object* Intersect(const Ray& ray, float* t = nullptr, bool any = false, uint32_t mask = ~0) override;
	ENGINE_EXPORT virtual void FrustumCheck(Camera* camera, std::vector<Object*>& objects, uint32_t mask = ~0) override;

	ENGINE_EXPORT virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera, Scene* scene) override;

private:
	struct Primitive {
		Object* mObject;
		AABB mBounds;
		float3 mCenter;
	};
	struct Node {
		AABB mBounds;
		uint32_t mStartIndex;
		uint32_t mCount;
		uint32_t mRightOffset;
	};
	uint32_t mLeafSize;
	std::vector<Primitive> mPrimitives;
	std::vector<Node> mNodes;
};