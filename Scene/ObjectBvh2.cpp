#include <Scene/ObjectBvh2.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Renderer.hpp>

using namespace std;

void ObjectBvh2::Build(Object** objects, uint32_t objectCount) {
	mPrimitives.clear();
	mNodes.clear();

	mRendererBounds.mMin = 1e10f;
	mRendererBounds.mMax = -1e10f;

	for (uint32_t i = 0; i < objectCount; i++) {
		AABB aabb(objects[i]->Bounds());
		aabb.mMin -= 1e-2f;
		aabb.mMax += 1e-2f;
		mPrimitives.push_back({ aabb, objects[i] });

		if (dynamic_cast<Renderer*>(objects[i]))
			mRendererBounds.Encapsulate(aabb);
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
		for (uint32_t p = start + 1; p < end; ++p) {
			bb.Encapsulate(mPrimitives[p].mBounds);
			bc.Encapsulate(mPrimitives[p].mBounds.Center());
		}
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

void ObjectBvh2::FrustumCheck(const float4 frustum[6], vector<Object*>& objects, uint32_t mask) {
	if (mNodes.size() == 0) return;

	uint32_t todo[1024];
	int32_t stackptr = 0;

	todo[stackptr] = 0;

	while (stackptr >= 0) {
		int ni = todo[stackptr];
		stackptr--;
		const Node& node(mNodes[ni]);

		if (node.mRightOffset == 0) { // leaf node
			for (uint32_t o = 0; o < node.mCount; ++o)
				if ((mPrimitives[node.mStartIndex + o].mObject->LayerMask() & mask) && mPrimitives[node.mStartIndex + o].mBounds.Intersects(frustum))
					objects.push_back(mPrimitives[node.mStartIndex + o].mObject);
		} else {
			uint32_t n0 = ni + 1;
			uint32_t n1 = ni + node.mRightOffset;
			if (mNodes[n0].mBounds.Intersects(frustum)) todo[++stackptr] = n0;
			if (mNodes[n1].mBounds.Intersects(frustum)) todo[++stackptr] = n1;
		}
	}
}
Object* ObjectBvh2::Intersect(const Ray& ray, float* t, bool any, uint32_t mask) {
	if (mNodes.size() == 0) return nullptr;

	float ht = 1e20f;
	Object* hitObject = nullptr;

	uint32_t todo[128];
	int stackptr = 0;

	todo[stackptr] = 0;

	while (stackptr >= 0) {
		uint32_t ni = todo[stackptr];
		stackptr--;
		const Node& node = mNodes[ni];

		if (node.mRightOffset == 0) {
			for (uint32_t o = 0; o < node.mCount; ++o) {
				if ((mPrimitives[node.mStartIndex + o].mObject->LayerMask() & mask) == 0) continue;

				float ct;
				if (!mPrimitives[node.mStartIndex + o].mObject->Intersect(ray, &ct, any)) continue;

				if (ct < ht) {
					ht = ct;
					hitObject = mPrimitives[node.mStartIndex + o].mObject;
					if (any) {
						if (t) *t = ct;
						return mPrimitives[node.mStartIndex + o].mObject;
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
	return hitObject;
}

void ObjectBvh2::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera, Scene* scene) {
	/*
	if (mNodes.size() == 0) return;

	uint32_t todo[1024];
	int32_t stackptr = 0;

	todo[stackptr] = 0;

	while (stackptr >= 0) {
		int ni = todo[stackptr];
		stackptr--;
		const Node& node(mNodes[ni]);

		if (node.mRightOffset == 0) { // leaf node
			for (uint32_t o = 0; o < node.mCount; ++o){
				AABB box = mPrimitives[node.mStartIndex + o].mBounds;
				Gizmos::DrawWireCube(box.Center(), box.Extents(), quaternion(0, 0, 0, 1), float4(1, 1, 1, .5f));
			}
		} else {
			uint32_t n0 = ni + 1;
			uint32_t n1 = ni + node.mRightOffset;
			todo[++stackptr] = n0;
			todo[++stackptr] = n1;
		}
	}
	*/
}