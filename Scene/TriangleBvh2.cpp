#include <Scene/TriangleBvh2.hpp>

#include <Scene/Scene.hpp>

using namespace std;

void TriangleBvh2::Build(void* vertices, uint32_t vertexCount, size_t vertexStride, void* indices, uint32_t indexCount, int32_t vertexOffset, VkIndexType indexType) {
	mPrimitives.clear();
	mNodes.clear();

	mVertices.resize(vertexCount);

	for (uint32_t i = 0; i < vertexCount; i++)
		mVertices[i] = *(float3*)((uint8_t*)vertices + vertexStride * i);

	for (uint32_t i = 0; i < indexCount; i += 3) {
		int3 tri = indexType == VK_INDEX_TYPE_UINT16 ?
			int3(((uint16_t*)indices)[i], ((uint16_t*)indices)[i+1], ((uint16_t*)indices)[i+2]) :
			int3(((uint32_t*)indices)[i], ((uint32_t*)indices)[i+1], ((uint32_t*)indices)[i+2]);
		tri += vertexOffset;
		float3 v0 = *(float3*)((uint8_t*)vertices + vertexStride * tri.x);
		float3 v1 = *(float3*)((uint8_t*)vertices + vertexStride * tri.y);
		float3 v2 = *(float3*)((uint8_t*)vertices + vertexStride * tri.z);
		mPrimitives.push_back({ AABB(min(min(v0, v1), v2), max(max(v0, v1), v2)), tri });
	}

	struct BuildTask {
		uint32_t mParentOffset;
		uint32_t mStart;
		uint32_t mEnd;
	};

	BuildTask todo[1024];
	uint32_t stackptr = 0;
	const uint32_t untouched = 0xffffffff;
	const uint32_t touchedTwice = 0xfffffffd;

	uint32_t nNodes = 0;
	uint32_t nLeafs = 0;

	Node node;

	todo[stackptr].mStart = 0;
	todo[stackptr].mEnd = mPrimitives.size();
	todo[stackptr].mParentOffset = 0xfffffffc;
	stackptr++;

	while (stackptr > 0) {
		// Pop the next item off of the stack
		BuildTask& bnode(todo[--stackptr]);
		uint32_t start = bnode.mStart;
		uint32_t end = bnode.mEnd;
		uint32_t nPrims = end - start;

		nNodes++;
		node.mStartIndex = start;
		node.mCount = nPrims;
		node.mRightOffset = untouched;

		// Calculate the bounding box for this node
		AABB bb(mPrimitives[start].mBounds);
		AABB bc(mPrimitives[start].mBounds.Center(), mPrimitives[start].mBounds.Center());
		for (uint32_t p = start + 1; p < end; ++p)
			bb.Encapsulate(mPrimitives[p].mBounds);
		node.mBounds = bb;

		// If the number of primitives at this point is less than the leaf
		// size, then this will become a leaf. (Signified by rightOffset == 0)
		if (nPrims <= 1) {
			node.mRightOffset = 0;
			nLeafs++;
		}

		mNodes.push_back(node);

		// Child touches parent...
		// Special case: Don't do this for the root.
		if (bnode.mParentOffset != 0xfffffffc) {
			mNodes[bnode.mParentOffset].mRightOffset--;

			// When this is the second touch, this is the right child.
			// The right child sets up the offset for the flat tree.
			if (mNodes[bnode.mParentOffset].mRightOffset == touchedTwice) {
				mNodes[bnode.mParentOffset].mRightOffset = nNodes - 1 - bnode.mParentOffset;
			}
		}

		// If this is a leaf, no need to subdivide.
		if (node.mRightOffset == 0) continue;

		// Set the split dimensions
		uint32_t split_dim = 0;
		float3 ext = bc.Extents();
		if (ext.y > ext.x) {
			split_dim = 1;
			if (ext.z > ext.y) split_dim = 2;
		} else
			if (ext.z > ext.x) split_dim = 2;

		// Split on the center of the longest axis
		float split_coord = .5f * (bc.mMin[split_dim] + bc.mMax[split_dim]);

		// Partition the list of objects on this split
		uint32_t mid = start;
		for (uint32_t i = start; i < end; ++i)
			if (mPrimitives[i].mBounds.Center()[split_dim] < split_coord) {
				swap(mPrimitives[i], mPrimitives[mid]);
				mid++;
			}

		// If we get a bad split, just choose the center...
		if (mid == start || mid == end)
			mid = start + (end - start) / 2;

		todo[stackptr].mStart = mid;
		todo[stackptr].mEnd = end;
		todo[stackptr].mParentOffset = nNodes - 1;
		stackptr++;
		todo[stackptr].mStart = start;
		todo[stackptr].mEnd = mid;
		todo[stackptr].mParentOffset = nNodes - 1;
		stackptr++;
	}
}

bool TriangleBvh2::Intersect(const Ray& ray, float* t, bool any) {
	if (mNodes.size() == 0) return false;

	float hitT = 1e20f;
	int hitPrim = -1;

	float4 bbhits;
	int32_t closer, other;

	struct WorkItem {
		uint32_t mIndex;
		float mTmin;
	};
	WorkItem todo[1024];
	int32_t stackptr = 0;

	todo[stackptr].mIndex = 0;
	todo[stackptr].mTmin = 0;

	while (stackptr >= 0) {
		int ni = todo[stackptr].mIndex;
		float near = todo[stackptr].mTmin;
		stackptr--;
		const Node& node(mNodes[ni]);

		if (near > hitT) continue;

		if (node.mRightOffset == 0) { // leaf node
			for (uint32_t o = 0; o < node.mCount; ++o) {

				uint3 tri = mPrimitives[node.mStartIndex + o].mTriangle;
				float3 isect = ray.Intersect(mVertices[tri.x], mVertices[tri.y], mVertices[tri.z]);

				if (isect.y < 0 || isect.z < 0 || isect.x < near || (isect.y + isect.z) > 1.f) continue;

				if (any) {
					if (t) *t = isect.x;
					return true;
				}
				if (isect.x < hitT) {
					hitT = isect.x;
					hitPrim = node.mStartIndex + o;
				}
			}
		} else {
			bool h0 = ray.Intersect(mNodes[ni + 1].mBounds, bbhits.v2[0]);
			bool h1 = ray.Intersect(mNodes[ni + node.mRightOffset].mBounds, bbhits.v2[1]);
			float t0 = fminf(bbhits.x, bbhits.y);
			float t1 = fminf(bbhits.z, bbhits.w);
			if (t0 < near) t0 = fmaxf(bbhits.x, bbhits.y);
			if (t1 < near) t1 = fmaxf(bbhits.z, bbhits.w);
			if (h0 && t0 >= near && h1 && t1 >= near) {
				closer = ni + 1;
				other = ni + node.mRightOffset;
				if (bbhits[1] < bbhits[0]) {
					swap(bbhits[0], bbhits[1]);
					swap(closer, other);
				}
				todo[++stackptr] = { (uint32_t)other, bbhits[1] };
				todo[++stackptr] = { (uint32_t)closer, bbhits[0] };
			} else if (h0 && t0 >= near) todo[++stackptr] = { (uint32_t)ni + 1, bbhits[0] };
			else if (h1 && t1 >= near) todo[++stackptr] = { (uint32_t)ni + node.mRightOffset, bbhits[1] };
		}
	}

	if (hitPrim >= -1 && t) *t = hitT;
	return hitPrim >= 0;
}