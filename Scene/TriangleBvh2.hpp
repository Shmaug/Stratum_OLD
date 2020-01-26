#pragma once

#include <Util/Util.hpp>

class TriangleBvh2 {
public:
	inline TriangleBvh2(uint32_t leafSize = 4) : mLeafSize(leafSize) {};
	inline ~TriangleBvh2() {}

	inline AABB Bounds() { return mNodes.size() ? mNodes[0].mBounds : AABB(); }

	ENGINE_EXPORT void Build(void* vertices, uint32_t vertexCount, size_t vertexStride, void* indices, uint32_t indexCount, VkIndexType indexType);

	ENGINE_EXPORT bool Intersect(const Ray& ray, float* t, bool any);

private:
	#pragma pack(push)
	#pragma pack(1)
	struct Primitive {
		AABB mBounds;
		uint3 mTriangle;
	};
	struct Node {
		AABB mBounds;
		// index of the first primitive inside this node
		uint32_t mStartIndex;
		// number of primitives inside this node
		uint32_t mCount;
		uint32_t mRightOffset; // 1st child is at node[index + 1], 2nd child is at node[index + mRightOffset]
	};
	#pragma pack(pop)

	std::vector<Node> mNodes;
	std::vector<Primitive> mPrimitives;

	std::vector<float3> mVertices;

	uint32_t mLeafSize;
};