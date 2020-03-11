#include <Scene/TriangleBvh2.hpp>

#include <Scene/Scene.hpp>

using namespace std;

void TriangleBvh2::Build(const void* vertices, uint32_t baseVertex, uint32_t vertexCount, size_t vertexStride, const void* indices, uint32_t indexCount, VkIndexType indexType) {
	mTriangles.clear();
	mNodes.clear();

	mVertices.resize(vertexCount);

	vector<AABB> aabbs;

	for (uint32_t i = 0; i < vertexCount; i++)
		mVertices[i] = *(float3*)((uint8_t*)vertices + vertexStride * (i + baseVertex));

	uint16_t* indices16 = (uint16_t*)indices;
	uint32_t* indices32 = (uint32_t*)indices;

	for (uint32_t i = 0; i < indexCount; i += 3) {
		uint3 tri = indexType == VK_INDEX_TYPE_UINT16 ?
			uint3(indices16[i], indices16[i+1], indices16[i+2]) :
			uint3(indices32[i], indices32[i+1], indices32[i+2]);
		mTriangles.push_back(tri);
		float3 v0 = mVertices[tri.x - baseVertex];
		float3 v1 = mVertices[tri.y - baseVertex];
		float3 v2 = mVertices[tri.z - baseVertex];
		aabbs.push_back(AABB(min(min(v0, v1), v2) - 1e-3f, max(max(v0, v1), v2) + 1e-3f));
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
	todo[stackptr].mEnd = mTriangles.size();
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
		AABB bb(aabbs[start]);
		AABB bc(aabbs[start].Center(), aabbs[start].Center());
		for (uint32_t p = start + 1; p < end; ++p) {
			bb.Encapsulate(aabbs[p]);
			bc.Encapsulate(aabbs[p].Center());
		}
		node.mBounds = bb;

		// If the number of primitives at this point is less than the leaf
		// size, then this will become a leaf. (Signified by rightOffset == 0)
		if (nPrims <= mLeafSize) {
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
		float split_coord = bc.Center()[split_dim];

		// Partition the list of objects on this split
		uint32_t mid = start;
		for (uint32_t i = start; i < end; ++i)
			if (aabbs[i].Center()[split_dim] < split_coord) {
				swap(mTriangles[i], mTriangles[mid]);
				swap(aabbs[i], aabbs[mid]);
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

	float ht = 1.e20f;
	float2 bary = 0;
	int hitIndex = -1;

	uint32_t todo[256];
	int stackptr = 0;

	todo[stackptr] = 0;

	while (stackptr >= 0) {
		uint32_t ni = todo[stackptr];
		stackptr--;
		const Node& node = mNodes[ni];

		if (node.mRightOffset == 0) {
			for (uint32_t o = 0; o < node.mCount; ++o) {
				uint3 tri = mTriangles[node.mStartIndex + o];

				float3 tuv;
				bool h = ray.Intersect(mVertices[tri.x], mVertices[tri.y], mVertices[tri.z], &tuv);

				if (h && tuv.x > 0 && tuv.x < ht) {
					ht = tuv.x;
					bary.x = tuv.y;
					bary.y = tuv.z;
					hitIndex = node.mStartIndex + o;
					if (any) {
						if (t) *t = ht;
						return hitIndex;
					}
				}
			}
		} else {
			uint32_t n0 = ni + 1;
			uint32_t n1 = ni + node.mRightOffset;

			float2 t0;
			float2 t1;
			bool h0 = ray.Intersect(mNodes[n0].mBounds, t0);
			bool h1 = ray.Intersect(mNodes[n1].mBounds, t1);

			if (h0 && t0.y < ht) todo[++stackptr] = n0;
			if (h1 && t1.y < ht) todo[++stackptr] = n1;
		}
	}

	if (t) *t = ht;
	return hitIndex != -1;
}