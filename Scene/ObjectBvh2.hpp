#pragma once

#include <Scene/Object.hpp>

#ifdef GetObject
#undef GetObject
#endif

class ObjectBvh2 {
public:
	struct Primitive {
		AABB mBounds;
		Object* mObject;
	};
	struct Node {
		AABB mBounds;
		// index of the first primitive inside this node
		uint32_t mStartIndex;
		// number of primitives inside this node
		uint32_t mCount;
		uint32_t mRightOffset; // 1st child is at node[index + 1], 2nd child is at node[index + mRightOffset]
		uint32_t mMask;
	};

	inline ObjectBvh2(uint32_t leafSize = 1) : mLeafSize(leafSize) {};
	inline ~ObjectBvh2() {}

	const Node& GetNode(uint32_t index) const { return mNodes[index]; }
	Object* GetObject(uint32_t index) const { return mPrimitives[index].mObject; }

	inline AABB Bounds() { return mNodes.size() ? mNodes[0].mBounds : AABB(); }

	ENGINE_EXPORT void Build(Object** objects, uint32_t objectCount, uint32_t mask = ~0);
	ENGINE_EXPORT void FrustumCheck(const float4 frustum[6], std::vector<Object*>& objects, uint32_t mask);
	ENGINE_EXPORT Object* Intersect(const Ray& ray, float* t, bool any, uint32_t mask);

	ENGINE_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera, Scene* scene);

private:
	std::vector<Node> mNodes;
	std::vector<Primitive> mPrimitives;

	uint32_t mLeafSize;
};