#pragma once

#include <Util/Util.hpp>

class TriangleBvh2 {
public:
	struct Primitive {
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

	const std::vector<Node>& Nodes() const { return mNodes; }

	float3 GetVertex(uint32_t index) const { return mVertices[index]; }
	uint3 GetTriangle(uint32_t index) const { return mTriangles[index]; }
	uint32_t TriangleCount() const { return mTriangles.size(); }

	inline AABB Bounds() { return mNodes.size() ? mNodes[0].mBounds : AABB(); }

	ENGINE_EXPORT void Build(const void* vertices, uint32_t baseVertex, uint32_t vertexCount, size_t vertexStride, const void* indices, uint32_t indexCount, VkIndexType indexType);

	ENGINE_EXPORT bool Intersect(const Ray& ray, float* t, bool any);

private:
	std::vector<Node> mNodes;

	std::vector<uint3> mTriangles;
	std::vector<float3> mVertices;

	uint32_t mLeafSize;
};