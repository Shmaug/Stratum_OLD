#pragma once

#include <Util/Util.hpp>

class TriangleBvh2 {
public:
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

	inline TriangleBvh2(uint32_t leafSize = 4) : mLeafSize(leafSize) {};
	inline ~TriangleBvh2() {}

	const Node& GetNode(uint32_t index) const { return mNodes[index]; }
	uint3 GetPrimitive(uint32_t index) const { return mPrimitives[index].mTriangle; }

	inline AABB Bounds() { return mNodes.size() ? mNodes[0].mBounds : AABB(); }

	ENGINE_EXPORT void Build(void* vertices, uint32_t vertexCount, size_t vertexStride, void* indices, uint32_t indexCount, int32_t vertexOffset, VkIndexType indexType);

	ENGINE_EXPORT bool Intersect(const Ray& ray, float* t, bool any);

private:
	std::vector<Node> mNodes;
	std::vector<Primitive> mPrimitives;

	std::vector<float3> mVertices;

	uint32_t mLeafSize;
};